#include <jni.h>
#include <android/log.h>
#include <GLES/gl.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <unistd.h>
#include <chrono>
#include <string>

extern "C"{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libavutil/imgutils.h"
#include "libswscale/swscale.h"
}

#define NELEM(X) sizeof(X)/sizeof(X[0])
#define LOG(X) __android_log_print(ANDROID_LOG_DEBUG, "DEBUG", (X))

const char* REGCLASS = "com/max/openglffmpeg/FFMpegRender";
static JNIEnv *this_env;

short indices  [4]  = {0,1,2,3};
GLuint m_iTexture;
const float vertices[] = {
        -1.0f, -1.0f, 0,
        1.0f, -1.0f, 0,
        1.0f,  1.0f, 0,
        -1.0f, 1.0f, 0
};
// 四边形坐标(0-1)
const float coords[] = {
        0.0f, 1.0f,
        1.0f, 1.0f,
        1.0f, 0.0f,
        0.0f, 0.0f,
};

const char* pFile="/sdcard/test/test1.mp4";
static std::mutex mut;
static uint8_t * src;
static int videoWidth;
static int videoHeight;
static double audio_clock_now;
static double audio_clock_pre = 0;
static std::chrono::steady_clock::time_point timeNow;
static std::chrono::steady_clock::time_point timePre;
static bool isStarted = false;
static bool isSurfaceCreated = false;

void _surfaceChange(int width, int height);
void _createSurface();
void _drawFrame();
jint openVideo();
bool setupGraphics();
void loadTexture(void* pData, GLuint m_iVideoW, GLuint m_iVideoH);

void _surfaceChange(int width, int height){
    //图形最终显示到屏幕的区域的位置、长和宽
    glViewport(0, 0, width, width);
    //指定矩阵
    glMatrixMode(GL_PROJECTION);
    //将当前的矩阵设置为glMatrixGL_TRIANGLE_STRIPMode指定的矩阵
    glLoadIdentity();
    glOrthof(-2, 2, -2, 2, -2, 2);
}
void _createSurface(){
//    setupGraphics();
    isSurfaceCreated = true;
}

void _play(JNIEnv *env, jclass clazz, jstring obj){
    pFile = env->GetStringUTFChars(obj, NULL);
    std::thread play_thread(openVideo);
    play_thread.detach();
}

void _drawFrame(){
    if (!isStarted)
        return;

    std::unique_lock<std::mutex> lock(mut);
    loadTexture(src, videoWidth, videoHeight);
}

bool setupGraphics()
{
    glDisable(GL_DITHER);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

    glClearColor(.5f, .5f, .5f, 1);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glRotatef(-180.0f,1.0f,0.0f,0.0f);

    return true;
}

void loadTexture(void* pData, GLuint m_iVideoW, GLuint m_iVideoH)
{
    if(!m_iTexture)
    {
        LOG("gen texture");
        glGenTextures(1, &m_iTexture);
    }
    // 清除屏幕
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // 启用顶点和纹理坐标
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    glBindTexture(GL_TEXTURE_2D, m_iTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_iVideoW, m_iVideoH, 0, GL_RGBA, GL_UNSIGNED_BYTE/*GL_UNSIGNED_SHORT_5_6_5*/, pData);

    // 绘制四边形
    glMatrixMode(GL_PROJECTION);
    glFrontFace(GL_CCW);
    glVertexPointer(3, GL_FLOAT, 0, vertices);
    glEnable(GL_TEXTURE_2D);
    glTexCoordPointer(2, GL_FLOAT, 0, coords);
    glDrawElements(GL_TRIANGLE_FAN, 4, GL_UNSIGNED_SHORT, indices);
}

