#ifndef OUTPUTCONFIG_H
#define OUTPUTCONFIG_H

#include "strbuf.h"
#include "outputinfo.h"
#include <stddef.h>

struct outputconfig {
    outputinfo_handler info;
    const strbuf* init_ext;
    const strbuf* init_mime;
    const strbuf* media_ext;
    const strbuf* media_mime;
    unsigned int time_base;
};

typedef struct outputconfig outputconfig;

#define OUTPUTCONFIG_ZERO  { .info = OUTPUTINFO_HANDLER_ZERO, .init_ext = NULL, .init_mime = NULL, .media_ext = NULL, .media_mime = NULL, .time_base = 0 }

typedef int (*outputconfig_submit_callback)(void*, const outputconfig*);

struct outputconfig_handler {
    outputconfig_submit_callback submit;
    void* userdata;
};

typedef struct outputconfig_handler outputconfig_handler;

#define OUTPUTCONFIG_HANDLER_ZERO { .submit = outputconfig_null_submit, .userdata = NULL }

#ifdef __cplusplus
extern "C" {
#endif

extern const outputconfig_handler outputconfig_ignore;
int outputconfig_null_submit(void *, const outputconfig*);

#ifdef __cplusplus
}
#endif

#endif

