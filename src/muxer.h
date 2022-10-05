#ifndef MUXER_H
#define MUXER_H

#include "muxer_plugin.h"
#include "picture.h"
#include "codecs.h"
#include "tag.h"

struct muxer {
    void* userdata;
    const muxer_plugin* plugin;
    segment_receiver segment_receiver;
    picture_handler picture_handler;
    uint8_t inband_images;
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

int muxer_open(const muxer*, const packet_source* source);

/* dsi will trigger writing an init segment, if appropriate */
int muxer_submit_dsi(const muxer* m, const membuf* dsi);
int muxer_submit_packet(const muxer*, const packet*);
int muxer_submit_tags(const muxer*, const taglist* tags);

int muxer_flush(const muxer*);


#ifdef __cplusplus
}
#endif

#endif