jint openVideo(){

    av_register_all();

    AVFormatContext *pFormatCtx = avformat_alloc_context();

    if (avformat_open_input(&pFormatCtx, pFile, NULL, NULL) != 0){
        __android_log_print(ANDROID_LOG_DEBUG, "DEBUG", "avformat_open error. ");
        return -1;
    }

    if (avformat_find_stream_info(pFormatCtx, NULL) < 0){
        __android_log_print(ANDROID_LOG_DEBUG, "DEBUG", "avformat find stream info failed. ");
        return -1;
    }

    // Find the first video stream
    int videoStream = -1, i;
    for (i = 0; i < pFormatCtx->nb_streams; i++) {
        if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO
            && videoStream < 0) {
            videoStream = i;
        }
    }
    if(videoStream==-1) {
        LOG("Didn't find a video stream.");
        return -1; // Didn't find a video stream
    }

    // Get a pointer to the codec context for the video stream
    AVCodecContext  * pCodecCtx = pFormatCtx->streams[videoStream]->codec;

    // Find the decoder for the video stream
    AVCodec * pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    if(pCodec==NULL) {
        LOG("Codec not found.");
        return -1; // Codec not found
    }

    if(avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        LOG("Could not open codec.");
        return -1; // Could not open codec
    }

    // 获取视频宽高
     videoWidth = pCodecCtx->width;
     videoHeight = pCodecCtx->height;

    __android_log_print(ANDROID_LOG_DEBUG, "DEBUG", "video width%d height%d ", videoWidth, videoHeight);

    LOG("program here. ");
    if(avcodec_open2(pCodecCtx, pCodec, NULL)<0) {
        LOG("Could not open codec.");
        return -1; // Could not open codec
    }

    // Allocate video frame
    AVFrame * pFrame = av_frame_alloc();

    // 用于渲染
    AVFrame * pFrameRGBA = av_frame_alloc();
    if(pFrameRGBA == NULL || pFrame == NULL) {
        LOG("Could not allocate video frame.");
        return -1;
    }

    // Determine required buffer size and allocate buffer
    int numBytes=av_image_get_buffer_size(AV_PIX_FMT_RGBA, pCodecCtx->width, pCodecCtx->height, 1);
    uint8_t * buffer=(uint8_t *)av_malloc(numBytes*sizeof(uint8_t));
    av_image_fill_arrays(pFrameRGBA->data, pFrameRGBA->linesize, buffer, AV_PIX_FMT_RGBA,
                         pCodecCtx->width, pCodecCtx->height, 1);

    // 由于解码出来的帧格式不是RGBA的,在渲染之前需要进行格式转换
    struct SwsContext *sws_ctx = sws_getContext(pCodecCtx->width,
                                                pCodecCtx->height,
                                                pCodecCtx->pix_fmt,
                                                pCodecCtx->width,
                                                pCodecCtx->height,
                                                AV_PIX_FMT_RGBA,
                                                SWS_BILINEAR,
                                                NULL,
                                                NULL,
                                                NULL);

    int frameFinished;
    AVPacket packet;
    while(av_read_frame(pFormatCtx, &packet)>=0) {
        // Is this a packet from the video stream?
        if(packet.stream_index==videoStream) {

            // Decode video frame
            avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);

            // 并不是decode一次就可解码出一帧
            if (frameFinished) {
                // 格式转换
                sws_scale(sws_ctx, (uint8_t const * const *)pFrame->data,
                          pFrame->linesize, 0, pCodecCtx->height,
                          pFrameRGBA->data, pFrameRGBA->linesize);

                pFrame->pts = av_frame_get_best_effort_timestamp(pFrame);
                audio_clock_now = pFrame->pts * av_q2d(pFormatCtx->streams[videoStream]->time_base);  //计算显示时间

                //同步外部时间 play video sync to system clock
                if (!isStarted){
                    timePre = timeNow = std::chrono::steady_clock::now();
                    isStarted = true;
                } else{
                    long timestamp_duration = (audio_clock_now - audio_clock_pre) * 1000000;
                    audio_clock_pre = audio_clock_now;
                    timeNow = std::chrono::steady_clock::now();
                    long duration = std::chrono::duration_cast<std::chrono::microseconds>(timeNow - timePre).count();

                    __android_log_print(ANDROID_LOG_DEBUG, "DEBUG", "timestamp %ld", timestamp_duration);
                    __android_log_print(ANDROID_LOG_DEBUG, "DEBUG", "duration %ld ", duration);

                    if (timestamp_duration > duration){
                        __android_log_print(ANDROID_LOG_DEBUG, "DEBUG", "sleep %ld us ",(timestamp_duration-duration));
                        usleep(timestamp_duration - duration);
                    }
                    timePre = std::chrono::steady_clock::now();
                }

                //use lock when change decoded data
                {
                    std::unique_lock<std::mutex> lock(mut);
                    src = (uint8_t*) (pFrameRGBA->data[0]);
                }
                int srcStride = pFrameRGBA->linesize[0];
            }

        }
        av_packet_unref(&packet);
    }

    av_free(buffer);
    av_free(pFrameRGBA);

    // Free the YUV frame
    av_free(pFrame);

    // Close the codecs
    avcodec_close(pCodecCtx);

    // Close the video file
    avformat_close_input(&pFormatCtx);
}

static JNINativeMethod method[]={
        {"surfaceChange", "(II)V", (void *)_surfaceChange},
        {"createSurface", "()V", (void *)_createSurface},
        {"drawFrame", "()V", (void *)_drawFrame},
        {"native_play", "(Ljava/lang/String;)V", (void *)_play}
};

jint JNI_OnLoad(JavaVM *vm, void* reserved){

    jint ret = 0;
    if (vm->GetEnv((void **)&this_env, JNI_VERSION_1_4) != JNI_OK){
        LOG("get env failed, now exit. ");
        return (-1);
    }

    jclass clazz = this_env->FindClass(REGCLASS);

    this_env->RegisterNatives(clazz, method, NELEM(method));

    return JNI_VERSION_1_4;
}
