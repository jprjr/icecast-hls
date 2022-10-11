#include "codecs.h"

static const char* codec_names[] = {
    "unknown",
    "aac",
    "alac",
    "flac",
    "mp3",
    "opus"
};

const char *codec_name(codec_type t) {
    return codec_names[t];
}
