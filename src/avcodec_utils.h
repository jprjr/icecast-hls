#ifndef AVCODEC_UTILS
#define AVCODEC_UTILS

#include "codecs.h"
#include <libavcodec/avcodec.h>

#ifdef __cplusplus
extern "C" {
#endif

enum AVCodecID codec_to_avcodec(codec_type c);
codec_type avcodec_to_codec(enum AVCodecID id);

#ifdef __cplusplus
}
#endif

#endif
