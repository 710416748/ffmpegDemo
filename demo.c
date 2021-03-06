#include "include/main.h"

#define VIDEO_SOURCE "../1.mp4"
#define AUDIO_SOURCE "../1.mp3"
#define PICTURE_SOURCE "../1.jpg"
#define YUV_SOURCE "../output.yuv"
#define OUT_TS "../out.mp4"
#define PIC_OUT "../pic.yuv"
#define PCM_OUT "../outAudio.pcm"
#define MP3_OUT "../out.mp3"

#define true 1
#define false 0

//#define DUMP_IN 1
#define MAX_AUDIO_FRAME_SIZE 192000 // 1 second of 48khz 32-bit audio

//For decoder
AVCodec         *mCodecDecoder = NULL;     //编解码器
AVFormatContext *mFmtCtxDecoder = NULL;    //贯穿ffmpeg的上下文
AVIOContext     *mIOContext = NULL;        //输入设备上下文，ffmpeg将文件也视为一种协议
AVFrame         *mFrame = NULL;            //解码后的原始帧数据
AVFrame         *mFrameYUV = NULL;         //原始帧数据转化后的YUV数据
AVPacket        mPacket;                   //解码前的帧数据
AVInputFormat   *mInputFormat = NULL;      //输入格式的上下文
AVCodecContext  *mCodecContext = NULL;     //编解码器的上下文
AVCodecParameters *mCodecParaEncoder = NULL; //取代AVCodecContext

//For encoder
AVFormatContext *mFmtCtxEncoder = NULL;
AVOutputFormat  *mOutputFormat = NULL;     //输出格式
AVStream        *mViderStream = NULL;
AVCodecContext  *mCodecCtxEncoder = NULL;
AVCodec         *mCodecEncoder = NULL;
AVPacket         mPacketEncoder;
AVFrame         *mFrameEncoder = NULL;
uint8_t         *mPictureBuffer = NULL;
int              mPictureSize = 0;

//For pic
AVFormatContext *mFmtCtxPic = NULL;
AVCodec         *mCodecPic = NULL;
AVCodecContext  *mCodecCtxPic = NULL;
AVFrame         *mFramePic = NULL;
AVFrame         *mFramePicYUV = NULL;
AVPacket         mPacketPIC;
uint8_t         *mBufferPic = NULL;
int              mBufferSize = 0;

//For audio
AVFormatContext *mFmtCtxAudio = NULL;
AVCodec         *mCodecAudio = NULL;
AVCodecContext  *mCodecCtxAudio = NULL;
AVFrame         *mFrameAudio = NULL;
AVPacket        mPacketAudio;

//for audio encode
AVFormatContext *mFmtCtxAudioEn = NULL;
AVOutputFormat  *mOutFmtAndioEN = NULL;
AVCodec         *mCodecAudioEn = NULL;
AVCodecContext  *mCodecCtxAudioEn = NULL;
AVFrame         *mFrameAudioEn = NULL;
AVStream        *mAudioStreamEn = NULL;
AVPacket        mPacketAudioEn;


//for target mp4
AVFormatContext *mAVFmtCtx;
AVOutputFormat *mOutFmt;


//for file source
char *mAudioSource = NULL;
char *mPictureSource = NULL;
char *mOutVideoSource = NULL;

struct SwsContext *img_convert_ctx = NULL; //图像处理的上下文

#ifdef DUMP_IN
FILE            *mYuvFd = NULL;                   //输出文件
#endif
FILE            *mYuvInFd = NULL;
FILE            *mEncodeOutFd = NULL;

FILE            *mPicInFd = NULL;
FILE            *mPicOutFd = NULL;

FILE            *mAudioPCM = NULL;
FILE            *mAudioIn = NULL;
FILE            *mAudioOut = NULL;


unsigned char   *mBuffer = NULL;           //应该是用于存储像素点

//不公开的结构体
//AVIStream       *mIStream;               该字段为AVStream中的priv_data数据
//URLContext      *mURLContext;            IOContext中使用的内部数据结构

