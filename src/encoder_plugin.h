#ifndef ENCODER_PLUGIN_H
#define ENCODER_PLUGIN_H

#include "strbuf.h"
#include "codecs.h"
#include "frame.h"
#include "packet.h"
#include "audioconfig.h"
#include "muxerconfig.h"
#include "samplefmt.h"

#include <stddef.h>

typedef int (*encoder_plugin_init)(void);
typedef void (*encoder_plugin_deinit)(void);

typedef void* (*encoder_plugin_create)(void);

typedef int (*encoder_plugin_config)(void* userdata, const strbuf* key, const strbuf* value);

typedef int (*encoder_plugin_open)(void* userdata, const audioconfig*, const muxerconfig_handler*);

typedef void (*encoder_plugin_close)(void* userdata);

typedef int (*encoder_plugin_submit_frame)(void* userdata, const frame* frame, const packet_handler* handler);
typedef int (*encoder_plugin_flush)(void* userdata, const packet_handler* handler);

struct encoder_plugin {
    const strbuf name;
    encoder_plugin_init init;
    encoder_plugin_deinit deinit;
    encoder_plugin_create create;
    encoder_plugin_config config;
    encoder_plugin_open open;
    encoder_plugin_close close;
    encoder_plugin_submit_frame submit_frame;
    encoder_plugin_flush flush;
};

typedef struct encoder_plugin encoder_plugin;

extern const encoder_plugin* encoder_plugin_list[];

#ifdef __cplusplus
extern "C" {
#endif

const encoder_plugin* encoder_plugin_get(const strbuf* name);

int encoder_plugin_global_init(void);
void encoder_plugin_global_deinit(void);

#ifdef __cplusplus
}
#endif

#endif

