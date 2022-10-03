#ifndef ENCODERINFO_H
#define ENCODERINFO_H

/* a struct used to communicate encoder-specific info
 * back to a filter, this way the encoder can specify
 * a frame length and the filter can always generate
 * the correct length of frame */
#include <stddef.h>
#include "samplefmt.h"

struct encoderinfo {
    samplefmt format;
    unsigned int frame_len;
};

typedef struct encoderinfo encoderinfo;

#define ENCODERINFO_ZERO { .format = SAMPLEFMT_UNKNOWN, .frame_len = 0 }

typedef int(*encoderinfo_submit_callback)(void* userdata, const encoderinfo* info);

struct encoderinfo_handler {
    encoderinfo_submit_callback submit;
    void* userdata;
};

typedef struct encoderinfo_handler encoderinfo_handler;

#define ENCODERINFO_HANDLER_ZERO { .submit = encoderinfo_null_submit, .userdata = NULL }

#ifdef __cplusplus
extern "C" {
#endif

extern const encoderinfo_handler encoderinfo_ignore;
int encoderinfo_null_submit(void*, const encoderinfo* info);

#ifdef __cplusplus
}
#endif

#endif
