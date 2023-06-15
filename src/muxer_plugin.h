#ifndef MUXER_PLUGIN_H
#define MUXER_PLUGIN_H

#include "strbuf.h"
#include "codecs.h"
#include "packet.h"
#include "segment.h"
#include "tag.h"

typedef int (*muxer_plugin_init)(void);
typedef void (*muxer_plugin_deinit)(void);

typedef void* (*muxer_plugin_create)(void);

typedef int (*muxer_plugin_config)(void* userdata, const strbuf* key, const strbuf* value);

typedef int (*muxer_plugin_open)(void* userdata, const packet_source* source, const segment_receiver* dest);

typedef void (*muxer_plugin_close)(void* userdata);

typedef int (*muxer_plugin_submit_packet)(void* userdata, const packet* packet, const segment_receiver* dest);
typedef int (*muxer_plugin_submit_tags)(void* userdata, const taglist* tags, const segment_receiver* dest);
typedef int (*muxer_plugin_flush)(void* userdata, const segment_receiver* dest);
typedef uint32_t (*muxer_plugin_get_caps)(void* userdata);

typedef int (*muxer_plugin_get_segment_info)(const void* userdata, const packet_source_info*, const segment_receiver*, packet_source_params*);

struct muxer_plugin {
    const strbuf name;
    muxer_plugin_init init;
    muxer_plugin_deinit deinit;
    muxer_plugin_create create;
    muxer_plugin_config config;
    muxer_plugin_open open;
    muxer_plugin_close close;
    muxer_plugin_submit_packet submit_packet;
    muxer_plugin_submit_tags submit_tags;
    muxer_plugin_flush flush;
    muxer_plugin_get_caps get_caps;
    muxer_plugin_get_segment_info get_segment_info;
};

typedef struct muxer_plugin muxer_plugin;

extern const muxer_plugin* muxer_plugin_list[];

#ifdef __cplusplus
extern "C" {
#endif

const muxer_plugin* muxer_plugin_get(const strbuf* name);

int muxer_plugin_global_init(void);
void muxer_plugin_global_deinit(void);

#ifdef __cplusplus
}
#endif

#endif


