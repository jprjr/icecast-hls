#ifndef MUXER_H
#define MUXER_H

#include "muxer_plugin.h"
#include "picture.h"
#include "codecs.h"
#include "tag.h"

struct muxer {
    void* userdata;
    const muxer_plugin* plugin;
    segment_handler segment_handler;
    picture_handler picture_handler;
    outputconfig_handler outputconfig_handler;
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

int muxer_set_segment_handler(muxer*, const segment_handler*);
int muxer_set_picture_handler(muxer*, const picture_handler*);
int muxer_set_outputconfig_handler(muxer*, const outputconfig_handler*);

int muxer_open(const muxer*, const muxerconfig*);

/* dsi will trigger writing an init segment, if appropriate */
int muxer_submit_dsi(const muxer* m, const membuf* dsi);
int muxer_submit_packet(const muxer*, const packet*);
int muxer_submit_tags(const muxer*, const taglist* tags);

int muxer_flush(const muxer*);


#ifdef __cplusplus
}
#endif

#endif

