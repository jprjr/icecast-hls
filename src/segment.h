#ifndef SEGMENT_H
#define SEGMENT_H

#include <stddef.h>
#include "strbuf.h"

/* segments are produced by muxers and sent into outputs */
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


/* the segment_source_params is used by a segment
 * receiver (output) to send params back into the segment producer
 * (muxer), to request things like - a particular segment length */
struct segment_source_params {
    unsigned int segment_length;
};
typedef struct segment_source_params segment_source_params;

/* the callback used by the receiver to send the params over */
typedef int (*segment_source_set_params_cb)(void* segment_source, const segment_source_params* params);

/* used by segment producers (muxers) to open a segment receiver (output)
 * and provide details on segments - extension info, mimetypes, and time base */
struct segment_source {
    void* handle; /* a pointer back to whoever's providing this config */
    segment_source_set_params_cb set_params; /* the callback to provide params back down to the segment producer (muxer) */
    const strbuf* init_ext;
    const strbuf* init_mime;
    const strbuf* media_ext;
    const strbuf* media_mime;
    unsigned int time_base;
};
typedef struct segment_source segment_source;


/* a segment receiver represents output - it receives segments from
 * the muxer. It has an open function (mapped to output_open),
 * so the muxer can tell the output about segment info */

typedef int (*segment_receiver_open_cb)(void* handle, const segment_source* source);
typedef int (*segment_receiver_submit_segment_cb)(void* handle, const segment* segment);
typedef int (*segment_receiver_flush_cb)(void* handle);

struct segment_receiver {
    void* handle;
    segment_receiver_open_cb open;
    segment_receiver_submit_segment_cb submit_segment;
    segment_receiver_flush_cb flush;
};

typedef struct segment_receiver segment_receiver;

/* finally some defines for allocating these things on the stack */

#define SEGMENT_RECEIVER_ZERO { .handle = NULL, .open = segment_receiver_open_null, .submit_segment = segment_receiver_submit_segment_null, .flush = segment_receiver_flush_null }

#define SEGMENT_SOURCE_ZERO { .handle = NULL, .set_params = segment_source_set_params_null, .init_ext = NULL, .init_mime = NULL, .media_ext = NULL, .media_mime = NULL, .time_base = 0 }

#define SEGMENT_SOURCE_PARAMS_ZERO { .segment_length = 0 }

#define SEGMENT_ZERO { .type = SEGMENT_TYPE_UNKNOWN, .data = NULL, .len = 0, .samples = 0 }

#ifdef __cplusplus
extern "C" {
#endif

extern const segment_source_params segment_source_params_zero;
extern const segment_receiver segment_receiver_zero;
extern const segment_source segment_source_zero;
extern const segment segment_zero;

int segment_source_set_params_null(void* userdata, const segment_source_params* params);
int segment_receiver_open_null(void* handle, const segment_source*);
int segment_receiver_submit_segment_null(void* handle, const segment*);
int segment_receiver_flush_null(void* handle);

int segment_source_set_params_ignore(void* userdata, const segment_source_params* params);

#ifdef __cplusplus
}
#endif

#endif