void decoder_video() {
    int ret = 0;
    int i = 0;
    int got_frame = 0;
    int y_size = 0;
    int video_index = -1;

    //打开文件
    //ret = avio_open2(&mIOContext,VIDEO_SOURCE,AVIO_FLAG_READ,NULL,NULL);

    //打开文件并将AVStream等参数赋值给AVFormatContext
    ret = avformat_open_input(&mFmtCtxDecoder,VIDEO_SOURCE,NULL,NULL);
    if (ret < 0) {
        printf("open failed\n");
    } else {
        // 获取流的信息赋值给AVFormatContext
        ret = avformat_find_stream_info(mFmtCtxDecoder,NULL);
        if (ret < 0 ) {
            printf("find stream info error\n");
            exit(1);
        }
        
	//获取部分AVInputFormat信息
        //mInputFormat = mFmtCtxDecoder->iformat;
        //if (mInputFormat == NULL) {
        //    printf("iformat == NULL\n");
        //} else {
        //    printf("iformat name is %s, codec id is =%d\n",mInputFormat->name,mInputFormat->raw_codec_id);
        //    if(!mInputFormat->codec_tag) {
        //        printf("codec tag is NULL\n");
        //    }
    }
    //mIOContext = mFmtCtxDecoder->opaque;

    printf("nb_streams = %u\n",mFmtCtxDecoder->nb_streams);

    for (i = 0; i < mFmtCtxDecoder->nb_streams; i++) {
        printf("AVStream[%d]:nb_frames = %ld\n",i,(*(mFmtCtxDecoder->streams + i))->nb_frames);

        //判断是否为视频信息
        if ((mFmtCtxDecoder->streams)[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            //获取编解码器上下文
            mCodecContext = mFmtCtxDecoder->streams[i]->codec;

            video_index = i;
            if (mCodecContext->codec_name) {
                printf("codec name = %s, codec id = %d\n",mCodecContext->codec_name,mCodecContext->codec_id);

                //根据编码器id找到decodeer
                mCodecDecoder = avcodec_find_decoder(mCodecContext->codec_id);
                if(!mCodecDecoder) {
                    printf("Cannot find codec\n");
                }

                //打开解码器
                if (avcodec_open2(mCodecContext,mCodecDecoder,NULL) < 0 ) {
                    printf("open codec fail\n");
                }
            }
        }
    }

    if (mCodecContext == NULL) {
        printf("get mCodecContext fail\n");
        exit(1);
    }

    //初始化buffer，（应该是用于存放像素点的）
    mBuffer = (unsigned char *)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUV420P,
                                        mCodecContext->width, mCodecContext->height,1));  

    //为YUVFrame分配相应空间
    av_image_fill_arrays(mFrameYUV->data, mFrameYUV->linesize,mBuffer,  
            AV_PIX_FMT_YUV420P,mCodecContext->width, mCodecContext->height,1);  

    //Y数据的大小
    y_size = mCodecContext->width * mCodecContext->height;
    printf("width = %d,height = %d\n",mCodecContext->width,mCodecContext->height);


    //初始化图像格式转化的上下文
    img_convert_ctx = sws_getContext(mCodecContext->width, mCodecContext->height, mCodecContext->pix_fmt,   
        mCodecContext->width, mCodecContext->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);  

    ret = 0;
    while (ret == 0) {
    
        //读取一帧数据
        ret = av_read_frame(mFmtCtxDecoder,&mPacket);
        if (ret == 0) {
            //printf("data size = %d, pts = %ld, dts = %ld, index = %d\n",mPacket.size,mPacket.pts,mPacket.dts,mPacket.stream_index);
            if (mPacket.stream_index != video_index) {
                continue;
            }

            //解码一帧数据，成功返回数据大小
            i = avcodec_decode_video2(mCodecContext,mFrame,&got_frame,&mPacket);

            if (i > 0) {
                printf("mFrame format = %d, got_frame = %d, data size = %d, is I-Frame %s.\n",mFrame->format,got_frame,i,mFrame->format?"true":"false");
                
                //将原始的图像数据转化为YUV数据
                sws_scale(img_convert_ctx, (const unsigned char* const*)mFrame->data, mFrame->linesize, 0, mCodecContext->height,   
                    mFrameYUV->data, mFrameYUV->linesize); 
#ifdef DUMP_IN
                //dump
                fwrite(mFrameYUV->data[0],1,y_size,mYuvFd);    //Y
                fwrite(mFrameYUV->data[1],1,y_size/4,mYuvFd);  //U
                fwrite(mFrameYUV->data[2],1,y_size/4,mYuvFd);  //V
#endif
        } else if (i == 0) {
                printf("There is no frame needed to decode\n");
            }
        } else {
            printf("read frame end\n");
        }
    }

//if (mIOContext == NULL || mIOContext->opaque == NULL) {
//    printf("mIOContext or opaque == NULL\n");
//    exit(1);
//} else {
//    mURLContext = mIOContext->opaque;

//    printf("file name is %s",mIOContext->opaque->filename);
//}


//for (mCodecDecoder = av_codec_next(avcodec); mCodecDecoder != NULL; mCodecDecoder = av_codec_next(avcodec)) {

//    printf("first codec short name is %s,\tlong name is %s\n",
//                            mCodecDecoder->name,mCodecDecoder->long_name);
//}

}

void init() {
#ifdef DUMP_IN
    mYuvFd = fopen("output.yuv","wb+");
#endif
    //注册各种编解码器等
    av_register_all();

    //为avformat分配空间和部分参数初始化
    mFmtCtxDecoder = avformat_alloc_context();
    if (mFmtCtxDecoder == NULL) {
        printf("init AVFormatContext == NULL,exit\n");
        exit(1);
    }

    // mFmtCtxDecoder->pb为AVIOContext数据，输入文件的协议，其成员opaque在ffmpeg源码内部使用
    //if (mFmtCtxDecoder->pb == NULL) {
    //    printf("AVIOContext == NULL\n");
    //    avio_alloc_context(mFmtCtxDecoder->pb);
    //}

    //mIOContext = mFmtCtxDecoder->pb;

    //为AVFrame初始化，不包含像素点占用的空间
    mFrame = av_frame_alloc();
    mFrameYUV = av_frame_alloc();
    if (mFrame == NULL || mFrameYUV == NULL) {
        printf("mFrame == NULL\n");
        exit(1);
    }

    //88字节，不包含帧数据
    av_init_packet(&mPacket);

    //printf("av_init_packet size is %lu\n",sizeof(mPacket));
    //av_free_packet(&mPacket);

    //初始化AVFrame的第二种方式
    //av_new_packet(&mPacket,10);
    //printf("av_new_packet size is %lu\n",sizeof(mPacket));
    //av_free_packet(&mPacket);

}

void relese_buffer() {
    if (mFmtCtxDecoder) {
        avformat_free_context(mFmtCtxDecoder);
    }
/*
    if (mFmtCtxEncoder) {
        avformat_free_context(mFmtCtxEncoder);
    }*/
    if (mViderStream) {
        if (mViderStream->codec) {
            avcodec_close(mViderStream->codec);
        }

        if (mFrameEncoder) {
            av_free(mFrameEncoder);
        }
        if (mPictureBuffer) {
            av_free(mPictureBuffer);
        }
    }
#ifdef DUMP_IN
    fclose(mYuvFd);
#endif
}

