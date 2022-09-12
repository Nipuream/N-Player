//
// Created by 杨辉 on 2022/9/3.
//

#ifndef N_PLAYER_AUDIO_OPENSL_IMPL_H
#define N_PLAYER_AUDIO_OPENSL_IMPL_H

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include "base.h"

enum NPlayer_Result {
    FATAL = -1,
    OK = 0,
    CREATE_SL_ENGINE_FAILED = 1,
    REALIZE_SL_ENGINE_FAILED = 2,
    GET_SL_I_ENGINE_FAILED = 3,

    CREATE_SL_MIX_FAILED = 4,
    REALIZE_SL_MIX_FAILED = 5,

    CREATE_SL_PLAYER_FAILED = 6,
    REALIZE_SL_PLAYER_FAILED = 7,
    GET_SL_I_PLAYER_FAILED = 8,
    GET_SL_IID_PCM_QUEUE_FAILED = 9,
};

//pcm data callback.
void pcmCallBack(SLAndroidSimpleBufferQueueItf bf, void *context);


class AudioOpenSlPlayer {

public:
    ~AudioOpenSlPlayer();
    static std::unique_ptr<AudioOpenSlPlayer> create_audio_sl_player(){
        return std::unique_ptr<AudioOpenSlPlayer>(new AudioOpenSlPlayer());
    }

    int startPlay();


private:
    AudioOpenSlPlayer();
    int createPlayer();


private:
    SLObjectItf engineSl;
    SLEngineItf engineItf;

    SLObjectItf mixOutput;

    SLObjectItf  player; //通用型的object.
    SLPlayItf playItf; // player接口
    SLAndroidSimpleBufferQueueItf pcmQueue;

private:
    bool player_init_state = false;
};


#endif //N_PLAYER_AUDIO_OPENSL_IMPL_H
