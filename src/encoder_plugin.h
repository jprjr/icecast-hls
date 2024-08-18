#ifndef ENCODER_PLUGIN_H
#define ENCODER_PLUGIN_H

#include "strbuf.h"
#include "codecs.h"
#include "frame.h"
#include "packet.h"
#include "samplefmt.h"

#include <stddef.h>

typedef int (*encoder_plugin_init)(void);
typedef void (*encoder_plugin_deinit)(void);
typedef size_t (*encoder_plugin_size)(void);

typedef int (*encoder_plugin_create)(void* userdata);

typedef int (*encoder_plugin_config)(void* userdata, const strbuf* key, const strbuf* value);

typedef int (*encoder_plugin_open)(void* userdata, const frame_source *source, const packet_receiver* dest);

typedef void (*encoder_plugin_close)(void* userdata);

typedef int (*encoder_plugin_submit_frame)(void* userdata, const frame* frame, const packet_receiver* dest);
typedef int (*encoder_plugin_flush)(void* userdata, const packet_receiver* dest);

typedef int (*encoder_plugin_reset)(void* userdata);

struct encoder_plugin {
    const strbuf* name;
    encoder_plugin_size size;
    encoder_plugin_init init;
    encoder_plugin_deinit deinit;
    encoder_plugin_create create;
    encoder_plugin_config config;
    encoder_plugin_open open;
    encoder_plugin_close close;
    encoder_plugin_submit_frame submit_frame;
    encoder_plugin_flush flush;
    encoder_plugin_reset reset;
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