void init_encoder() {
    mYuvInFd = fopen(YUV_SOURCE,"rb");
    if (!mYuvInFd) {
        printf("open file fail\n");
        exit(1);
    }
    av_register_all();
}
void encoder_video() {
    int ret = -1;
    int in_width = 720;
    int in_height = 480;
    int y_size = 0;

    // 根据输出文件名初始化一些参数，并选择编码器等
    avformat_alloc_output_context2(&mFmtCtxEncoder,NULL,NULL,OUT_TS);
    mOutputFormat = mFmtCtxEncoder->oformat;

    // 将输出文件的和mURLContext绑定
    ret = avio_open(&mFmtCtxEncoder->pb,OUT_TS,AVIO_FLAG_READ_WRITE);
    if (ret < 0) {
        printf("avio_open fail \n");
        exit(1);
    }

    // 创建视频流
    mViderStream = avformat_new_stream(mFmtCtxEncoder,0);
    if (!mViderStream) {
        printf("mViderStream == NULL\n");
        exit(1);
    }

    // 必要的编码器参数设置
    mCodecCtxEncoder = mViderStream->codec;
    mCodecCtxEncoder->codec_id = mOutputFormat->video_codec;
    mCodecCtxEncoder->codec_type = AVMEDIA_TYPE_VIDEO;
    mCodecCtxEncoder->pix_fmt = AV_PIX_FMT_YUV420P;
    mCodecCtxEncoder->width = in_width;
    mCodecCtxEncoder->height = in_height;
    mCodecCtxEncoder->bit_rate = 400000;
    mCodecCtxEncoder->gop_size = 250;
    mCodecCtxEncoder->time_base.num = 1;
    mCodecCtxEncoder->time_base.den = 25;

    mCodecCtxEncoder->qmin =10;
    mCodecCtxEncoder->qmax = 51;
    mCodecCtxEncoder->max_b_frames = 3;

    AVDictionary *param = 0;

    printf("mCodecCtxEncoder codec id = %d\n",mCodecCtxEncoder->codec_id);
    av_dump_format(mFmtCtxEncoder, 0, OUT_TS, 1);

    mCodecEncoder = avcodec_find_encoder(mCodecCtxEncoder->codec_id);

    if (!mCodecEncoder) {
        printf("cannot find encoder\n");
        exit(1);
    }

    ret = avcodec_open2(mCodecCtxEncoder,mCodecEncoder, NULL);
    if (ret < 0) {
        printf("cannot open encoder\n");
        exit(1);
    }

    mFrameEncoder = av_frame_alloc();
    if (!mFrameEncoder) {
        printf("mFrameEncoder == NULL\n");
        exit(1);
    }

    mPictureSize = avpicture_get_size(mCodecCtxEncoder->pix_fmt,mCodecCtxEncoder->width,mCodecCtxEncoder->height);
    mPictureBuffer = (uint8_t *)av_malloc(mPictureSize);

    avpicture_fill((AVPicture *)mFrameEncoder,mPictureBuffer,mCodecCtxEncoder->pix_fmt,mCodecCtxEncoder->width,mCodecCtxEncoder->height);

    // 写入文件头
    avformat_write_header(mFmtCtxEncoder,NULL);

    av_new_packet(&mPacketEncoder,mPictureSize);

    y_size = mCodecCtxEncoder->width * mCodecCtxEncoder->height;
    int size = 0;
    int frame_num = 0;
    int frame_encoder_num = 0;

    for (;;) {

        // 读取YUV数据
        size = fread(mPictureBuffer,1,y_size * 3 / 2,mYuvInFd);
        if (size < y_size * 3 / 2) {
            printf("remain %d byte\ntotal read %d frames\n",size,frame_num);
            break;
        } else if(feof(mYuvInFd)) {
            printf("total read %d frames\n",frame_num);
            break;
        }else {
            frame_num++;
        }
        mFrameEncoder->data[0] = mPictureBuffer;
        mFrameEncoder->data[1] = mPictureBuffer + y_size;
        mFrameEncoder->data[2] = mPictureBuffer + y_size * 5 / 4;

        mFrameEncoder->format = 0;
        mFrameEncoder->width = in_width;
        mFrameEncoder->height = in_height;

        mFrameEncoder->pts = frame_num * mViderStream->time_base.den / ((mViderStream->time_base.num) * 25);

        int got_pic = 0;

        // 编码
        //printf("before encode\n");
        ret = avcodec_encode_video2(mCodecCtxEncoder,&mPacketEncoder,mFrameEncoder,&got_pic);
        //printf("after encode\n");
        if (ret < 0 ) {
            printf("encode fail\n");
            exit(1);
        }

        if (1 == got_pic) {
            frame_encoder_num++;
            mPacketEncoder.stream_index = mViderStream->index;
            av_write_frame(mFmtCtxEncoder,&mPacketEncoder);
            av_free_packet(&mPacketEncoder);
        }
    }

    // 文件尾写入
    av_write_trailer(mFmtCtxEncoder);
}

void init_pic() {
    av_register_all();

    mPicOutFd = fopen(PIC_OUT,"wb+");
    if (!mPicOutFd) {
        printf("open fail \n");
        exit(1);
    }
    mFramePic = av_frame_alloc();
    mFramePicYUV= av_frame_alloc();
}

void decoder_pic() {
    int ret = -1;
    int y_size = 0;
    int got_frame = 0;
    int pic_size = 0;
    int count = 0;

    ret = avformat_open_input(&mFmtCtxPic,PICTURE_SOURCE,NULL,NULL);

    if (ret < 0) {
        ALOGE("open input fail\n");
        exit(1);
    }
        ret = avformat_find_stream_info(mFmtCtxPic,NULL);
    if (ret < 0 ) {
        ALOGE("find stream info error\n");
        exit(1);
    }
    ALOGI("nb_streams = %u,codec type = %d\n",mFmtCtxPic->nb_streams,(mFmtCtxPic->streams)[0]->codec->codec_type);
    mCodecCtxPic = (mFmtCtxPic->streams)[0]->codec;

    if (!mCodecCtxPic) {
        ALOGE("mCodecCtxPic is NULL\n");
        exit(1);
    }

    mCodecPic = avcodec_find_decoder((mFmtCtxPic->streams)[0]->codec->codec_id);
    if (!mCodecPic) {
        ALOGE("can not find decoder\n");
        exit(1);
    }

    ret = avcodec_open2(mCodecCtxPic,mCodecPic,NULL);

    if (ret < 0) {
        ALOGE("open encoder fail\n");
        exit(1);
    }
    ALOGI("picture codec id = %d,type= %d\n",mCodecCtxPic->codec_id,mCodecCtxPic->codec_type);

    mBufferPic = (unsigned char *)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUV420P,
                                    mCodecCtxPic->width, mCodecCtxPic->height,1));

    av_image_fill_arrays(mFramePicYUV->data, mFramePicYUV->linesize,mBufferPic,
        AV_PIX_FMT_YUV420P,mCodecCtxPic->width, mCodecCtxPic->height,1);

    y_size = mCodecCtxPic->width * mCodecCtxPic->height;
    ALOGI("width = %d,height = %d\n",mCodecCtxPic->width,mCodecCtxPic->height);

    img_convert_ctx = sws_getContext(mCodecCtxPic->width, mCodecCtxPic->height, mCodecCtxPic->pix_fmt,
        mCodecCtxPic->width, mCodecCtxPic->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);

    pic_size = avpicture_get_size(mCodecCtxPic->pix_fmt,mCodecCtxPic->width,mCodecCtxPic->height);
   // av_new_packet(&mPacketPIC,pic_size);

    ret = av_read_frame(mFmtCtxPic,&mPacketPIC);


    if (ret != 0) {
        ALOGE("read frame fail\n");
        exit(1);
    }


     ret = avcodec_decode_video2(mCodecCtxPic,mFramePic,&got_frame,&mPacketPIC);

     if (ret > 0) {
        ALOGI("ret = %d,got_frame = %d\n",ret,got_frame);
        sws_scale(img_convert_ctx, (const unsigned char* const*)mFramePic->data, mFramePic->linesize, 0,
                        mCodecCtxPic->height,mFramePicYUV->data, mFramePicYUV->linesize);

        fwrite(mFramePicYUV->data[0],1,y_size,mPicOutFd);    //Y
        fwrite(mFramePicYUV->data[1],1,y_size/4,mPicOutFd);  //U
        fwrite(mFramePicYUV->data[2],1,y_size/4,mPicOutFd);  //V
    } else if (ret == 0) {
        ALOGI("read zero frame\n");
    } else {
        ALOGI("ret = %d,got_frame = %d\n",ret,got_frame);
        ALOGI("decode frame fail\n");
        exit(1);
    }

}

