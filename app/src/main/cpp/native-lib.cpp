#include <jni.h>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavcodec/jni.h>
}

#include "include/base.h"
#include <android/native_window.h>
#include <android/native_window_jni.h>

#include "include/audio_opensl_impl.h"
#include <unistd.h>


static inline double r2d(AVRational r){
    return r.num == 0 || r.den == 0 ? 0.: (double)r.num/(double)r.den;
}

long long GetNowMs(){

    struct timeval tv;
    gettimeofday(&tv, NULL);

    int sec = tv.tv_sec % 360000;
    long long t = sec * 1000 + tv.tv_usec / 1000;
    return t;
}

extern "C" JNIEXPORT
jint JNI_OnLoad(JavaVM *vm, void *unused) {

    LOGI("jni onload ....");
    av_jni_set_java_vm(vm, 0);
    return JNI_VERSION_1_4;
}


std::unique_ptr<AudioOpenSlPlayer> sl_audio_player;

extern "C" JNIEXPORT jstring JNICALL
Java_com_nipuream_n_1player_MainActivity_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    hello += avcodec_configuration();
//    testAudio();

    sl_audio_player = AudioOpenSlPlayer::create_audio_sl_player();
    if(sl_audio_player) {
        int ret = sl_audio_player->startPlay();
        LOGI("start play ret : %d",  ret);
    }


    return env->NewStringUTF(hello.c_str());
}


extern "C"
JNIEXPORT void JNICALL
Java_com_nipuream_n_1player_MainActivity_readByteBuffer(JNIEnv *env, jobject thiz, jobject buf) {

    jbyte* jbuf = (jbyte*)env->GetDirectBufferAddress(buf);
    jlong capcity = (jlong)env->GetDirectBufferCapacity(buf);

    if(jbuf != NULL){
        LOGI("buf : %s, capcity : %d", jbuf, capcity);
    }
}


