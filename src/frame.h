#ifndef FRAME_H
#define FRAME_H

#include "samplefmt.h"
#include "membuf.h"

/* represents a frame of audio */
struct frame {
    membuf samples; /* in planer formats this has as many elements as there are channels */
    samplefmt format;
    unsigned int channels;
    unsigned int duration; /* duration given in number of samples */

    /* sample_rate and pts are only used when
     * converting from a frame to an AVFrame */
    unsigned int sample_rate;

    /* note - this is an unsigned type but ffmpeg uses a signed type,
     * I think to signal things like encoder delay? Either way, we just
     * case into an int64_t if we need to, from my testing, that's how
     * avcodec/etc handles stuff with > int64_t timestamps anyway */
    uint64_t pts;
};
typedef struct frame frame;

/* this is sent by encoders/filters (frame receivers) to
 * set params on the source (decoders, filters) */
struct frame_source_params {
    samplefmt format;
    unsigned int channels;
    unsigned int duration;
};
typedef struct frame_source_params frame_source_params;

typedef int (*frame_source_set_params_cb)(void* handle, const frame_source_params* params);

/* the frame source is used to inform frame receivers (encoders, filters) about the kind of incoming frame data */
struct frame_source {
    void* handle;
    frame_source_set_params_cb set_params;
    samplefmt format;
    unsigned int channels;
    unsigned int duration;
    unsigned int sample_rate;
};
typedef struct frame_source frame_source;

/* a frame receiver can be an encoder or filter */
typedef int (*frame_receiver_open_cb)(void* handle, const frame_source* source);
typedef int (*frame_receiver_submit_frame_cb)(void* handle, const frame* frame);
typedef int (*frame_receiver_flush_cb)(void* handle);

struct frame_receiver {
    void* handle;
    frame_receiver_open_cb open;
    frame_receiver_submit_frame_cb submit_frame;
    frame_receiver_flush_cb flush;
};
typedef struct frame_receiver frame_receiver;

#define FRAME_ZERO { \
    .samples = MEMBUF_ZERO, \
    .format = SAMPLEFMT_UNKNOWN, \
    .channels = 0, \
    .duration = 0, \
    .sample_rate = 0, \
    .pts = 0, \
}

#define FRAME_SOURCE_PARAMS_ZERO { \
    .format = SAMPLEFMT_UNKNOWN, \
    .channels = 0, \
    .duration = 0, \
}

#define FRAME_SOURCE_ZERO { \
    .handle = NULL, \
    .set_params = frame_source_set_params_null, \
    .format = SAMPLEFMT_UNKNOWN, \
    .channels = 0, \
    .duration = 0, \
    .sample_rate = 0, \
}

#define FRAME_RECEIVER_ZERO { \
    .handle = NULL, \
    .open = frame_receiver_open_null, \
    .submit_frame = frame_receiver_submit_frame_null, \
    .flush = frame_receiver_flush_null, \
}

#ifdef __cplusplus
extern "C" {
#endif

void frame_init(frame*);
void frame_free(frame*);

int frame_ready(frame*);

int frame_buffer(frame*); /* after setting the duration, buffers space for samples */

membuf* frame_get_channel(const frame* f, size_t idx);

/* returns a pointer directly to the sample data */
void* frame_get_channel_samples(const frame* f, size_t idx);

int frame_copy(frame*, const frame*);

int frame_convert(frame*, const frame*, samplefmt fmt);

int frame_append(frame*, const frame*);

/* move sample data from one frame to another,
 * uses memcpy under the hood, meaning src and
 * dest parameters (including format) have to
 * match */
int frame_move(frame* dest, frame* src, unsigned int len);

/* throw away samples from the beginning of the frame */
int frame_trim(frame*, unsigned int len);

/* pad the frame with zeroes */
int frame_fill(frame*, unsigned int duration);


int frame_receiver_open_null(void* handle, const frame_source* source);
int frame_receiver_submit_frame_null(void* handle, const frame* frame);
int frame_receiver_flush_null(void* handle);
int frame_source_set_params_null(void* handle, const frame_source_params* params);
int frame_source_set_params_ignore(void* handle, const frame_source_params* params);

extern const frame_receiver frame_receiver_zero;
extern const frame_source frame_source_zero;
extern const frame frame_zero;
extern const frame_source_params frame_source_params_zero;

#ifdef __cplusplus
}
#endif

#endif