void pic_to_video() {

    int ret = -1;
    int in_width = mCodecCtxPic->width;
    int in_height = mCodecCtxPic->height;
    int y_size = in_width * in_height;
    avformat_alloc_output_context2(&mFmtCtxEncoder,NULL,NULL,OUT_TS);
    mOutputFormat = mFmtCtxEncoder->oformat;

    ret = avio_open(&mFmtCtxEncoder->pb,OUT_TS,AVIO_FLAG_READ_WRITE);
    if (ret < 0) {
        ALOGE("avio_open fail \n");
        exit(1);
    }

    mViderStream = avformat_new_stream(mFmtCtxEncoder,0);
    if (!mViderStream) {
        ALOGE("mViderStream == NULL\n");
        exit(1);
    }
    mCodecCtxEncoder = mViderStream->codec;
    mCodecCtxEncoder->codec_id = mOutputFormat->video_codec;
    mCodecCtxEncoder->codec_type = AVMEDIA_TYPE_VIDEO;
    mCodecCtxEncoder->pix_fmt = AV_PIX_FMT_YUV420P;
    mCodecCtxEncoder->width = in_width;
    mCodecCtxEncoder->height = in_height;
    mCodecCtxEncoder->bit_rate = 400000;
    mCodecCtxEncoder->gop_size = 250;
    mCodecCtxEncoder->time_base.num = 1;
    mCodecCtxEncoder->time_base.den = 25;

    mCodecCtxEncoder->qmin =10;
    mCodecCtxEncoder->qmax = 51;
    mCodecCtxEncoder->max_b_frames = 3;

    AVDictionary *param = 0;

    ALOGE("mCodecCtxEncoder codec id = %d\n",mCodecCtxEncoder->codec_id);
    av_dump_format(mFmtCtxEncoder, 0, OUT_TS, 1);

    mCodecEncoder = avcodec_find_encoder(mCodecCtxEncoder->codec_id);

    if (!mCodecEncoder) {
        ALOGE("cannot find encoder\n");
        exit(1);
    }

    ret = avcodec_open2(mCodecCtxEncoder,mCodecEncoder, NULL);
    if (ret < 0) {
        ALOGE("cannot open encoder\n");
        exit(1);
    }

    mFrameEncoder = av_frame_alloc();
    if (!mFrameEncoder) {
        ALOGE("mFrameEncoder == NULL\n");
        exit(1);
    }

    mPictureSize = avpicture_get_size(mCodecCtxEncoder->pix_fmt,mCodecCtxEncoder->width,mCodecCtxEncoder->height);
    mPictureBuffer = (uint8_t *)av_malloc(mPictureSize);

    avpicture_fill((AVPicture *)mFrameEncoder,mPictureBuffer,mCodecCtxEncoder->pix_fmt,mCodecCtxEncoder->width,mCodecCtxEncoder->height);

    avformat_write_header(mFmtCtxEncoder,NULL);

    av_new_packet(&mPacketEncoder,mPictureSize);

    y_size = mCodecCtxEncoder->width * mCodecCtxEncoder->height;
    int size = 0;
    int frame_num = 0;
    int frame_encoder_num = 0;
    int frames = 0;

    for (frames = 0; frames < 100; frames++) {

        //printf("do one encoder\n");
        mFrameEncoder->data[0] = mFramePicYUV->data[0];
        mFrameEncoder->data[1] = mFramePicYUV->data[1];
        mFrameEncoder->data[2] = mFramePicYUV->data[2];

        mFrameEncoder->format = 0;
        mFrameEncoder->width = in_width;
        mFrameEncoder->height = in_height;

        mFrameEncoder->pts = frames * mViderStream->time_base.den / ((mViderStream->time_base.num) * 25);

        int got_pic = 0;

        //printf("before encode\n");
        ret = avcodec_encode_video2(mCodecCtxEncoder,&mPacketEncoder,mFrameEncoder,&got_pic);
        //printf("after encode\n");
        if (ret < 0 ) {
            ALOGE("encode fail\n");
            exit(1);
        }
        if (1 == got_pic) {
            frame_encoder_num++;
            mPacketEncoder.stream_index = mViderStream->index;
            av_write_frame(mFmtCtxEncoder,&mPacketEncoder);
            av_free_packet(&mPacketEncoder);
        }
    }
    ALOGI("total encode %d frames\n",frame_encoder_num);
    av_write_trailer(mFmtCtxEncoder);
}

void init_aud() {
    av_register_all();

    mAudioPCM = fopen(PCM_OUT,"wb+");

    if (!mAudioPCM) {
        ALOGE("open file failed");
    }

}

