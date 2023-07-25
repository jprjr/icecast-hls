#ifndef DECODER_PLUGIN_H
#define DECODER_PLUGIN_H

#include <stddef.h>

#include "strbuf.h"
#include "input.h"
#include "tag.h"
#include "packet.h"
#include "frame.h"

typedef int (*decoder_plugin_init)(void);
typedef void (*decoder_plugin_deinit)(void);

typedef size_t (*decoder_plugin_size)(void);
typedef int (*decoder_plugin_create)(void* userdata);

typedef int (*decoder_plugin_config)(void* userdata, const strbuf* key, const strbuf* value);

typedef int (*decoder_plugin_open)(void* userdata, const packet_source* src, const frame_receiver* frame_dest);

typedef int (*decoder_plugin_decode)(void* userdata, const packet* p, const frame_receiver* frame_dest);

/* flush any remaining frames of audio out, MUST NOT call frame_receiver flush() */
typedef int (*decoder_plugin_flush)(void* userdata, const frame_receiver* frame_dest);

/* reset the decoder state for another call to open() */
typedef int (*decoder_plugin_reset)(void* userdata);

typedef void (*decoder_plugin_close)(void* userdata);

struct decoder_plugin {
    const strbuf name;
    decoder_plugin_size size;
    decoder_plugin_init init;
    decoder_plugin_deinit deinit;
    decoder_plugin_create create;
    decoder_plugin_config config;
    decoder_plugin_open open;
    decoder_plugin_close close;
    decoder_plugin_decode decode;
    decoder_plugin_flush flush;
    decoder_plugin_reset reset;
};

typedef struct decoder_plugin decoder_plugin;

extern const decoder_plugin* decoder_plugin_list[];

#ifdef __cplusplus
extern "C" {
#endif

const decoder_plugin* decoder_plugin_get(const strbuf* name);

int decoder_plugin_global_init(void);
void decoder_plugin_global_deinit(void);

#ifdef __cplusplus
}
#endif

#endif
