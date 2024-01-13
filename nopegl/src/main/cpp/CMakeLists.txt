# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html.
# For more examples on how to use CMake, see https://github.com/android/ndk-samples.

if (NOT DEFINED ENV{NGL_ANDROID_ENV})
    message(FATAL_ERROR "NGL_ANDROID_ENV must set")
endif()

set(NGL_ANDROID_ENV $ENV{NGL_ANDROID_ENV}/${ANDROID_ABI})
set(NGL_LIB_PATH ${NGL_ANDROID_ENV}/lib)
set(NGL_HEADER_PATH ${NGL_ANDROID_ENV}/include)

# Sets the minimum CMake version required for this project.
cmake_minimum_required(VERSION 3.22.1)

# Declares the project name. The project name can be accessed via ${ PROJECT_NAME},
# Since this is the top level CMakeLists.txt, the project name is also accessible
# with ${CMAKE_PROJECT_NAME} (both CMake variables are in-sync within the top level
# build script scope).
project("nopegl_native")

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.
#
# In this top level CMakeLists.txt, ${CMAKE_PROJECT_NAME} is used to define
# the target library name; in the sub-module's CMakeLists.txt, ${PROJECT_NAME}
# is preferred for the same purpose.
#
# In order to load a library into your app from Java/Kotlin, you must call
# System.loadLibrary() and pass the name of the library defined here;
# for GameActivity/NativeActivity derived applications, the same library name must be
# used in the AndroidManifest.xml file.
add_library(${CMAKE_PROJECT_NAME} SHARED
        # List C/C++ source files with relative paths to this CMakeLists.txt.
        nopegl.c)

include_directories(${NGL_HEADER_PATH})

set(NGL_LIBS
        nopegl
        nopemd
        avfilter
        swresample
        swscale
        avformat
        avcodec
        avutil
        avdevice
        png16
        freetype
        fribidi
        harfbuzz
        )

foreach(LIB ${NGL_LIBS})
    add_library(${LIB} SHARED IMPORTED)
    set_target_properties(${LIB} PROPERTIES IMPORTED_LOCATION ${NGL_LIB_PATH}/lib${LIB}.so)
endforeach()

# Specifies libraries CMake should link to your target library. You
# can link libraries from various origins, such as libraries defined in this
# build script, prebuilt third-party libraries, or Android system libraries.
target_link_libraries(${CMAKE_PROJECT_NAME}
        # List libraries link to the target library
        log
        android
        EGL
        GLESv2
        vulkan
        m
        c++_shared
        ${NGL_LIBS}
        )