void decoder_audio() {
    int i = 0;
    int ret = -1;
    int audio_index = -1;

    if ( avformat_open_input(&mFmtCtxAudio,AUDIO_SOURCE,NULL,NULL) < 0) {
        ALOGE("open audio source failed\n");
    }

    if (avformat_find_stream_info(mFmtCtxAudio,NULL) < 0) {
        ALOGE("find stream info failed\n");
    }

    for (i = 0; i < mFmtCtxAudio->nb_streams; i++) {
        if (mFmtCtxAudio->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_index = i;
            ALOGE("audio index is %d\n",i);
            break;
        }
    }

    if (audio_index == -1) {
        ALOGE("cannot find audio stream \n");
        exit(1);
    }

    mCodecCtxAudio = mFmtCtxAudio->streams[audio_index]->codec;

    mCodecAudio = avcodec_find_decoder(mCodecCtxAudio->codec_id);

    if (mCodecAudio != NULL) {
        ALOGI("audio codec id is %d, name is %s\n",mCodecAudio->id,mCodecAudio->name);
    } else {
        ALOGE("cannot find audio codec\n");
    }

    if (avcodec_open2(mCodecCtxAudio,mCodecAudio,NULL) < 0) {
        ALOGE("cannot bind mCodecAudio to mCodecCtxAudio\n");
    }

    ALOGI("bit rate is %ld \n",mCodecCtxAudio->bit_rate);
    ALOGI("saple rate = %d\n",mCodecCtxAudio->sample_rate);
    ALOGI("channels = %d\n",mCodecCtxAudio->channels);
    ALOGI("block_align is %d \n",mCodecCtxAudio->block_align);

    uint8_t *audio_pkt_data = NULL;
    int audio_pkt_size = 0;
    int output_size = MAX_AUDIO_FRAME_SIZE * 100;
    uint8_t *audio_buf = NULL;
    int got_frame = -1;
    int packet_num = 0;

    SwrContext *swr_ctx = NULL;
    //enum AVSampleFormat dst_format = AV_SAMPLE_FMT_S16;

    mFrameAudio = av_frame_alloc();
    audio_buf = (uint8_t *) malloc (sizeof(MAX_AUDIO_FRAME_SIZE * 3 / 2));

    while (true) {
        ret = av_read_frame(mFmtCtxAudio,&mPacketAudio);

        if (ret == AVERROR_EOF) {
            ALOGI("read end \n");
            break;
        } else if (ret < 0) {
            ALOGI("read frame end\n");
        }
        packet_num++;
        if (mPacketAudio.stream_index == audio_index) {

                ret = avcodec_send_packet(mCodecCtxAudio, &mPacketAudio);
                if (ret < 0 ) {
                    ALOGE("send packet error\n");
                    exit(1);
                }

                ret = avcodec_receive_frame(mCodecCtxAudio, mFrameAudio);
                if (ret < 0 ) {
                    ALOGE("receive packet error,ret = %d\n",ret);
                }
                //printf("decodec one packet\n");
                //fwrite(mFrameAudio->data,1,mFrameAudio->side_data,mAudioPCM);
#if 0
                if (mFrameAudio->channels > 0 && mFrameAudio->channel_layout == 0) {
                    mFrameAudio->channel_layout = av_get_default_channel_layout(mFrameAudio->channels);
                } else if (mFrameAudio->channels == 0 && mFrameAudio->channel_layout > 0) {
                    mFrameAudio->channels = av_get_channel_layout_nb_channels(mFrameAudio->channel_layout);
                }
                //dst_format = AV_SAMPLE_FMT_S16;//av_get_packed_sample_fmt((AVSampleFormat)frame->format);
                uint64_t dst_layout = av_get_default_channel_layout(mFrameAudio->channels);

                swr_ctx = swr_alloc_set_opts(NULL, dst_layout, AV_SAMPLE_FMT_S16, mFrameAudio->sample_rate,
                    mFrameAudio->channel_layout, mFrameAudio->format, mFrameAudio->sample_rate, 0, NULL);

                if (!swr_ctx || swr_init(swr_ctx) < 0) {
                    exit(1);
                }

                int dst_nb_samples = av_rescale_rnd(swr_get_delay(swr_ctx, mFrameAudio->sample_rate) + mFrameAudio->nb_samples,
                                                                                    mFrameAudio->sample_rate, mFrameAudio->sample_rate, AVRounding(1));
                int nb = swr_convert(swr_ctx, &audio_buf, dst_nb_samples, (const uint8_t**)mFrameAudio->data, mFrameAudio->nb_samples);
                int data_size = mFrameAudio->channels * nb * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
#endif
                ALOGI("mFrameAudio linesize = %d, format = %d\n",mFrameAudio->linesize[0],mFrameAudio->format);
                int data_size = mFrameAudio->channels * mFrameAudio->nb_samples * av_get_bytes_per_sample(mFrameAudio->format);
                ALOGI("data size = %d\n",data_size);
                if (mFrameAudio->format >= 0) {
                    fwrite(mFrameAudio->data,1,data_size,mAudioPCM);
                }
                if (*(mFrameAudio->data) == NULL) {
                    ALOGE("data is NULL\n");
                }
            }

        av_free_packet(&mPacketAudio);

    }
            ALOGI("total has %d packet\n",packet_num);
}

void init_audio_en() {
    ALOGI("init\n");
    av_register_all();

    mAudioOut = fopen(MP3_OUT,"wb+");

    if (!mAudioOut) {
        ALOGE("open %s failed",MP3_OUT);
    }

    mAudioIn = fopen(PCM_OUT,"rb+");

    if (!mAudioIn) {
        ALOGE("open %s failed",PCM_OUT);
    }
}

