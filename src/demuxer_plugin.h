#ifndef DEMUXER_PLUGIN_H
#define DEMUXER_PLUGIN_H

#include <stddef.h>

#include "strbuf.h"
#include "input.h"
#include "packet.h"
#include "tag.h"

typedef int (*demuxer_plugin_init)(void);
typedef int (*demuxer_plugin_deinit)(void);

typedef void* (*demuxer_plugin_create)(void);

typedef int (*demuxer_plugin_config)(void* userdata, const strbuf* key, const strbuf* value);

typedef int (*demuxer_plugin_open)(void* userdata, input* in, const packet_receiver* dest);

typedef int (*demuxer_plugin_demux)(void* userdata, const tag_handler* tag_handler, const packet_receiver* dest);
typedef void (*demuxer_plugin_close)(void* userdata);

struct demuxer_plugin {
    const strbuf name;
    demuxer_plugin_init init;
    demuxer_plugin_deinit deinit;
    demuxer_plugin_create create;
    demuxer_plugin_config config;
    demuxer_plugin_open open;
    demuxer_plugin_close close;
    demuxer_plugin_demux demux;
};

typedef struct demuxer_plugin demuxer_plugin;

extern const demuxer_plugin* demuxer_plugin_list[];

#ifdef __cplusplus
extern "C" {
#endif

const demuxer_plugin* demuxer_plugin_get(const strbuf* name);

int demuxer_plugin_global_init(void);
void demuxer_plugin_global_deinit(void);

#ifdef __cplusplus
}
#endif

#endif
