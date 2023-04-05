//
// Created by 杨辉 on 2023/4/5.
//

#include "include/base.h"
#include "include/video_opengl_impl.h"

extern JavaVM* vm;

VideoOpenGlPlayer::VideoOpenGlPlayer(const jobject& surface) {

    JNIEnv *env ;
    vm->GetEnv((void **)&env, JNI_VERSION_1_4);
    // 获取原始窗口
    window = ANativeWindow_fromSurface(env, surface);
    bindSurface();

}

void VideoOpenGlPlayer::bindSurface() {

    // display.
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if(display == EGL_NO_DISPLAY) {
        LOGD("elgGetDisplay failed.");
        return ;
    }

    if(eglInitialize(display, 0, 0) != EGL_TRUE) {
        LOGE("eglInitialize failed.");
        return ;
    }

    // surface 窗口配置
    EGLConfig config;
    EGLint configNum;
    EGLint configSpec[] = {
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_NONE
    };

    if(eglChooseConfig(display, configSpec, &config, 1, &configNum) != EGL_TRUE){
        LOGD("eglChooseConfig failed.");
        return ;
    }

    //创建surface
    EGLSurface  win_surface = eglCreateWindowSurface(display, config,static_cast<ANativeWindow*>(window), nullptr);
    if(win_surface == EGL_NO_SURFACE) {
        LOGD("eglCreate window surface failed.");
        return ;
    }

    //context 创建关联上下文
    const EGLint ctxAttr[] = {
            EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE
    };

    EGLContext  context = eglCreateContext(display, config, EGL_NO_CONTEXT, ctxAttr);
    if(context == EGL_NO_CONTEXT) {
        LOGD("eglCreate Context failed.");
        return ;
    }

    if(eglMakeCurrent(display, win_surface, win_surface, context) != EGL_TRUE){
        LOGD("eglMakeCurrent failed.");
        return ;
    }
}