extern "C"
JNIEXPORT void JNICALL
Java_com_nipuream_n_1player_NPlayer_open(JNIEnv *env, jobject thiz, jstring url, jobject surface) {

    const char* path = env->GetStringUTFChars(url, JNI_FALSE);

    //初始化解封装
    av_register_all();
    //初始化网络
    avformat_network_init();
    avcodec_register_all();


    //打开文件
    AVFormatContext *ic = NULL;
    int ret = avformat_open_input(&ic, path, 0, 0);

    if(ret != 0){
        LOGW("avformat_open_input failed !: %s", av_err2str(ret));
        return ;
    }

    //获取流信息,如果文件没有头部信息，可以通过这个方法读取一部分流信息，获取文件信息
    ret = avformat_find_stream_info(ic, 0);
    if(ret != 0){
        LOGW("avformat_find_stream_info failed!");
    }

    LOGI("duration = %lld, nb_streams = %d", ic->duration, ic->nb_streams);


    //视频数据
    int width,height, fps, videoStream, codec_id, pix_format;
    //音频数据
    int audioStream, simple_rate, channels, audio_format;

    //通过遍历的方式 获取音视频数据
    for(int i = 0; i < ic->nb_streams; i++){
        AVStream *stream = ic->streams[i];
        if(stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO){
            videoStream = i;
            fps = r2d(stream->avg_frame_rate);
            width = stream->codecpar->width;
            height = stream->codecpar->height;
            codec_id = stream->codecpar->codec_id;
            pix_format = stream->codecpar->format;
            LOGI("video stream. index : %d, width : %d, height :%d, codec_id : %d, pix_format : %d", i, width, height, codec_id);
        } else if(stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO){
            audioStream = i;
            simple_rate = stream->codecpar->sample_rate;
            channels = stream->codecpar->channels;
            audio_format = stream->codecpar->format;
            LOGI("audio stream. index : %d, simple_rate : %d, channels : %d, audio_format : %d", i, simple_rate, channels, audio_format);
        }
    }

    //通过 av_find_best_stream 获取音视频数据
    audioStream = av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    LOGI("find audioStream index : %d", audioStream);


    //视频解码器初始化
//    AVCodec *vc = avcodec_find_decoder(ic->streams[videoStream]->codecpar->codec_id); //软解码
    AVCodec *vc = avcodec_find_decoder_by_name("h264_mediacodec");
    if(!vc){
        LOGW("video avcodec find failed!");
        return ;
    }

    AVCodecContext *vc_context = avcodec_alloc_context3(vc);
    avcodec_parameters_to_context(vc_context, ic->streams[videoStream]->codecpar);
    vc_context->thread_count = 1;
    //打开视频解码器
    ret = avcodec_open2(vc_context, 0, 0);
    if(ret != 0){
        LOGW("avcodec_open2 video failed!");
        return ;
    }

    //音频解码器初始化
    AVCodec *ac = avcodec_find_decoder(ic->streams[audioStream]->codecpar->codec_id); //软解码
    if(!ac){
        LOGW("audio avcodec find failed !");
        return ;
    }
    AVCodecContext *ac_context = avcodec_alloc_context3(ac);
    avcodec_parameters_to_context(ac_context,ic->streams[audioStream]->codecpar);
    ac_context->thread_count = 1;
    //打开音频解码器
    ret = avcodec_open2(ac_context, 0, 0);
    if(ret != 0){
        LOGW("avcodec open2 audio failed!");
        return ;
    }

    //初始化像素格式转换上下文
    SwsContext *vctx = NULL;
    int outWidth = 1280;
    int outHeight = 720;
    uint8_t *rgb = new uint8_t[1300 * 1000 * 4];
    uint8_t *pcm = new uint8_t[48000 * 4 * 2];

    //音频重采样上下文初始化
    SwrContext *actx = swr_alloc();
    actx = swr_alloc_set_opts(actx,
                              av_get_default_channel_layout(ac_context->channels),
                              AV_SAMPLE_FMT_S16,
                              ac_context->sample_rate,
                              av_get_default_channel_layout(ac_context->channels),
                              ac_context->sample_fmt,
                              ac_context->sample_rate,
                              0,0
    );

    ret = swr_init(actx);
    if(ret != 0){
        LOGW("swr_init failed.");
    } else {
        LOGW("swr_init successfully.");
    }

    //读取帧数据
    AVPacket* packet = av_packet_alloc();
    //解码后的帧数据
    AVFrame* frame = av_frame_alloc();

    ANativeWindow *native_window = ANativeWindow_fromSurface(env, surface);
    if(!native_window){
        LOGI("native create failed...");
    }
    ANativeWindow_setBuffersGeometry(native_window, outWidth, outHeight, WINDOW_FORMAT_RGBA_8888);
    ANativeWindow_Buffer wbuf;

    for(;;){
        int ret = av_read_frame(ic,packet);
        if(ret != 0){
            LOGW("read in end.");
            /*
            int pos = r2d(ic->streams[videoStream]->time_base) * 20;
            //seek
            av_seek_frame(ic, videoStream, ic->duration/2, AVSEEK_FLAG_BACKWARD|AVSEEK_FLAG_FRAME);
            continue;
             */
            break;
        }

        LOGW("stream = %d, size = %d, pts = %lld, flag = %d", packet->stream_index, packet->size,
             packet->pts, packet->flags);

        AVCodecContext *aacontext = vc_context;
        if(packet->stream_index == AVMEDIA_TYPE_AUDIO){
            aacontext = ac_context;
        }

        //发送解码队列，让ffmpeg去解码
        ret = avcodec_send_packet(aacontext, packet);
        av_packet_unref(packet); //释放data空间

        if(ret != 0){
            LOGW("send packet decode failed.");
            continue;
        }

        for(;;){
            ret = avcodec_receive_frame(aacontext, frame);
            if(ret != 0){
                break;
            }
            LOGI("receive frame pts : %lld", frame->pts);

            //视频图像数据处理
            if(aacontext == vc_context){

                vctx = sws_getCachedContext(vctx,
                                            frame->width,
                                            frame->height,
                                            (AVPixelFormat)frame->format,
                                            outWidth,
                                            outHeight,
                                            AV_PIX_FMT_RGBA,
                                            SWS_FAST_BILINEAR,
                                            0,0,0);

                LOGW("frame width=%d, height=%d, format=%d", frame->width, frame->height, frame->format);


                if(!vctx){
                    LOGW("sws_getCacheContext failed.");
                } else {
                    uint8_t  *data[AV_NUM_DATA_POINTERS] = {0};
                    data[0] = rgb;
                    int lines[AV_NUM_DATA_POINTERS] = {0};
                    lines[0] = outWidth * 4;
                    int h = sws_scale(vctx, frame->data, frame->linesize, 0, frame->height,data, lines);
                    LOGW("sws_scale : %d", h);

                    if(ANativeWindow_lock(native_window, &wbuf, 0)){
                        LOGI("lock faile...");
                    }
                    void  *dst_buf = wbuf.bits;
                    memcpy(dst_buf, rgb, outWidth * outHeight* 4);
                    ANativeWindow_unlockAndPost(native_window);
                }
            }

            if(aacontext == ac_context){

                uint8_t *out[2] = {0};
                out[0] = pcm;

                //audio resample.
                int len = swr_convert(actx, out, frame->nb_samples, (const uint8_t **)frame->data, frame->nb_samples);
                LOGW("swr_convert len : %d", len);
            }
        }
    }


    delete[] rgb;
    delete[] pcm;


    //释放窗口
//    ANativeWindow_release(native_window);
    //关闭上下文
    avformat_close_input(&ic);

    env->ReleaseStringUTFChars(url, path);
}