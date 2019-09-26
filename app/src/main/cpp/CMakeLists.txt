# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.4.1)

# external FFMpeg libraries
set(FFMPEG_DIR ${CMAKE_SOURCE_DIR}/external/ffmpeg)

add_library(avformat STATIC IMPORTED)
add_library(avcodec STATIC IMPORTED)
add_library(avutil STATIC IMPORTED)
add_library(swresample STATIC IMPORTED)
add_library(swscale STATIC IMPORTED)

set_target_properties(avformat PROPERTIES IMPORTED_LOCATION
        ${FFMPEG_DIR}/${CMAKE_ANDROID_ARCH_ABI}/lib/libavformat.a)
set_target_properties(avcodec PROPERTIES IMPORTED_LOCATION
        ${FFMPEG_DIR}/${CMAKE_ANDROID_ARCH_ABI}/lib/libavcodec.a)
set_target_properties(avutil PROPERTIES IMPORTED_LOCATION
        ${FFMPEG_DIR}/${CMAKE_ANDROID_ARCH_ABI}/lib/libavutil.a)
set_target_properties(swresample PROPERTIES IMPORTED_LOCATION
        ${FFMPEG_DIR}/${CMAKE_ANDROID_ARCH_ABI}/lib/libswresample.a)
set_target_properties(swscale PROPERTIES IMPORTED_LOCATION
        ${FFMPEG_DIR}/${CMAKE_ANDROID_ARCH_ABI}/lib/libswscale.a)

include_directories(${FFMPEG_DIR}/${CMAKE_ANDROID_ARCH_ABI}/include)

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.

add_library( # Sets the name of the library.
             native-lib

             # Sets the library as a shared library.
             SHARED

             # Provides a relative path to your source file(s).
             demuxing_decoding.c )

# Searches for a specified prebuilt library and stores the path as a
# variable. Because CMake includes system libraries in the search path by
# default, you only need to specify the name of the public NDK library
# you want to add. CMake verifies that the library exists before
# completing its build.

find_library( # Sets the name of the path variable.
              log-lib

              # Specifies the name of the NDK library that
              # you want CMake to locate.
              log )

find_library( # Sets the name of the path variable.
              z-lib

              # Specifies the name of the NDK library that
              # you want CMake to locate.
              z )

# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

target_link_libraries( # Specifies the target library.
                       native-lib

                       avformat
                       avcodec
                       avutil

                       # Links the target library to the log library
                       # included in the NDK.
                       ${log-lib}
                       ${z-lib} )
