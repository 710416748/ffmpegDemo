#include "ffmpeg.h"

#include <stdio.h>

#ifndef _SYS_LOG_
#define _SYS_LOG_

#include <libavutil/log.h>

#define ALOGD(format, ...) av_log(NULL, AV_LOG_DEBUG, format, ##__VA_ARGS__);
#define ALOGV(format, ...) av_log(NULL, AV_LOG_VERBOSE, format, ##__VA_ARGS__);
#define ALOGI(format, ...) av_log(NULL, AV_LOG_INFO, format, ##__VA_ARGS__);
#define ALOGW(format, ...) av_log(NULL, AV_LOG_WARNING, format, ##__VA_ARGS__);
#define ALOGE(format, ...) av_log(NULL, AV_LOG_ERROR, format, ##__VA_ARGS__);

#endif

void my_logoutput(void* ptr, int level, const char* fmt,va_list vl);
