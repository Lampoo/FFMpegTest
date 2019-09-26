#include <jni.h>
#include <string>

extern void native_main(JNIEnv*, jobject);
extern "C" void av_jni_set_java_vm

extern "C" JNIEXPORT void JNICALL
Java_org_lampoo_ffmpeg_MainActivity_main(
        JNIEnv* env,
        jobject self /* this */) {
    native_main(env, self);
}

static JNINativeMethod native_methods[] = {
    {"main", "()V", (void*) native_main },
};


extern "C" JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM* vm, void* unused /* reserved */)
{
    JNIEnv* env = nullptr;

    if (vm->GetEnv((void**)&env, JNI_VERSION_1_4) != JNI_OK)
        return JNI_FALSE;

    return JNI_VERSION_1_4;
}
