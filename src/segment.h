#ifndef SEGMENT_H
#define SEGMENT_H

#include <stddef.h>

enum segment_type {
    SEGMENT_TYPE_UNKNOWN,
    SEGMENT_TYPE_INIT,
    SEGMENT_TYPE_MEDIA
};

typedef enum segment_type segment_type;

struct segment {
    segment_type type;
    const void* data;
    size_t len;
    unsigned int samples; /* will be 0 for init segments */
};

typedef struct segment segment;

typedef int (*segment_handler_callback)(void* userdata, const segment* segment);
typedef int (*segment_handler_flush_callback)(void* userdata);

struct segment_handler {
    segment_handler_callback cb;
    segment_handler_flush_callback flush;
    void* userdata;
};

typedef struct segment_handler segment_handler;

#endif
