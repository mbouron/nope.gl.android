/*
 * Copyright 2023-2024 Nope Forge
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <jni.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>

#include <libavformat/avformat.h>
#include <libavcodec/jni.h>

#include <nopegl.h>
#include <jni.h>

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved)
{
    av_jni_set_java_vm(vm, NULL);
    ngl_jni_set_java_vm(vm);

    return JNI_VERSION_1_6;
}

static void av_android_log(void *arg, int level, const char *fmt, va_list vl)
{
    static const int levels[] = {
            [AV_LOG_TRACE]   = ANDROID_LOG_VERBOSE,
            [AV_LOG_VERBOSE] = ANDROID_LOG_VERBOSE,
            [AV_LOG_DEBUG]   = ANDROID_LOG_DEBUG,
            [AV_LOG_INFO]    = ANDROID_LOG_INFO,
            [AV_LOG_WARNING] = ANDROID_LOG_WARN,
            [AV_LOG_ERROR]   = ANDROID_LOG_ERROR,
    };
    const int mapped = level >= 0 && level < FF_ARRAY_ELEMS(levels);
    const int android_log_level = mapped ? levels[level] : ANDROID_LOG_VERBOSE;
    __android_log_vprint(android_log_level, "ffmpeg", fmt, vl);
}

static void ngl_android_log(void *arg, int level, const char *filename, int ln, const char *fn, const char *fmt, va_list vl)
{
    static const int levels[] = {
            [NGL_LOG_VERBOSE] = ANDROID_LOG_VERBOSE,
            [NGL_LOG_DEBUG]   = ANDROID_LOG_DEBUG,
            [NGL_LOG_INFO]    = ANDROID_LOG_INFO,
            [NGL_LOG_WARNING] = ANDROID_LOG_WARN,
            [NGL_LOG_ERROR]   = ANDROID_LOG_ERROR,
    };
    const int mapped = level >= 0 && level < FF_ARRAY_ELEMS(levels);
    const int android_log_level = mapped ? levels[level] : ANDROID_LOG_VERBOSE;
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, vl);
    __android_log_print(android_log_level, "ngl", "%s:%d %s: %s", filename, ln, fn, buf);
}

JNIEXPORT void JNICALL
Java_org_nopeforge_nopegl_Context_nativeInit(JNIEnv *env, jclass type, jobject context)
{
    av_log_set_level(AV_LOG_INFO);
    av_log_set_callback(av_android_log);

    ngl_log_set_callback(NULL, ngl_android_log);
    ngl_log_set_min_level(NGL_LOG_INFO);

    ngl_android_set_application_context(context);
    av_jni_set_android_app_ctx((*env)->NewGlobalRef(env, context), NULL);
}

JNIEXPORT jlong JNICALL
Java_org_nopeforge_nopegl_Context_nativeCreateNativeWindow(JNIEnv *env, jclass type, jobject surface)
{
    return (jlong)ANativeWindow_fromSurface(env, surface);
}

JNIEXPORT void JNICALL
Java_org_nopeforge_nopegl_Context_nativeReleaseNativeWindow(JNIEnv *env, jclass type, jlong native_ptr)
{
    if (native_ptr)
        ANativeWindow_release((ANativeWindow *)native_ptr);
}

JNIEXPORT jlong JNICALL
Java_org_nopeforge_nopegl_Context_nativeCreate(JNIEnv *env, jclass type)
{
    struct ngl_ctx *ctx = ngl_create();
    return (jlong)ctx;
}

enum jni_type {
    JNI_TYPE_INT,
    JNI_TYPE_LONG,
    JNI_TYPE_BOOL,
    JNI_TYPE_FLOAT,
    JNI_TYPE_INT_ARRAY,
    JNI_TYPE_FLOAT_ARRAY,
    JNI_TYPE_BYTE_BUFFER,
};

const char *jni_type_str[] = {
        [JNI_TYPE_INT]         = "I",
        [JNI_TYPE_LONG]        = "J",
        [JNI_TYPE_BOOL]        = "Z",
        [JNI_TYPE_FLOAT]       = "F",
        [JNI_TYPE_INT_ARRAY]   = "[I",
        [JNI_TYPE_FLOAT_ARRAY] = "[F",
        [JNI_TYPE_BYTE_BUFFER] = "Ljava/nio/ByteBuffer;",
};

#define OFFSET(x) offsetof(struct ngl_config, x)
static const struct {
    const char *name;
    enum jni_type type;
    int offset;
} config_fields[] = {
        {"backend",       JNI_TYPE_INT,         OFFSET(backend)},
        {"window",        JNI_TYPE_LONG,        OFFSET(window)},
        {"offscreen",     JNI_TYPE_BOOL,        OFFSET(offscreen)},
        {"width",         JNI_TYPE_INT,         OFFSET(width)},
        {"height",        JNI_TYPE_INT,         OFFSET(height)},
        {"samples",       JNI_TYPE_INT,         OFFSET(samples)},
        {"setSurfacePts", JNI_TYPE_BOOL,        OFFSET(set_surface_pts)},
        {"clearColor",    JNI_TYPE_FLOAT_ARRAY, OFFSET(clear_color)},
        {"captureBuffer", JNI_TYPE_BYTE_BUFFER, OFFSET(capture_buffer)},
        {"hud",           JNI_TYPE_BOOL,        OFFSET(hud)},
        {"hudScale",      JNI_TYPE_INT,         OFFSET(hud_scale)},
};

static void config_init(struct ngl_config *config, JNIEnv *env, jobject config_)
{
    jclass cls = (*env)->GetObjectClass(env, config_);
    assert(cls);

    for (int i = 0; i < sizeof(config_fields)/sizeof(*config_fields); i++) {
        const char *name = config_fields[i].name;
        const enum jni_type type = config_fields[i].type;
        const char *sig = jni_type_str[type];
        void *dst = (void *)((uintptr_t)config + config_fields[i].offset);

        jfieldID field_id = (*env)->GetFieldID(env, cls, name, sig);
        if ((*env)->ExceptionCheck(env)) {
            (*env)->ExceptionDescribe(env);
            (*env)->ExceptionClear(env);

            continue;
        }

        switch (type) {
            case JNI_TYPE_INT: {
                const jint value = (*env)->GetIntField(env, config_, field_id);
                memcpy(dst, &value, sizeof(value));
                break;
            }
            case JNI_TYPE_LONG: {
                const jlong value = (*env)->GetLongField(env, config_, field_id);
                memcpy(dst, &value, sizeof(value));
                break;
            }
            case JNI_TYPE_BOOL: {
                const jboolean value = (*env)->GetBooleanField(env, config_, field_id);
                memcpy(dst, &value, sizeof(value));
                break;
            }
            case JNI_TYPE_FLOAT: {
                const jfloat value = (*env)->GetFloatField(env, config_, field_id);
                memcpy(dst, &value, sizeof(value));
                break;
            }
            case JNI_TYPE_INT_ARRAY: {

                jobject array = (*env)->GetObjectField(env, config_, field_id);
                (*env)->GetIntArrayRegion(env, array, 0, 4, dst);
                break;
            }
            case JNI_TYPE_FLOAT_ARRAY: {
                jobject array = (*env)->GetObjectField(env, config_, field_id);
                (*env)->GetFloatArrayRegion(env, array, 0, 4, dst);
                break;
            }
            case JNI_TYPE_BYTE_BUFFER: {
                jobject buffer = (*env)->GetObjectField(env, config_, field_id);
                if (buffer) {
                    uint8_t *capture_buffer = (*env)->GetDirectBufferAddress(env, buffer);
                    memcpy(dst, &capture_buffer, sizeof(capture_buffer));
                }
                break;
            }
            default:
                assert(0);
        }
    }

    __android_log_print(ANDROID_LOG_ERROR, "XXX", "%f %f %f %f", config->clear_color[0],
                        config->clear_color[1], config->clear_color[2], config->clear_color[3]);
    (*env)->DeleteLocalRef(env, cls);
}

JNIEXPORT jint JNICALL
Java_org_nopeforge_nopegl_Context_nativeConfigure(JNIEnv *env, jclass type, jlong native_ptr, jobject config_)
{
    struct ngl_ctx *ctx = (struct ngl_ctx *)native_ptr;
    struct ngl_config config = {
            .swap_interval = -1,
    };
    config_init(&config, env, config_);

    return ngl_configure(ctx, &config);
}

JNIEXPORT jint JNICALL
Java_org_nopeforge_nopegl_Context_nativeLoadScene(JNIEnv *env, jclass type, jlong native_ptr, jstring scene_)
{
    const char *scene_str = (*env)->GetStringUTFChars(env, scene_, 0);
    assert(scene_str);

    struct ngl_scene *scene = ngl_scene_create();
    if (!scene)
        return NGL_ERROR_MEMORY;

    int ret = ngl_scene_init_from_str(scene, scene_str);
    (*env)->ReleaseStringUTFChars(env, scene_, scene_str);

    if (ret < 0) {
        ngl_scene_unrefp(&scene);
        return ret;
    }

    struct ngl_ctx *ctx = (struct ngl_ctx *)native_ptr;
    assert(ctx);

    ret = ngl_set_scene(ctx, scene);
    ngl_scene_unrefp(&scene);

    return ret;
}

JNIEXPORT jint JNICALL
Java_org_nopeforge_nopegl_Context_nativeResize(JNIEnv *env, jclass type, jlong native_ptr,
                                               jint width, jint height)
{
    struct ngl_ctx *ctx = (struct ngl_ctx *)native_ptr;
    return ngl_resize(ctx, width, height);
}

JNIEXPORT jint JNICALL
Java_org_nopeforge_nopegl_Context_nativeDraw(JNIEnv *env, jclass type, jlong native_ptr, jdouble time)
{
    struct ngl_ctx *ctx = (struct ngl_ctx *)native_ptr;
    return ngl_draw(ctx, time);
}

JNIEXPORT jint JNICALL
Java_org_nopeforge_nopegl_Context_nativeSetCaptureBuffer(JNIEnv *env, jobject thiz,
                                                         jlong native_ptr, jobject buffer) {
    struct ngl_ctx *ctx = (struct ngl_ctx *)native_ptr;
    void *capture_buffer = (*env)->GetDirectBufferAddress(env, buffer);
    assert(capture_buffer);
    return ngl_set_capture_buffer(ctx, capture_buffer);
}

JNIEXPORT void JNICALL
Java_org_nopeforge_nopegl_Context_nativeRelease(JNIEnv *env, jclass type, jlong native_ptr)
{
    struct ngl_ctx *ctx = (struct ngl_ctx *)native_ptr;
    ngl_freep(&ctx);
}

JNIEXPORT jint JNICALL
Java_org_nopeforge_nopegl_Context_nativeResetScene(JNIEnv *env, jclass type, jlong native_ptr)
{
    struct ngl_ctx *ctx = (struct ngl_ctx *)native_ptr;
    ngl_set_scene(ctx, NULL);
    ngl_draw(ctx, 0.0);

    return 0;
}
