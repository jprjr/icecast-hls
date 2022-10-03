#ifndef FRAME_H
#define FRAME_H

#include "samplefmt.h"
#include "membuf.h"

/* represents a frame of audio */
struct frame {
    membuf samples; /* in planer formats this has as many elements as there are channels */
    size_t channels;
    size_t duration; /* duration given in number of samples */
    samplefmt format;

    /* sample_rate and pts are only used when
     * converting from a frame to an AVFrame */
    unsigned int sample_rate;

    /* note - this is an unsigned type but ffmpeg uses a signed type,
     * I think to signal things like encoder delay? Either way,
     * when setting/updating a frame PTS, check if it's over INT64_MAX
     * and subract INT64_MAX, if so */
    uint64_t pts;
};

typedef struct frame frame;

typedef int (*frame_handler_callback)(void* userdata, const frame* frame);
typedef int (*frame_handler_flush_callback)(void* userdata);

struct frame_handler {
    frame_handler_callback cb;
    frame_handler_flush_callback flush;
    void* userdata;
};

typedef struct frame_handler frame_handler;

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
int frame_move(frame* dest, frame* src, size_t len);

/* throw away samples from the beginning of the frame */
int frame_discard(frame*, size_t len);

#ifdef __cplusplus
}
#endif

#endif
