#ifndef AUDIOCONFIG_H
#define AUDIOCONFIG_H

#include "samplefmt.h"
#include "encoderinfo.h"

/* a struct used by decoders/sources/filters to
 * inform their consumer of what kind of audio
 * to expect - things like number of channels,
 * sample rate, and so on */

struct audioconfig {
    encoderinfo_handler info;
    samplefmt format;
    unsigned int channels;
    unsigned int sample_rate;
};

typedef struct audioconfig audioconfig;

#define AUDIOCONFIG_ZERO { .info = ENCODERINFO_HANDLER_ZERO, .format = SAMPLEFMT_UNKNOWN, .channels = 0, .sample_rate = 0 }

typedef int(*audioconfig_open_callback)(void* userdata, const audioconfig* config);

struct audioconfig_handler {
    audioconfig_open_callback open;
    void* userdata;
};

typedef struct audioconfig_handler audioconfig_handler;

#define AUDIOCONFIG_HANDLER_ZERO { .open = audioconfig_null_open, .userdata = NULL }

#ifdef __cplusplus
extern "C" {
#endif

extern const audioconfig_handler audioconfig_ignore;
extern const audioconfig audioconfig_zero;
int audioconfig_null_open(void*, const audioconfig*);

#ifdef __cplusplus
}
#endif

#endif
