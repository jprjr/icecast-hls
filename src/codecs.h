#ifndef CODECS_H
#define CODECS_H

enum codec_type {
    CODEC_TYPE_UNKNOWN  = 0,
    CODEC_TYPE_PRIVATE, /* used by avformat/avcodec if it's not one of the codecs below */
    CODEC_TYPE_AAC,
    CODEC_TYPE_ALAC,
    CODEC_TYPE_FLAC,
    CODEC_TYPE_MP3,
    CODEC_TYPE_OPUS,
    CODEC_TYPE_AC3,
    CODEC_TYPE_EAC3,
};

typedef enum codec_type codec_type;

#define CODEC_PROFILE_AAC_LC    2
#define CODEC_PROFILE_AAC_HE    5
#define CODEC_PROFILE_AAC_HE2  29
#define CODEC_PROFILE_AAC_LAYER3  34
#define CODEC_PROFILE_AAC_USAC 42

const char* codec_name(codec_type t);

#endif
