#ifndef DESTINATION_H
#define DESTINATION_H

/* a destination will have:
 * a filter (resample, etc)
 * an encoder (aac, flac, etc)
 * a muxer (fmp4, maybe packed audio or mpeg-ts?)
 * an output (a single file, a folder, http push).
 *
 * when each "thing" is opened it includes the previous
 * item in the chain - like the output is opened with a
 * reference to the muxer, muxer is opened with a reference
 * to the encoder, etc.
 *
 * Then each "thing" will push data upward - so the destination
 * will receive a frame, and push it into the filter. The filter
 * will push filtered frames into the encoder, which pushes
 * packets into the muxer, which pushes segments into
 * the output, which writes to disk or whatever */

#include "source.h"
#include "frame.h"
#include "thread.h"
#include "filter.h"
#include "encoder.h"
#include "muxer.h"
#include "output.h"
#include "tagmap.h"
#include "imagemode.h"
#include "ich_time.h"
#include <stdint.h>

struct destination {
    strbuf source_id; /* used during the configure phase */
    strbuf tagmap_id; /* used during the configure phase */
    const source* source;
    const taglist* tagmap;
    filter filter; /* this can be a user-configured filter. At a
                      minimum, it will be allocated to handle format
                      conversions and buffering, if nothing else */
    encoder encoder;
    muxer muxer;
    output output;
    /* if a user doesn't specify a filter, we're still going to have
     * one no matter what, so - we'll let the destination itself store
     * the sample rate and channels */
    uint8_t configuring;
    taglist_map_flags map_flags;
    image_mode image_mode;
};

typedef struct destination destination;

#ifdef __cplusplus
extern "C" {
#endif

/* global init stuff */

int destination_global_init(void);
void destination_global_deinit(void);

void destination_init(destination*);
void destination_free(destination*);

int destination_config(destination*, const strbuf* key, const strbuf* val);

int destination_open(destination*, const ich_time* now);

int destination_submit_frame(destination*, const frame* frame);
int destination_flush(const destination*);
int destination_submit_tags(const destination*, const taglist* tags);

void destination_run(void*);

void destination_dump_counters(const destination*, const strbuf* prefix);

#ifdef __cplusplus
}
#endif

#endif
