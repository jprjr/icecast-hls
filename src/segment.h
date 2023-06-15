#ifndef SEGMENT_H
#define SEGMENT_H

#include <stddef.h>
#include "strbuf.h"
#include "tag.h"

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
    uint64_t pts; /* pts of this segment, used to detect discontinuities */
};

typedef struct segment segment;

struct segment_source_info {
    unsigned int time_base; /* the time base (samplerate, usually) of the segment source */
    unsigned int frame_len; /* the length of a frame, in samples, of a packet within the source */
};
typedef struct segment_source_info segment_source_info;

struct segment_params {
    size_t segment_length; /* given in milliseconds */
    size_t packets_per_segment;
};
typedef struct segment_params segment_params;

/* used by segment producers (muxers) to open a segment receiver (output)
 * and provide details on segments - extension info, mimetypes, and time base */
struct segment_source {
    void* handle; /* a pointer back to whoever's providing this config */
    const strbuf* init_ext;
    const strbuf* init_mimetype;
    const strbuf* media_ext;
    const strbuf* media_mimetype;
    unsigned int time_base;
    unsigned int frame_len;
};
typedef struct segment_source segment_source;


/* a segment receiver represents output - it receives segments from
 * the muxer. It has an open function (mapped to output_open),
 * so the muxer can tell the output about segment info */

typedef int (*segment_receiver_get_segment_info_cb)(const void* handle, const segment_source_info* info, segment_params* params);
typedef int (*segment_receiver_open_cb)(void* handle, const segment_source* source);
typedef int (*segment_receiver_submit_segment_cb)(void* handle, const segment* segment);
typedef int (*segment_receiver_submit_tags_cb)(void* handle, const taglist* tags);
typedef int (*segment_receiver_flush_cb)(void* handle);

struct segment_receiver {
    void* handle;
    segment_receiver_open_cb open;
    segment_receiver_submit_segment_cb submit_segment;
    segment_receiver_submit_tags_cb submit_tags;
    segment_receiver_flush_cb flush;
    segment_receiver_get_segment_info_cb get_segment_info;
};

typedef struct segment_receiver segment_receiver;

/* finally some defines for allocating these things on the stack */

#define SEGMENT_RECEIVER_ZERO { .handle = NULL, .get_segment_info = segment_receiver_get_segment_info_null, .open = segment_receiver_open_null, .submit_segment = segment_receiver_submit_segment_null, .submit_tags = segment_receiver_submit_tags_null, .flush = segment_receiver_flush_null }

#define SEGMENT_SOURCE_ZERO { .handle = NULL, .init_ext = NULL, .init_mimetype = NULL, .media_ext = NULL, .media_mimetype = NULL }
#define SEGMENT_SOURCE_INFO_ZERO { .time_base = 0, .frame_len = 0 }
#define SEGMENT_PARAMS_ZERO { .segment_length = 0, .packets_per_segment = 0 }

#define SEGMENT_ZERO { .type = SEGMENT_TYPE_UNKNOWN, .data = NULL, .len = 0, .samples = 0 }

#ifdef __cplusplus
extern "C" {
#endif

extern const segment_params segment_params_zero;
extern const segment_receiver segment_receiver_zero;
extern const segment_source segment_source_zero;
extern const segment segment_zero;

int segment_receiver_get_segment_info_null(const void* handle, const segment_source_info* info, segment_params* params);
int segment_receiver_open_null(void* handle, const segment_source*);
int segment_receiver_submit_segment_null(void* handle, const segment*);
int segment_receiver_submit_tags_null(void* handle, const taglist*);
int segment_receiver_flush_null(void* handle);

#ifdef __cplusplus
}
#endif

#endif
