/*
 * Copyright (c) 2012 Stefano Sabatini
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * @file
 * Demuxing and decoding example.
 *
 * Show how to use the libavformat and libavcodec API to demux and
 * decode audio and video data.
 * @example demuxing_decoding.c
 */

#include <stdlib.h>

#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
#ifdef ANDROID
#include <libavcodec/jni.h>
#endif

#ifdef ANDROID
#include <jni.h>
#include <android/log.h>
#define TAG "FFMPEG_JNI"

#define PRINT_ERR(...)  __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
//#define PRINT(...)    __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#else
#define PRINT_ERR(...)  fprintf(stderr, __VA_ARGS__)
#define PRINT(...)      fprintf(stdout, __VA_ARGS__)
#endif

#ifndef PRINT
#define PRINT(...)
#endif

static AVFormatContext *fmt_ctx = NULL;
static AVCodecContext *video_dec_ctx = NULL;
static int width, height;
static enum AVPixelFormat pix_fmt;
static AVStream *video_stream = NULL;
static const char *src_filename = NULL;
static const char *video_dst_filename = NULL;
static FILE *video_dst_file = NULL;

static uint8_t *video_dst_data[4] = {NULL};
static int      video_dst_linesize[4];
static int video_dst_bufsize;

static int video_stream_idx = -1;
static AVFrame *frame = NULL;
//static AVPacket pkt;
static int video_frame_count = 0;

/* Enable or disable frame reference counting. You are not supposed to support
 * both paths in your application but pick the one most appropriate to your
 * needs. Look for the use of refcount in this example to see what are the
 * differences of API usage between them. */
static int refcount = 0;

#if 0
static int decode_packet(int *got_frame, int cached)
{
    int ret = 0;
    int decoded = pkt.size;

    *got_frame = 0;

    if (pkt.stream_index == video_stream_idx) {
        /* decode video frame */
        ret = avcodec_decode_video2(video_dec_ctx, frame, got_frame, &pkt);
        if (ret < 0) {
            PRINT_ERR("Error decoding video frame (%s)\n", av_err2str(ret));
            return ret;
        }

        if (*got_frame) {

            if (frame->width != width || frame->height != height ||
                frame->format != pix_fmt) {
                /* To handle this change, one could call av_image_alloc again and
                 * decode the following frames into another rawvideo file. */
                PRINT_ERR("Error: Width, height and pixel format have to be "
                        "constant in a rawvideo file, but the width, height or "
                        "pixel format of the input video changed:\n"
                        "old: width = %d, height = %d, format = %s\n"
                        "new: width = %d, height = %d, format = %s\n",
                        width, height, av_get_pix_fmt_name(pix_fmt),
                        frame->width, frame->height,
                        av_get_pix_fmt_name(frame->format));
                return -1;
            }

            PRINT("video_frame%s n:%d coded_n:%d\n",
                   cached ? "(cached)" : "",
                   video_frame_count++, frame->coded_picture_number);

            /* copy decoded frame to destination buffer:
             * this is required since rawvideo expects non aligned data */
            av_image_copy(video_dst_data, video_dst_linesize,
                          (const uint8_t **)(frame->data), frame->linesize,
                          pix_fmt, width, height);

            /* write to rawvideo file */
            fwrite(video_dst_data[0], 1, video_dst_bufsize, video_dst_file);
        }
    }

    /* If we use frame reference counting, we own the data and need
     * to de-reference it when we don't use it anymore */
    if (*got_frame && refcount)
        av_frame_unref(frame);

    return decoded;
}
#endif