void encoder_audio() {
    uint8_t *frame_buf = NULL;
    int got_frame = 0;
    int ret = 0;
    int size = 0;
    int frame_num = 100;


    // 获取AVFormatContext以及outputformat的两种方法
    // 方法一：
    // avformat_alloc_output_context2(&mFmtCtxAudioEn,NULL,NULL,MP3_OUT);
    // mOutFmtAndioEN = mFmtCtxAudioEn->oformat;

    // 方法二：
    mFmtCtxAudioEn = avformat_alloc_context();
    mOutFmtAndioEN = av_guess_format(NULL,MP3_OUT,NULL);
    mFmtCtxAudioEn->oformat = mOutFmtAndioEN;
    ALOGI("get outputformat\n");

    // 打开输出文件
    if (avio_open(&mFmtCtxAudioEn->pb,MP3_OUT,AVIO_FLAG_READ_WRITE) < 0) {
        ALOGE("avio_open failed\n");
        return;
    }

    // 创建音频流
    mAudioStreamEn = avformat_new_stream(mFmtCtxAudioEn,0);
    if (!mAudioStreamEn) {
        ALOGE("create stream failed\n");
    }

    // 配置音频数据
    mCodecCtxAudioEn = mAudioStreamEn->codec;
    mCodecCtxAudioEn->codec_id = mOutFmtAndioEN->audio_codec;
    mCodecCtxAudioEn->codec_type = AVMEDIA_TYPE_AUDIO;
    mCodecCtxAudioEn->sample_fmt = AV_SAMPLE_FMT_S16P;
    mCodecCtxAudioEn->sample_rate = 44100;
    mCodecCtxAudioEn->channel_layout = AV_CH_LAYOUT_STEREO;
    mCodecCtxAudioEn->channels = av_get_channel_layout_nb_channels(mCodecCtxAudioEn->channel_layout);
    mCodecCtxAudioEn->bit_rate = 320000;

    ALOGI("mCodecCtxAudioEn = %p, codec_id= %d,  codec_type = %d, sample_fmt = %d,channels = %d\n",
            mCodecCtxAudioEn,mCodecCtxAudioEn->codec_id, mCodecCtxAudioEn->codec_type,AV_SAMPLE_FMT_S16P,mCodecCtxAudioEn->channels);

    av_dump_format(avformat_alloc_output_context2,0,MP3_OUT,1);


    // 匹配codec，mp3 encoder需要三方库支持
    mCodecAudioEn = avcodec_find_encoder(mCodecCtxAudioEn->codec_id);

    if (!mCodecAudioEn) {
        ALOGE("find encoder failed,codec id = %d\n",mCodecCtxAudioEn->codec_id);
    }

    if (avcodec_open2(mCodecCtxAudioEn,mCodecAudioEn,NULL) < 0) {
        ALOGE("open encoder failed\n");
    }

    // 为frame 以及 packet分配相应的内存以及初始化
    mFrameAudioEn = av_frame_alloc();
    mFrameAudioEn->nb_samples = mCodecCtxAudioEn->frame_size;
    mFrameAudioEn->format = mCodecCtxAudioEn->sample_fmt;

    size = av_samples_get_buffer_size(NULL,mCodecCtxAudioEn->channels,mCodecCtxAudioEn->frame_size,mCodecCtxAudioEn->sample_fmt,1);
    frame_buf = (uint8_t *)av_malloc(size);
    avcodec_fill_audio_frame(mFrameAudioEn,mCodecCtxAudioEn->channels,mCodecCtxAudioEn->sample_fmt,(const uint8_t*)frame_buf,size,1);

    ALOGI("avcodec_fill_audio_frame done,size = %d \n", size);
    avformat_write_header(mFmtCtxAudioEn,NULL);

    ALOGI("avformat_write_header done\n");
    av_new_packet(&mPacketAudioEn,size);


    // 开始编码
    for (int i = 1; ; i++) {
        ALOGI("i = %d\n",i);
        if (fread(frame_buf,1,size,mAudioIn) < 0) {
            ALOGE("read audio  failed\n");
            break;
        } else if (feof(mAudioIn)) {
            ALOGI("read all audio \n");
            break;
        }

        mFrameAudioEn->data[0] = frame_buf;
        mFrameAudioEn->pts = 100 * i;

        got_frame = 0;

        ret = avcodec_encode_audio2(mCodecCtxAudioEn,&mPacketAudioEn,mFrameAudioEn,&got_frame);

        if (ret < 0 ) {
            ALOGE("encode failed\n");
            return -1;
        }

        if (got_frame == 1) {
            ALOGI("got 1 audio frame\n");
            mPacketAudioEn.stream_index = mAudioStreamEn->index;
            ret = av_write_frame(mFmtCtxAudioEn,&mPacketAudioEn);
            av_free_packet(&mPacketAudioEn);
        }
    }

    // 写入输出文件结尾部分，ts格式不需要该操作
    av_write_trailer(mFmtCtxAudioEn);


    if (mAudioStreamEn) {
        avcodec_close(mAudioStreamEn->codec);
        av_free(frame_buf);
        av_free(mFrameAudioEn);
    }

    // 释放相应资源
    avio_close(mFmtCtxAudioEn->opaque);
    avformat_free_context(mFmtCtxAudioEn);

    fclose(mAudioOut);
    fclose(mAudioIn);
}


