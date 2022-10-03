#ifndef CODECS_H
#define CODECS_H

enum codec_type {
    CODEC_TYPE_UNKNOWN = 0,
    CODEC_TYPE_AAC = 1, /* strictly AAC-LC */
    CODEC_TYPE_ALAC = 2,
    CODEC_TYPE_FLAC = 3,
    CODEC_TYPE_USAC = 4,
};

typedef enum codec_type codec_type;

#endif
