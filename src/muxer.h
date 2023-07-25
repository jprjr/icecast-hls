#ifndef MUXER_H
#define MUXER_H

#include "muxer_plugin.h"
#include "picture.h"
#include "codecs.h"
#include "tag.h"
#include "imagemode.h"
#include "ich_time.h"

struct muxer {
    void* userdata;
    const muxer_plugin* plugin;
    segment_receiver segment_receiver;
    picture_handler picture_handler;
    image_mode image_mode;
    size_t counter;
    ich_time ts;
    int output_opened;
};

typedef struct muxer muxer;

#ifdef __cplusplus
extern "C" {
#endif


/* performs any needed global init/deinit */
int muxer_global_init(void);
void muxer_global_deinit(void);

void muxer_init(muxer*);
void muxer_free(muxer*);

int muxer_create(muxer*, const strbuf* plugin_name);
int muxer_config(const muxer*, const strbuf* name, const strbuf* value);

int muxer_open(muxer*, const packet_source* source);

int muxer_submit_packet(muxer*, const packet*);
int muxer_submit_tags(const muxer*, const taglist* tags);

int muxer_flush(const muxer*);
int muxer_reset(const muxer*);

uint32_t muxer_get_caps(const muxer*);

/* accepts a sample rate + frame length and
 * returns the length of a segment and number
 * of packets per segment, pretty much just used
 * by exhale to configure a good tune-in period.
 * the muxer plugin shouldn't NOT make changes or anything,
 * this is strictly informative and may not even be called */
int muxer_get_segment_info(const muxer*, const packet_source_info*, packet_source_params*);


void muxer_dump_counters(const muxer*, const strbuf*);

#ifdef __cplusplus
}
#endif

#endif