void muxer() {

// video
    ALOGI("enter muxer video \n");
    int ret = -1;
    int in_width = mCodecCtxPic->width;
    int in_height = mCodecCtxPic->height;
    int y_size = in_width * in_height;
    avformat_alloc_output_context2(&mAVFmtCtx,NULL,NULL,OUT_TS);
    mOutFmt= mAVFmtCtx->oformat;
    mOutFmt->audio_codec = 86017;

   ALOGI("LINE = %d\n", __LINE__);
    ret = avio_open(&mAVFmtCtx->pb,OUT_TS,AVIO_FLAG_READ_WRITE);
    if (ret < 0) {
        ALOGE("avio_open fail \n");
        exit(1);
    }
    ALOGI("LINE = %d\n", __LINE__);
    mViderStream = avformat_new_stream(mAVFmtCtx,0);
    if (!mViderStream) {
        ALOGE("mViderStream == NULL\n");
        exit(1);
    }
    mCodecCtxEncoder = mViderStream->codec;
    mCodecCtxEncoder->codec_id = mOutFmt->video_codec;
    mCodecCtxEncoder->codec_type = AVMEDIA_TYPE_VIDEO;
    mCodecCtxEncoder->pix_fmt = AV_PIX_FMT_YUV420P;
    mCodecCtxEncoder->width = in_width;
    mCodecCtxEncoder->height = in_height;
    mCodecCtxEncoder->bit_rate = 400000;
    mCodecCtxEncoder->gop_size = 250;
    mCodecCtxEncoder->time_base.num = 1;
    mCodecCtxEncoder->time_base.den = 25;

    mCodecCtxEncoder->qmin =10;
    mCodecCtxEncoder->qmax = 51;
    mCodecCtxEncoder->max_b_frames = 3;
    ALOGI("LINE = %d\n", __LINE__);

    // audio
    // 创建音频流
    mAudioStreamEn = avformat_new_stream(mAVFmtCtx,0);
    if (!mAudioStreamEn) {
        ALOGE("create stream failed\n");
    }
     ALOGI("LINE = %d\n", __LINE__);
    // 配置音频数据
    mCodecCtxAudioEn = mAudioStreamEn->codec;
    mCodecCtxAudioEn->codec_id = mOutFmt->audio_codec;
    mCodecCtxAudioEn->codec_type = AVMEDIA_TYPE_AUDIO;
    mCodecCtxAudioEn->sample_fmt = AV_SAMPLE_FMT_S16P;
    mCodecCtxAudioEn->sample_rate = 44100;
    mCodecCtxAudioEn->channel_layout = AV_CH_LAYOUT_STEREO;
    mCodecCtxAudioEn->channels = av_get_channel_layout_nb_channels(mCodecCtxAudioEn->channel_layout);
    mCodecCtxAudioEn->bit_rate = 320000;

    ALOGI("mCodecCtxAudioEn = %p, codec_id= %d,  codec_type = %d, sample_fmt = %d,channels = %d\n",
            mCodecCtxAudioEn,mCodecCtxAudioEn->codec_id, mCodecCtxAudioEn->codec_type,AV_SAMPLE_FMT_S16P,mCodecCtxAudioEn->channels);


    AVDictionary *param = 0;

    ALOGI("mCodecCtxEncoder codec id = %d\n",mCodecCtxEncoder->codec_id);
    av_dump_format(mAVFmtCtx, 0, OUT_TS, 1);

    mCodecEncoder = avcodec_find_encoder(mCodecCtxEncoder->codec_id);

    if (!mCodecEncoder) {
        ALOGE("cannot find encoder\n");
        exit(1);
    }
    ALOGI("LINE = %d\n", __LINE__);
    ret = avcodec_open2(mCodecCtxEncoder,mCodecEncoder, NULL);
    if (ret < 0) {
        ALOGE("cannot open encoder\n");
        exit(1);
    }
    ALOGI("LINE = %d\n", __LINE__);
    mFrameEncoder = av_frame_alloc();
    if (!mFrameEncoder) {
        ALOGE("mFrameEncoder == NULL\n");
        exit(1);
    }
    ALOGI("LINE = %d\n", __LINE__);

    mPictureSize = avpicture_get_size(mCodecCtxEncoder->pix_fmt,mCodecCtxEncoder->width,mCodecCtxEncoder->height);
    mPictureBuffer = (uint8_t *)av_malloc(mPictureSize);
    ALOGI("LINE = %d\n", __LINE__);

    avpicture_fill((AVPicture *)mFrameEncoder,mPictureBuffer,mCodecCtxEncoder->pix_fmt,mCodecCtxEncoder->width,mCodecCtxEncoder->height);
    ALOGI("LINE = %d\n", __LINE__);

    avformat_write_header(mAVFmtCtx,NULL);
    ALOGI("LINE = %d\n", __LINE__);

    av_new_packet(&mPacketEncoder,mPictureSize);
    ALOGI("LINE = %d\n", __LINE__);

    y_size = mCodecCtxEncoder->width * mCodecCtxEncoder->height;
    int size = 0;
    int frame_num = 0;
    int frame_encoder_num = 0;
    int frames = 0;
    ALOGI("LINE = %d\n", __LINE__);
    for (frames = 0; frames < 100; frames++) {

        //ALOGI("do one encoder\n");
        mFrameEncoder->data[0] = mFramePicYUV->data[0];
        mFrameEncoder->data[1] = mFramePicYUV->data[1];
        mFrameEncoder->data[2] = mFramePicYUV->data[2];

        mFrameEncoder->format = 0;
        mFrameEncoder->width = in_width;
        mFrameEncoder->height = in_height;

        mFrameEncoder->pts = frames * mViderStream->time_base.den / ((mViderStream->time_base.num) * 25);

        int got_pic = 0;

        //ALOGI("before encode\n");
        ret = avcodec_encode_video2(mCodecCtxEncoder,&mPacketEncoder,mFrameEncoder,&got_pic);
        //ALOGI("after encode\n");
        if (ret < 0 ) {
            ALOGE("encode fail\n");
            exit(1);
        }
        if (1 == got_pic) {
            frame_encoder_num++;
            mPacketEncoder.stream_index = mViderStream->index;
            av_write_frame(mAVFmtCtx,&mPacketEncoder);
            av_free_packet(&mPacketEncoder);
        }
    }
    ALOGI("total encode %d frames\n",frame_encoder_num); 

// audio
    uint8_t *frame_buf = NULL;
    int got_frame = 0;
//    int ret = 0;
//    int size = 0;
//    int frame_num = 100;


    // 获取AVFormatContext以及outputformat的两种方法
    // 方法一：
    // avformat_alloc_output_context2(&mFmtCtxAudioEn,NULL,NULL,MP3_OUT);
    // mOutFmtAndioEN = mFmtCtxAudioEn->oformat;

    // 方法二：
    //mFmtCtxAudioEn = avformat_alloc_context();
   // mOutFmtAndioEN = av_guess_format(NULL,MP3_OUT,NULL);
    //mFmtCtxAudioEn->oformat = mOutFmtAndioEN;
    ALOGI("get outputformat\n");

    // 打开输出文件
    //if (avio_open(&mFmtCtxAudioEn->pb,MP3_OUT,AVIO_FLAG_READ_WRITE) < 0) {
    //    ALOGE("avio_open failed\n");
    //    return;
    //}



    //av_dump_format(avformat_alloc_output_context2,0,MP3_OUT,1);


    // 匹配codec，mp3 encoder需要三方库支持
    mCodecAudioEn = avcodec_find_encoder(mCodecCtxAudioEn->codec_id);
    
    if (!mCodecAudioEn) {
        ALOGE("find encoder failed,codec id = %d\n",mCodecCtxAudioEn->codec_id);
    }

    if (avcodec_open2(mCodecCtxAudioEn,mCodecAudioEn,NULL) < 0) {
        ALOGE("open encoder failed\n");
    }

    // 为frame 以及 packet分配相应的内存以及初始化
    mFrameAudioEn = av_frame_alloc();
    mFrameAudioEn->nb_samples = mCodecCtxAudioEn->frame_size;
    mFrameAudioEn->format = mCodecCtxAudioEn->sample_fmt;

    size = av_samples_get_buffer_size(NULL,mCodecCtxAudioEn->channels,mCodecCtxAudioEn->frame_size,mCodecCtxAudioEn->sample_fmt,1);
    frame_buf = (uint8_t *)av_malloc(size);
    avcodec_fill_audio_frame(mFrameAudioEn,mCodecCtxAudioEn->channels,mCodecCtxAudioEn->sample_fmt,(const uint8_t*)frame_buf,size,1);

    ALOGI("avcodec_fill_audio_frame done,size = %d \n", size);
    //avformat_write_header(mAVFmtCtx,NULL);

    ALOGI("avformat_write_header done\n");
    av_new_packet(&mPacketAudioEn,size);

    ALOGI("pts = %d\n",size* 1000  /(44100 * 2 *2));

    // 开始编码
    for (int i = 1; ; i++) {
        //ALOGI("i = %d\n",i);
        if (fread(frame_buf,1,size,mAudioIn) < 0) {
            ALOGE("read audio  failed\n");
            break;
        } else if (feof(mAudioIn)) {
            ALOGI("read all audio ,total frame is %d\n", i);
            break;
        }

        mFrameAudioEn->data[0] = frame_buf;
        mFrameAudioEn->pts = 26 * i;

        got_frame = 0;

        ret = avcodec_encode_audio2(mCodecCtxAudioEn,&mPacketAudioEn,mFrameAudioEn,&got_frame);

        if (ret < 0 ) {
            ALOGE("encode failed\n");
            return -1;
        }

        if (got_frame == 1) {
            //ALOGI("got 1 audio frame\n");
            mPacketAudioEn.stream_index = mAudioStreamEn->index;
            ret = av_write_frame(mAVFmtCtx,&mPacketAudioEn);
            av_free_packet(&mPacketAudioEn);
        }
    }

    // 写入输出文件结尾部分，ts格式不需要该操作
    av_write_trailer(mAVFmtCtx);


    if (mAudioStreamEn) {
        avcodec_close(mAudioStreamEn->codec);
        av_free(frame_buf);
        av_free(mFrameAudioEn);
    }

    // 释放相应资源
    avio_close(mAVFmtCtx->opaque);
    avformat_free_context(mAVFmtCtx);

    //fclose(mAudioOut);
    fclose(mAudioIn);

}
void my_logoutput(void* ptr, int level, const char* fmt,va_list vl) {
    FILE *fp = fopen("my_log.txt","a+");
    if (fp) {
        vfprintf(fp,fmt,vl);
        fflush(fp);
        fclose(fp);
    }
}

