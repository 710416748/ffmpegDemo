#include "include/main.h"

#define VIDEO_SOURCE "1.mp4"
#define AUDIO_SOURCE "1.mp3"
#define PICTURE_SOURCE "1.jpg"

//#define DUMP_IN 1

AVCodec         *mCodec = NULL;            //编解码器
AVFormatContext *mFormatContext = NULL;    //贯穿ffmpeg的上下文
AVIOContext     *mIOContext = NULL;        //输入设备上下文，ffmpeg将文件也视为一种协议
AVFrame         *mFrame = NULL;            //解码后的原始帧数据
AVFrame         *mFrameYUV = NULL;         //原始帧数据转化后的YUV数据
AVPacket        mPacket;                   //解码前的帧数据
AVInputFormat   *mInputFormat = NULL;      //输入格式的上下文
AVCodecContext  *mCodecContext = NULL;     //编解码器的上下文

struct SwsContext *img_convert_ctx = NULL; //图像处理的上下文

#ifdef DUMP_IN
FILE            *mYuvFd;                   //输出文件
#endif

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
    ret = avformat_open_input(&mFormatContext,VIDEO_SOURCE,NULL,NULL);
    if (ret < 0) {
        printf("open failed\n");
    } else {
        // 获取流的信息赋值给AVFormatContext
        ret = avformat_find_stream_info(mFormatContext,NULL);
        if (ret < 0 ) {
            printf("find stream info error\n");
	    exit(1);
	}
        
	//获取部分AVInputFormat信息
        //mInputFormat = mFormatContext->iformat;
        //if (mInputFormat == NULL) {
        //    printf("iformat == NULL\n");
        //} else {
        //    printf("iformat name is %s, codec id is =%d\n",mInputFormat->name,mInputFormat->raw_codec_id);
        //    if(!mInputFormat->codec_tag) {
        //        printf("codec tag is NULL\n");
        //    }
    }
    //mIOContext = mFormatContext->opaque;

    printf("nb_streams = %u\n",mFormatContext->nb_streams);

    for (i = 0; i < mFormatContext->nb_streams; i++) {
        printf("AVStream[%d]:nb_frames = %ld\n",i,(*(mFormatContext->streams + i))->nb_frames);

        //判断是否为视频信息
        if ((mFormatContext->streams)[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            //获取编解码器上下文
            mCodecContext = mFormatContext->streams[i]->codec;

            video_index = i;
            if (mCodecContext->codec_name) {
                printf("codec name = %s, codec id = %d\n",mCodecContext->codec_name,mCodecContext->codec_id);

	        //根据编码器id找到decodeer
                mCodec = avcodec_find_decoder(mCodecContext->codec_id);
                if(!mCodec) {
	            printf("Cannot find codec\n");
	        }

                //打开解码器
                if (avcodec_open2(mCodecContext,mCodec,NULL) < 0 ) {
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
        ret = av_read_frame(mFormatContext,&mPacket);
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


//for (mCodec = av_codec_next(avcodec); mCodec != NULL; mCodec = av_codec_next(avcodec)) {

//    printf("first codec short name is %s,\tlong name is %s\n",
//                            mCodec->name,mCodec->long_name);
//}

}

void init() {
#ifdef DUMP_IN
    mYuvFd = fopen("output.yuv","wb+");
#endif
    //注册各种编解码器等
    av_register_all();

    //为avformat分配空间和部分参数初始化
    mFormatContext = avformat_alloc_context();
    if (mFormatContext == NULL) {
        printf("init AVFormatContext == NULL,exit\n");
        exit(1);
    }

    // mFormatContext->pb为AVIOContext数据，输入文件的协议，其成员opaque在ffmpeg源码内部使用
    //if (mFormatContext->pb == NULL) {
    //    printf("AVIOContext == NULL\n");
    //    avio_alloc_context(mFormatContext->pb);
    //}

    //mIOContext = mFormatContext->pb;

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
    avformat_free_context(mFormatContext);
#ifdef DUMP_IN
    fclose(mYuvFd);
#endif

}

int main() {

    init();

    decoder_video();

    relese_buffer();
    return 0;
}
