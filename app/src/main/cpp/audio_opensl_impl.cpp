//
// Created by 杨辉 on 2022/9/3.
//

#include "include/audio_opensl_impl.h"
#include "include/base.h"


AudioOpenSlPlayer::AudioOpenSlPlayer() {

    //create common sl engine object, i-engine object.
    if(createPlayer() != OK){
        LOGE("create engine failed, abort ~");
        return ;
    }

    LOGI("create audio player successful.");
    player_init_state = true;
}

int AudioOpenSlPlayer::startPlay() {

    if(!player_init_state) {
        return FATAL;
    }

    if(!pcmQueue || !playItf) {
        return FATAL;
    }

    LOGI("start play ...");

    //设置回调函数,播放队列空时回调用
    (*pcmQueue)->RegisterCallback(pcmQueue, pcmCallBack, NULL);

    //设置为播放状态
    (*playItf)->SetPlayState(playItf, SL_PLAYSTATE_PLAYING);

    //启动队列回调
    (*pcmQueue)->Enqueue(pcmQueue, "", 1);

    return OK;
}


AudioOpenSlPlayer::~AudioOpenSlPlayer() {

    LOGI("destroy audio open sl player.");
    player_init_state = false;

    if(player) {
        (*player)->Destroy(player);
        player = nullptr;
        pcmQueue = nullptr;
        playItf = nullptr;
    }

    if(mixOutput) {
        (*mixOutput)->Destroy(mixOutput);
        mixOutput = nullptr;
    }

    if(engineSl) {
        (*engineSl)->Destroy(engineSl);
        engineSl = nullptr;
        engineItf = nullptr;
    }
}

int AudioOpenSlPlayer::createPlayer() {

    SLresult ret;
    ret = slCreateEngine(&engineSl, 0, 0, 0, 0,0);
    if(ret != SL_RESULT_SUCCESS) {
        LOGW("__func__ : %s, create audio sl engine failed.", __func__ );
        return CREATE_SL_ENGINE_FAILED;
    }
    ret = (*engineSl)->Realize(engineSl, SL_BOOLEAN_FALSE);
    if(ret != SL_RESULT_SUCCESS) {
        LOGW("__func__ : %s, realize audio sl engine failed.", __func__ );
        return REALIZE_SL_ENGINE_FAILED;
    }

    ret = (*engineSl)->GetInterface(engineSl, SL_IID_ENGINE, &engineItf);
    if(ret != SL_RESULT_SUCCESS) {
        LOGW("__func__ : %s, getInterface audio sl engine failed.", __func__ );
        return GET_SL_I_ENGINE_FAILED;
    }

    ret = (*engineItf)->CreateOutputMix(engineItf, &mixOutput, 0, nullptr, nullptr);
    if(ret != SL_RESULT_SUCCESS) {
        LOGW("create output mix failed.");
        return CREATE_SL_MIX_FAILED;
    }

    ret = (*mixOutput)->Realize(mixOutput, SL_BOOLEAN_FALSE);  //阻塞式的等待
    if(ret != SL_RESULT_SUCCESS) {
        LOGW("mix realize failed.");
        return REALIZE_SL_MIX_FAILED;
    }

    SLDataLocator_OutputMix outMix = {SL_DATALOCATOR_OUTPUTMIX, mixOutput};
    SLDataSink audioSink = {&outMix, nullptr};

    //配置音频信息
    SLDataLocator_AndroidSimpleBufferQueue queue = { SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2}; //缓冲队列
    SLDataFormat_PCM pcm = {
            SL_DATAFORMAT_PCM,
            2, //声道数
            SL_SAMPLINGRATE_44_1,
            SL_PCMSAMPLEFORMAT_FIXED_16,
            SL_PCMSAMPLEFORMAT_FIXED_16,
            SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
            SL_BYTEORDER_LITTLEENDIAN //字节序，小端
    };


    //输入源
    SLDataSource ds = { &queue, &pcm};

    const SLInterfaceID ids[] = {SL_IID_BUFFERQUEUE};
    const SLboolean  req[] = {SL_BOOLEAN_TRUE};

    ret = (*engineItf)->CreateAudioPlayer(engineItf, &player, &ds, &audioSink, sizeof(ids)/sizeof(SLInterfaceID), ids, req);
    if(ret != SL_RESULT_SUCCESS) {
        LOGW("create audio player failed.");
        return CREATE_SL_PLAYER_FAILED;
    }

    ret = (*player)->Realize(player, SL_BOOLEAN_FALSE); //阻塞式等待实例化成功
    if(ret != SL_RESULT_SUCCESS) {
        LOGW("realize player failed.");
        return REALIZE_SL_PLAYER_FAILED;
    }

    //获取player接口
    ret = (*player)->GetInterface(player, SL_IID_PLAY, &playItf);
    if(ret != SL_RESULT_SUCCESS) {
        LOGW("get SLPlayItf failed.");
        return GET_SL_I_PLAYER_FAILED;
    }

    ret = (*player)->GetInterface(player, SL_IID_BUFFERQUEUE, &pcmQueue);
    if(ret != SL_RESULT_SUCCESS) {
        LOGW("get SL_IID_BUFFERQUEUE failed.");
        return GET_SL_IID_PCM_QUEUE_FAILED;
    }

    return OK;
}


void pcmCallBack(SLAndroidSimpleBufferQueueItf bf, void *context) {

    LOGD("pcm call.");

    static FILE *fp = nullptr;
    static unsigned char *buf = nullptr;

    if(!fp) {
        fp = fopen("/sdcard/ChainHang.pcm", "rb");
    }

    if(!buf) {
        buf = new unsigned char [1024 * 1024 * 10];
    }

    if(!fp) {
        LOGW("open file failed.");
        return ;
    }

    if(feof(fp) == 0) {  // 等于0，表示未读到文件结尾

        int len = fread(buf, 1, 1024, fp);
        if(len > 0) {
            (*bf)->Enqueue(bf, buf, len);
        }
    }
}