void drawMainUI() {
    system("clear");
    printf("*************************************************************************\n");
    printf("*  当前设置：图片文件: %s                                                \n",PICTURE_SOURCE);
    printf("*            音乐文件: %s                                                \n",AUDIO_SOURCE);
    printf("*            生成文件位置: %s                                            \n",OUT_TS);
    printf("*                                                                        \n");
    printf("*************************************************************************\n");
    printf("*  功能列表                                                              \n");
    printf("*  1.设置图片位置                     2.查看图片信息                     \n");
    printf("*  3.设置音乐位置                     4.查看音乐信息                     \n");
    printf("*  5.开始合成                                                            \n");
    printf("*  0.退出                                                                \n");
    printf("*************************************************************************\n");
//    printf("*  请输入操作指令：");

}

void drawPicInfo() {
    system("clear");
    init_pic();
    decoder_pic();
    printf("*************************************************************************\n");
    printf("图片编码器为：%d\n",mCodecCtxPic->codec_id);
    printf("图片尺寸为：%d x %d\n",mCodecCtxPic->width,mCodecCtxPic->height);
    printf("*************************************************************************\n");
    getchar();
}


void drawAudioInfo() {
    system("clear");
    init_aud();
    decoder_audio();
    printf("*************************************************************************\n");
    printf("音频编码器为：%d\n",mCodecCtxAudio->codec_id);
    printf("bit rate is %ld \n",mCodecCtxAudio->bit_rate);
    printf("saple rate = %d\n",mCodecCtxAudio->sample_rate);
    printf("channels = %d\n",mCodecCtxAudio->channels);
    printf("block_align is %d \n",mCodecCtxAudio->block_align);
    getchar();
}


void drawInputError() {
     drawMainUI();
     printf("\n");
     printf("请输入功能菜单中的选项\n");
}

void drawFileError() {
    drawMainUI();
    printf("\n");
    printf("请输入正确的文件路径\n");
    getchar();
}

void drawFileSucceed() {
    drawMainUI();
    printf("\n");
    printf("设置文件成功\n");
    getchar();
}


void changePicPath() {
    char *path = NULL;
    FILE *file = NULL;
    drawMainUI();
    printf("\n");
    printf("请输入图片文件路径:\n");
    scanf("%s\n",path);

    file = fopen(path,"wb+");
    if (file == NULL) {
        drawFileError();
    } else {
        drawFileSucceed();
        fclose(path);
    }

}

void changeAudioPath() {
    char *path = NULL;
    FILE *file = NULL;

    drawMainUI();
    printf("\n");
    printf("请输入音频文件路径:\n");
    scanf("%s\n",path);

    file = fopen(path,"wb+");
    if (file == NULL) {
        drawFileError();
    } else {
        drawFileSucceed();
        fclose(path);
    }
}

int main() {
    char option;
    av_log_set_level(AV_LOG_QUIET);
    drawMainUI();

    option = getchar();
    getchar();
    while (option != '0') {
        switch(option) {
            case '1':
                changePicPath();
                break;
            case '2':
                drawPicInfo();
                break;
            case '3':
                changeAudioPath();
                break;
            case '4':
                drawAudioInfo();
                break;
            case '5':
                printf("初始化图片中\n");
                init_pic();
                printf("初始化图片完成，解码图片中\n");
                decoder_pic();
                printf("解码图片完成，初始化音频中\n");
                init_audio_en();
                printf("初始化音频完成，视频生成中\n");
                muxer();
                printf("视频已生成\n");
                break;
            case '0':
                exit(1);
                break;
            default:
                drawInputError();
        }

        option = getchar();
        getchar();
    }

    //printf("get %c\n",option);
    
    //init();
    //decoder_video();


    //init_encoder();
    //encoder_video();

    //init_pic();
    //decoder_pic();

    //pic_to_video();

    //init_aud();
    //decoder_audio();

    //init_audio_en();
    //encoder_audio();

    //muxer();
    
    //relese_buffer();

    return 0;
}
