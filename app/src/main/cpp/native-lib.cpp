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

//egl
#include <EGL/egl.h>

//opengl
#include <GLES2/gl2.h>


//顶点着色器 glsl
#define GET_STR(x) #x
static const char *vertexShader = GET_STR(
        attribute vec4 aPosition; //顶点坐标
        attribute vec2 aTexCoord; //材质顶点坐标
        varying vec2 vTexCoord;   //输出的材质坐标
        void main(){
            vTexCoord = vec2(aTexCoord.x,1.0-aTexCoord.y);
            gl_Position = aPosition;
        }
);

//片元着色器, 软解码和部分x86硬解码出来的格式
static const char *fragYUV420P = GET_STR(
        precision mediump float;    //精度
        varying vec2 vTexCoord;     //顶点着色器传递的坐标
        uniform sampler2D yTexture; //输入的材质（不透明灰度，单像素）
        uniform sampler2D uTexture;
        uniform sampler2D vTexture;
        void main(){
            vec3 yuv;
            vec3 rgb;
            yuv.r = texture2D(yTexture,vTexCoord).r;
            yuv.g = texture2D(uTexture,vTexCoord).r - 0.5;
            yuv.b = texture2D(vTexture,vTexCoord).r - 0.5;
            rgb = mat3(1.0,     1.0,    1.0,
                       0.0,-0.39465,2.03211,
                       1.13983,-0.58060,0.0)*yuv;
            //输出像素颜色
            gl_FragColor = vec4(rgb,1.0);
        }
);

GLint initShader(const char *code, GLint type) {

    //申请创建shader
    GLint sh = glCreateShader(type);
    if(sh == 0) {
        LOGW("glCreateShader %d failed.", type);
        return GL_FALSE;
    }

    //加载shader
    glShaderSource(sh,
                   1, //shader的数量
                   &code,
                   0); //代码长度

    //编译shader
    glCompileShader(sh);

    //获取编译情况
    GLint status;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &status);

    if(status == GL_FALSE) {
        LOGW("glCompile shader failed.");
        return GL_FALSE;
    }

    return sh;
}



static inline double r2d(AVRational r){
    return r.num == 0 || r.den == 0 ? 0.: (double)r.num/(double)r.den;
}

long long GetNowMs(){

    struct timeval tv;
    gettimeofday(&tv, nullptr);

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
    unsigned int test = 3434l;

    if(jbuf != NULL){
        LOGI("buf : %s, capcity : %d", jbuf, capcity);
        LOGI("jbuf address: %p, hex_str: 0x%x", jbuf, test);
    }
}