static int open_codec_context(int *stream_idx,
                              AVCodecContext **dec_ctx, AVFormatContext *fmt_ctx, enum AVMediaType type)
{
    int ret, stream_index;
    AVStream *st;
    AVCodec *dec = NULL;
    AVDictionary *opts = NULL;

    ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
    if (ret < 0) {
        PRINT_ERR("Could not find %s stream in input file '%s'\n",
                av_get_media_type_string(type), src_filename);
        return ret;
    } else {
        stream_index = ret;
        st = fmt_ctx->streams[stream_index];

#ifdef ANDROID
        switch (st->codecpar->codec_id) {
        case AV_CODEC_ID_H264:
            dec = avcodec_find_decoder_by_name("h264_mediacodec");
            break;
        default:
            dec = avcodec_find_decoder(st->codecpar->codec_id);
            break;
        }
#else
        /* find decoder for the stream */
        dec = avcodec_find_decoder(st->codecpar->codec_id);
#endif
        if (!dec) {
            PRINT_ERR("Failed to find %s codec\n",
                    av_get_media_type_string(type));
            return AVERROR(EINVAL);
        }

        /* Allocate a codec context for the decoder */
        *dec_ctx = avcodec_alloc_context3(dec);
        if (!*dec_ctx) {
            PRINT_ERR("Failed to allocate the %s codec context\n",
                    av_get_media_type_string(type));
            return AVERROR(ENOMEM);
        }

        /* Copy codec parameters from input stream to output codec context */
        if ((ret = avcodec_parameters_to_context(*dec_ctx, st->codecpar)) < 0) {
            PRINT_ERR("Failed to copy %s codec parameters to decoder context\n",
                    av_get_media_type_string(type));
            return ret;
        }

        /* Init the decoders, with or without reference counting */
        //av_dict_set(&opts, "refcounted_frames", refcount ? "1" : "0", 0);
        av_dict_set(&opts, "threads", "1", 0);
        ret = avcodec_open2(*dec_ctx, dec, &opts);
        av_dict_free(&opts);
        if (ret < 0) {
            PRINT_ERR("Failed to open %s codec\n",
                    av_get_media_type_string(type));
            return ret;
        }
        *stream_idx = stream_index;
    }

    return 0;
}

static int get_format_from_sample_fmt(const char **fmt,
                                      enum AVSampleFormat sample_fmt)
{
    int i;
    struct sample_fmt_entry {
        enum AVSampleFormat sample_fmt; const char *fmt_be, *fmt_le;
    } sample_fmt_entries[] = {
        { AV_SAMPLE_FMT_U8,  "u8",    "u8"    },
        { AV_SAMPLE_FMT_S16, "s16be", "s16le" },
        { AV_SAMPLE_FMT_S32, "s32be", "s32le" },
        { AV_SAMPLE_FMT_FLT, "f32be", "f32le" },
        { AV_SAMPLE_FMT_DBL, "f64be", "f64le" },
    };
    *fmt = NULL;

    for (i = 0; i < FF_ARRAY_ELEMS(sample_fmt_entries); i++) {
        struct sample_fmt_entry *entry = &sample_fmt_entries[i];
        if (sample_fmt == entry->sample_fmt) {
            *fmt = AV_NE(entry->fmt_be, entry->fmt_le);
            return 0;
        }
    }

    PRINT("sample format %s is not supported as output format\n",
            av_get_sample_fmt_name(sample_fmt));
    return -1;
}

#ifdef ANDROID
static int c_main(int argc, char **argv)
#else
int main (int argc, char **argv)
#endif
{
    int ret = 0, got_frame;

#if 0
    if (argc != 4 && argc != 5) {
        PRINT(  "usage: %s [-refcount] input_file video_output_file audio_output_file\n"
                "API example program to show how to read frames from an input file.\n"
                "This program reads frames from a file, decodes them, and writes decoded\n"
                "video frames to a rawvideo file named video_output_file, and decoded\n"
                "audio frames to a rawaudio file named audio_output_file.\n\n"
                "If the -refcount option is specified, the program use the\n"
                "reference counting frame system which allows keeping a copy of\n"
                "the data for longer than one decode call.\n"
                "\n", argv[0]);
        exit(1);
    }
    if (argc == 5 && !strcmp(argv[1], "-refcount")) {
        refcount = 1;
        argv++;
    }
#endif
    if (argc < 2)
        exit(1);

    src_filename = argv[1];
    video_dst_filename = argc > 3 ? argv[2] : "/dev/null";

    /* open input file, and allocate format context */
    if (avformat_open_input(&fmt_ctx, src_filename, NULL, NULL) < 0) {
        PRINT_ERR("Could not open source file %s\n", src_filename);
        exit(1);
    }

    /* retrieve stream information */
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        PRINT_ERR("Could not find stream information\n");
        exit(1);
    }

    if (open_codec_context(&video_stream_idx, &video_dec_ctx, fmt_ctx, AVMEDIA_TYPE_VIDEO) >= 0) {
        video_stream = fmt_ctx->streams[video_stream_idx];

        video_dst_file = fopen(video_dst_filename, "wb");
        if (!video_dst_file) {
            PRINT_ERR("Could not open destination file %s\n", video_dst_filename);
            ret = 1;
            goto end;
        }

        /* allocate image where the decoded image will be put */
        width = video_dec_ctx->width;
        height = video_dec_ctx->height;
        pix_fmt = video_dec_ctx->pix_fmt;
        ret = av_image_alloc(video_dst_data, video_dst_linesize,
                             width, height, pix_fmt, 1);
        if (ret < 0) {
            PRINT_ERR("Could not allocate raw video buffer\n");
            goto end;
        }
        video_dst_bufsize = ret;
    }

    /* dump input information to stderr */
    av_dump_format(fmt_ctx, 0, src_filename, 0);

    if (!video_stream) {
        PRINT_ERR("Could not find audio or video stream in the input, aborting\n");
        ret = 1;
        goto end;
    }

    frame = av_frame_alloc();
    if (!frame) {
        PRINT_ERR("Could not allocate frame\n");
        ret = AVERROR(ENOMEM);
        goto end;
    }

    if (video_stream)
        PRINT_ERR("Demuxing video from file '%s' into '%s'\n", src_filename, video_dst_filename);

    /* read frames from the file */
    AVPacket packet;
    while (av_read_frame(fmt_ctx, &packet) >= 0) {
        if (packet.stream_index == video_stream_idx) {
            ret = avcodec_send_packet(video_dec_ctx, &packet);
            if (ret < 0) {
                PRINT_ERR("avcodec_send_packet failed %d", ret);
                break;
            }

            while (avcodec_receive_frame(video_dec_ctx, frame) == 0) {
                PRINT("decoded frames");
                av_frame_unref(frame);
            }
        }
        av_packet_unref(&packet);
    }

