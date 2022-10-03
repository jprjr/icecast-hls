#ifndef DECODER_PLUGIN_H
#define DECODER_PLUGIN_H

#include <stddef.h>

#include "strbuf.h"
#include "input.h"
#include "tag.h"
#include "frame.h"
#include "audioconfig.h"

typedef int (*decoder_plugin_init)(void);
typedef void (*decoder_plugin_deinit)(void);

typedef void* (*decoder_plugin_create)(void);

typedef int (*decoder_plugin_config)(void* userdata, const strbuf* key, const strbuf* value);

typedef int (*decoder_plugin_open)(void* userdata, const input *in, const audioconfig_handler* config);

typedef int (*decoder_plugin_decode)(void* userdata, const tag_handler* tag_handler, const frame_handler* frame_handler);

typedef void (*decoder_plugin_close)(void* userdata);

struct decoder_plugin {
    const strbuf name;
    decoder_plugin_init init;
    decoder_plugin_deinit deinit;
    decoder_plugin_create create;
    decoder_plugin_config config;
    decoder_plugin_open open;
    decoder_plugin_close close;
    decoder_plugin_decode decode;
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
