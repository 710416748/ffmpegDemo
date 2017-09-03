#include "include/main.h"

#define VIDEO_SOURCE "../1.mp4"
#define AUDIO_SOURCE "../1.mp3"
#define PICTURE_SOURCE "../1.jpg"
#define YUV_SOURCE "../output.yuv"
#define OUT_TS "../out.mp4"
#define PIC_OUT "../pic.yuv"
#define PCM_OUT "../outAudio.pcm"

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
AVCodec             *mCodecAudio = NULL;
AVCodecContext  *mCodecCtxAudio = NULL;
AVFrame             *mFrameAudio = NULL;
AVPacket              mPacketAudio;


struct SwsContext *img_convert_ctx = NULL; //图像处理的上下文

#ifdef DUMP_IN
FILE            *mYuvFd = NULL;                   //输出文件
#endif
FILE            *mYuvInFd = NULL;
FILE            *mEncodeOutFd = NULL;

FILE            *mPicInFd = NULL;
FILE            *mPicOutFd = NULL;

FILE            *mAudioPCM = NULL;
//FILE            *mAduioIn = NULL;

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
        printf("open input fail\n");
        exit(1);
    }
        ret = avformat_find_stream_info(mFmtCtxPic,NULL);
    if (ret < 0 ) {
        printf("find stream info error\n");
        exit(1);
    }
    printf("nb_streams = %u,codec type = %d\n",mFmtCtxPic->nb_streams,(mFmtCtxPic->streams)[0]->codec->codec_type);
    mCodecCtxPic = (mFmtCtxPic->streams)[0]->codec;

    if (!mCodecCtxPic) {
        printf("mCodecCtxPic is NULL\n");
        exit(1);
    }

    mCodecPic = avcodec_find_decoder((mFmtCtxPic->streams)[0]->codec->codec_id);
    if (!mCodecPic) {
        printf("can not find decoder\n");
        exit(1);
    }

    ret = avcodec_open2(mCodecCtxPic,mCodecPic,NULL);

    if (ret < 0) {
        printf("open encoder fail\n");
        exit(1);
    }
    printf("picture codec id = %d,type= %d\n",mCodecCtxPic->codec_id,mCodecCtxPic->codec_type);

    mBufferPic = (unsigned char *)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUV420P,
                                    mCodecCtxPic->width, mCodecCtxPic->height,1));

    av_image_fill_arrays(mFramePicYUV->data, mFramePicYUV->linesize,mBufferPic,
        AV_PIX_FMT_YUV420P,mCodecCtxPic->width, mCodecCtxPic->height,1);

    y_size = mCodecCtxPic->width * mCodecCtxPic->height;
    printf("width = %d,height = %d\n",mCodecCtxPic->width,mCodecCtxPic->height);

    img_convert_ctx = sws_getContext(mCodecCtxPic->width, mCodecCtxPic->height, mCodecCtxPic->pix_fmt,
        mCodecCtxPic->width, mCodecCtxPic->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);

    pic_size = avpicture_get_size(mCodecCtxPic->pix_fmt,mCodecCtxPic->width,mCodecCtxPic->height);
   // av_new_packet(&mPacketPIC,pic_size);

    ret = av_read_frame(mFmtCtxPic,&mPacketPIC);


    if (ret != 0) {
        printf("read frame fail\n");
        exit(1);
    }


     ret = avcodec_decode_video2(mCodecCtxPic,mFramePic,&got_frame,&mPacketPIC);

     if (ret > 0) {
        printf("ret = %d,got_frame = %d\n",ret,got_frame);
        sws_scale(img_convert_ctx, (const unsigned char* const*)mFramePic->data, mFramePic->linesize, 0,
                        mCodecCtxPic->height,mFramePicYUV->data, mFramePicYUV->linesize);

        fwrite(mFramePicYUV->data[0],1,y_size,mPicOutFd);    //Y
        fwrite(mFramePicYUV->data[1],1,y_size/4,mPicOutFd);  //U
        fwrite(mFramePicYUV->data[2],1,y_size/4,mPicOutFd);  //V
    } else if (ret == 0) {
        printf("read zero frame\n");
    } else {
        printf("ret = %d,got_frame = %d\n",ret,got_frame);
        printf("decode frame fail\n");
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
        printf("avio_open fail \n");
        exit(1);
    }

    mViderStream = avformat_new_stream(mFmtCtxEncoder,0);
    if (!mViderStream) {
        printf("mViderStream == NULL\n");
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
    printf("total encode %d frames\n",frame_encoder_num);
    av_write_trailer(mFmtCtxEncoder);
}

