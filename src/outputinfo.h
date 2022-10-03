#ifndef OUTPUTINFO_H
#define OUTPUTINFO_H

#include <stddef.h>

/* this struct is sent back down, via callback, to the muxer,
 * to include info the muxer may need like segment length */
struct outputinfo {
    unsigned int segment_length;
};

typedef struct outputinfo outputinfo;

#define OUTPUTINFO_ZERO  { .segment_length = 0 }

typedef int (*outputinfo_submit_callback)(void *, const outputinfo*);
struct outputinfo_handler {
    outputinfo_submit_callback submit;
    void* userdata;
};

typedef struct outputinfo_handler outputinfo_handler;

#define OUTPUTINFO_HANDLER_ZERO { .submit = outputinfo_null_submit, .userdata = NULL }


#ifdef __cplusplus
extern "C" {
#endif

extern const outputinfo_handler outputinfo_ignore;
int outputinfo_null_submit(void *, const outputinfo*);


#ifdef __cplusplus
}
#endif

#endif

