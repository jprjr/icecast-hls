#include "avcodec_utils.h"

enum AVCodecID codec_to_avcodec(codec_type c) {
    switch(c) {
        case CODEC_TYPE_AAC: return AV_CODEC_ID_AAC;
        case CODEC_TYPE_ALAC: return AV_CODEC_ID_ALAC;
        case CODEC_TYPE_FLAC: return AV_CODEC_ID_FLAC;
        case CODEC_TYPE_MP3: return AV_CODEC_ID_MP3;
        case CODEC_TYPE_OPUS: return AV_CODEC_ID_OPUS;
        case CODEC_TYPE_AC3: return AV_CODEC_ID_AC3;
        case CODEC_TYPE_EAC3: return AV_CODEC_ID_EAC3;
        default: break;
    }
    return AV_CODEC_ID_NONE;
}

codec_type avcodec_to_codec(enum AVCodecID id) {
    switch(id) {
        case AV_CODEC_ID_AAC: return CODEC_TYPE_AAC;
        case AV_CODEC_ID_ALAC: return CODEC_TYPE_ALAC;
        case AV_CODEC_ID_FLAC: return CODEC_TYPE_FLAC;
        case AV_CODEC_ID_MP3: return CODEC_TYPE_MP3;
        case AV_CODEC_ID_OPUS: return CODEC_TYPE_OPUS;
        case AV_CODEC_ID_AC3: return CODEC_TYPE_AC3;
        case AV_CODEC_ID_EAC3: return CODEC_TYPE_EAC3;
        default: break;
    }
    return CODEC_TYPE_PRIVATE;
}
