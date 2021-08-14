//
// Created by Administrator on 2021/8/14 0014.
//

#ifndef N_PLAYER_BASE_H
#define N_PLAYER_BASE_H


#include <android/log.h>

#define TAG "N-Player"


#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG,TAG,__VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,TAG,__VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,TAG,__VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,TAG,__VA_ARGS__)
#define LOGF(...) __android_log_print(ANDROID_LOG_FATAL,TAG,__VA_ARGS__)
#endif //N_PLAYER_BASE_H