#if 0
    /* flush cached frames */
    pkt.data = NULL;
    pkt.size = 0;
    do {
        decode_packet(&got_frame, 1);
    } while (got_frame);
#endif

    PRINT("Demuxing succeeded.\n");

    if (video_stream) {
        PRINT("Play the output video file with the command:\n"
               "ffplay -f rawvideo -pix_fmt %s -video_size %dx%d %s\n",
               av_get_pix_fmt_name(pix_fmt), width, height,
               video_dst_filename);
    }

end:
    avcodec_free_context(&video_dec_ctx);
    avformat_close_input(&fmt_ctx);
    if (video_dst_file)
        fclose(video_dst_file);
    av_frame_free(&frame);
    av_free(video_dst_data[0]);

    return ret < 0;
}

#ifdef ANDROID
#include <jni.h>

void
native_main(JNIEnv* env, jobject object, jstring path)
{
    const char *c_path = (*env)->GetStringUTFChars(env, path, NULL);
    char* argv[5];
    int argc;

    argc = 2;
    argv[0] = "main";
    argv[1] = c_path;
    argv[2] = NULL;

    c_main(argc, argv);

    (*env)->ReleaseStringUTFChars(env, path, c_path);
}

static JNINativeMethod native_methods[] = {
    {"native_main", "(Ljava/lang/String;)V", (void *) native_main },
};

static jint registerNativeMethods(JNIEnv* env, const char* className,
    JNINativeMethod* methods, int number)
{
    jclass clazz;

    clazz = (*env)->FindClass(env, className);
    if (clazz == NULL)
        return JNI_FALSE;

    if ((*env)->RegisterNatives(env, clazz, methods, number) < 0)
        return JNI_FALSE;

    return JNI_TRUE;
}

static void av_log_handler(void* ptr, int level, const char* fmt, va_list vl) {
    int android_level;

    if (level > av_log_get_level())
        return;

    if (level > AV_LOG_ERROR)
        android_level = ANDROID_LOG_FATAL;
    else if (level > AV_LOG_WARNING)
        android_level = ANDROID_LOG_ERROR;
    else if (level > AV_LOG_INFO)
        android_level = ANDROID_LOG_WARN;
    else if (level > AV_LOG_DEBUG)
        android_level = ANDROID_LOG_INFO;
    else if (level > AV_LOG_TRACE)
        android_level = ANDROID_LOG_DEBUG;
    else
        android_level = ANDROID_LOG_VERBOSE;

    __android_log_vprint(android_level, "FFMPEG", fmt, vl);
}

jint JNI_OnLoad(JavaVM* vm, void* unused /* reserved */)
{
    JNIEnv* env = NULL;

    if ((*vm)->GetEnv(vm, (void**) &env, JNI_VERSION_1_6) != JNI_OK) {
        return -1;
    }

    av_log_set_level(AV_LOG_INFO);
    av_log_set_callback(av_log_handler);
    av_jni_set_java_vm(vm, NULL);

    /* register all formats and codecs */
    av_register_all();

    registerNativeMethods(env, "org/lampoo/ffmpeg/MainActivity", native_methods, sizeof(native_methods) / sizeof(native_methods[0]));

    return JNI_VERSION_1_6;
}

#endif // ANDROID