void testffmpegToDraw(JNIEnv *env, const jstring& url, const jobject& surface) {
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


void testShaderToDraw(JNIEnv *env, const jstring& url_, const jobject& surface) {

    const char *url = env->GetStringUTFChars(url_, 0);
    LOGI("open url is %s", url);

    FILE *fp = fopen(url , "rb");
    if(!fp) {
        LOGI("open file %s failed!", url);
        return ;
    }

    // 获取原始窗口
    ANativeWindow *win = ANativeWindow_fromSurface(env, surface);

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
    EGLSurface  win_surface = eglCreateWindowSurface(display, config, win, nullptr);
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

    LOGD("egl init successful.");

    //顶点和片元初始化

    //顶点shader初始化
    GLint vsh = initShader(vertexShader, GL_VERTEX_SHADER);
    //片元shader 初始化
    GLint fsh = initShader(fragYUV420P, GL_FRAGMENT_SHADER);

    //创建渲染程序
    GLint program = glCreateProgram();
    if(program == GL_FALSE) {
        LOGW("glCreate program failed.");
        return ;
    }

    //向程序当中加入着色器代码
    glAttachShader(program, vsh);
    glAttachShader(program, fsh);

    //链接程序
    glLinkProgram(program);
    GLint status;
    glGetProgramiv(program, GL_LINK_STATUS, &status);

    if(status != GL_TRUE) {
        LOGW("gl program link failed.");
        return ;
    }

    LOGI("gl link program successful.");

    //激活渲染程序
    glUseProgram(program);

    //加入三维顶点数据
    static float vers[] = {
            1.0f, -1.0f, 0.0f,
            -1.0f, -1.0f, 0.0f,
            1.0f, 1.0f, 0.0f,
            -1.0f, 1.0f, 0.0f
    };

    GLuint apos = glGetAttribLocation(program, "aPosition");
    glEnableVertexAttribArray(apos);

    //传递顶点
    glVertexAttribPointer(apos, 3, GL_FLOAT, GL_FALSE, 12, vers);

    //加入材质坐标数据
    static float txts[] = {
            1.0f, 0.0f, //右下
            0.0f, 0.0f,
            1.0f, 1.0f,
            0.0f, 1.0f
    };

    GLuint  atex = glGetAttribLocation(program, "aTexCoord");
    glEnableVertexAttribArray(atex);
    glVertexAttribPointer(atex, 2, GL_FLOAT, GL_FALSE, 8, txts);

    int width = 424;
    int height = 240;

    //材质纹理初始化
    //设置纹理层
    glUniform1i(glGetUniformLocation(program, "yTexture"), 0); //对应纹理第一层
    glUniform1i(glGetUniformLocation(program, "uTexture"), 1);
    glUniform1i(glGetUniformLocation(program, "vTexture"), 2);

    //创建opengl材质
    GLuint texts[3] = {0};
    glGenTextures(3, texts); //创建三个材质
    //设置纹理属性
    glBindTexture(GL_TEXTURE_2D, texts[0]);
    //缩小的过滤器, GL_LINEAR 线性过滤，使用距离当前渲染像素中心最近的4个纹素加权平均值
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    //设置纹理格式和大小
    glTexImage2D(GL_TEXTURE_2D,
                 0, //细节 0:默认
                 GL_LUMINANCE, //gpu内部格式，亮度，灰度图
                 width, height,  //尺寸要是2的次方
                 0, //边框
                 GL_LUMINANCE, //数据的像素格式，要与上面保持一致
                 GL_UNSIGNED_BYTE, //像素的数据类型
                 NULL  //纹理的数据
                 );

    //设置纹理属性
    glBindTexture(GL_TEXTURE_2D, texts[1]);
    //缩小的过滤器, GL_LINEAR 线性过滤，使用距离当前渲染像素中心最近的4个纹素加权平均值
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    //设置纹理格式和大小
    glTexImage2D(GL_TEXTURE_2D,
                 0, //细节 0:默认
                 GL_LUMINANCE, //gpu内部格式，亮度，灰度图
                 width/2, height/2,  //尺寸要是2的次方
                 0, //边框
                 GL_LUMINANCE, //数据的像素格式，要与上面保持一致
                 GL_UNSIGNED_BYTE, //像素的数据类型
                 NULL  //纹理的数据
    );

    //设置纹理属性
    glBindTexture(GL_TEXTURE_2D, texts[2]);
    //缩小的过滤器, GL_LINEAR 线性过滤，使用距离当前渲染像素中心最近的4个纹素加权平均值
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    //设置纹理格式和大小
    glTexImage2D(GL_TEXTURE_2D,
                 0, //细节 0:默认
                 GL_LUMINANCE, //gpu内部格式，亮度，灰度图
                 width/2, height/2,  //尺寸要是2的次方
                 0, //边框
                 GL_LUMINANCE, //数据的像素格式，要与上面保持一致
                 GL_UNSIGNED_BYTE, //像素的数据类型
                 NULL  //纹理的数据
    );


    unsigned char *buf[3] = {0};
    buf[0] = new unsigned char [width * height];
    buf[1] = new unsigned char [width * height / 4];
    buf[2] = new unsigned char [width * height / 4];


    for(int i = 0; i < 10000; i++) {

//        memset(buf[0], i, width * height);
//        memset(buf[1], i, width * height /4);
//        memset(buf[2], i, width * height / 4);


        //yyyyyyyy uu vv
        if(feof(fp) == 0) {
            fread(buf[0], 1 , width * height, fp);
            fread(buf[1], 1, width * height / 4, fp);
            fread(buf[2], 1, width * height / 4, fp);
        }


        //纹理的修改和显示

        //激活第1层，绑定到创建的opengl的纹理
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texts[0]);
        //替换纹理内容
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_LUMINANCE, GL_UNSIGNED_BYTE,buf[0]);

        //激活第二层纹理，绑定到创建的open纹理
        glActiveTexture(GL_TEXTURE0 + 1);
        glBindTexture(GL_TEXTURE_2D, texts[1]);
        //替换纹理内容
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width/2, height/2, GL_LUMINANCE, GL_UNSIGNED_BYTE, buf[1]);

        //激活第二层纹理，绑定到创建的open纹理
        glActiveTexture(GL_TEXTURE0 + 2);
        glBindTexture(GL_TEXTURE_2D, texts[2]);
        //替换纹理内容
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width/2, height/2, GL_LUMINANCE, GL_UNSIGNED_BYTE, buf[2]);

        //三角绘制
        glDrawArrays(GL_TRIANGLE_STRIP, 0 ,4);
        //窗口显示
        eglSwapBuffers(display, win_surface);
    }

}


extern "C"
JNIEXPORT void JNICALL
Java_com_nipuream_n_1player_NPlayer_open(JNIEnv *env, jobject thiz, jstring url, jobject surface) {

//    testffmpegToDraw(env, url, surface);

      testShaderToDraw(env, url, surface);



}