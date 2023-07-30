#include "codecs.h"

static const char* codec_names[] = {
    "unknown",
    "private",
    "aac",
    "alac",
    "flac",
    "mp3",
    "opus",
    "ac3",
    "e-ac3",
};

const char *codec_name(codec_type t) {
    return codec_names[t];
}