void init_aud() {
    av_register_all();

    mAudioPCM = fopen(PCM_OUT,"wb+");

    if (!mAudioPCM) {
        printf("open file failed");
    }
}

void decoder_audio() {
    int i = 0;
    int ret = -1;
    int audio_index = -1;

    if ( avformat_open_input(&mFmtCtxAudio,AUDIO_SOURCE,NULL,NULL) < 0) {
        printf("open audio source failed\n");
    }

    if (avformat_find_stream_info(mFmtCtxAudio,NULL) < 0) {
        printf("find stream info failed\n");
    }

    for (i = 0; i < mFmtCtxAudio->nb_streams; i++) {
        if (mFmtCtxAudio->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_index = i;
            printf("audio index is %d\n",i);
            break;
        }
    }

    if (audio_index == -1) {
        printf("cannot find audio stream \n");
        exit(1);
    }

    mCodecCtxAudio = mFmtCtxAudio->streams[audio_index]->codec;

    mCodecAudio = avcodec_find_decoder(mCodecCtxAudio->codec_id);

    if (mCodecAudio != NULL) {
        printf("audio codec id is %d, name is %s\n",mCodecAudio->id,mCodecAudio->name);
    } else {
        printf("cannot find audio codec\n");
    }

    if (avcodec_open2(mCodecCtxAudio,mCodecAudio,NULL) < 0) {
        printf("cannot bind mCodecAudio to mCodecCtxAudio\n");
    }

    printf("bit rate is %ld \n",mCodecCtxAudio->bit_rate);
    printf("saple rate = %d\n",mCodecCtxAudio->sample_rate);
    printf("channels = %d\n",mCodecCtxAudio->channels);
    printf("block_align is %d \n",mCodecCtxAudio->block_align);

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
            printf("read end \n");
            break;
        } else if (ret < 0) {
            printf("read frame end\n");
        }
        packet_num++;
        if (mPacketAudio.stream_index == audio_index) {

                ret = avcodec_send_packet(mCodecCtxAudio, &mPacketAudio);
                if (ret < 0 ) {
                    printf("send packet error\n");
                    exit(1);
                }

                ret = avcodec_receive_frame(mCodecCtxAudio, mFrameAudio);
                if (ret < 0 ) {
                    printf("receive packet error,ret = %d\n",ret);
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
                printf("mFrameAudio linesize = %d, format = %d\n",mFrameAudio->linesize[0],mFrameAudio->format);
                int data_size = mFrameAudio->channels * mFrameAudio->nb_samples * av_get_bytes_per_sample(mFrameAudio->format);
                printf("data size = %d\n",data_size);
                if (mFrameAudio->format >= 0) {
                    fwrite(mFrameAudio->data,1,data_size,mAudioPCM);
                }
                if (*(mFrameAudio->data) == NULL) {
                    printf("data is NULL\n");
                }
            }

        av_free_packet(&mPacketAudio);

    }
            printf("total has %d packet\n",packet_num);
}

int main() {

    //init();
    //decoder_video();


    //init_encoder();
    //encoder_video();

    //init_pic();
    //decoder_pic();

    //pic_to_video();

    init_aud();
    decoder_audio();

    relese_buffer();

    return 0;
}
