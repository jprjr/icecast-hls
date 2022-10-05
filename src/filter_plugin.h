#ifndef FILTER_PLUGIN_H
#define FILTER_PLUGIN_H

#include "strbuf.h"
#include "frame.h"
#include "samplefmt.h"
#include <stddef.h>

typedef int (*filter_plugin_init)(void);
typedef void (*filter_plugin_deinit)(void);

typedef void* (*filter_plugin_create)(void);

typedef int (*filter_plugin_config)(void* userdata, const strbuf* key, const strbuf* value);

typedef int (*filter_plugin_open)(void* userdata, const frame_source*, const frame_receiver* dest);
typedef void (*filter_plugin_close)(void* userdata);

typedef int (*filter_plugin_submit_frame)(void* userdata, const frame*, const frame_receiver* handler);
typedef int (*filter_plugin_flush)(void* userdata, const frame_receiver* handler);

struct filter_plugin {
    const strbuf name;
    filter_plugin_init init;
    filter_plugin_deinit deinit;
    filter_plugin_create create;
    filter_plugin_config config;
    filter_plugin_open open;
    filter_plugin_close close;
    filter_plugin_submit_frame submit_frame;
    filter_plugin_flush flush;
};

typedef struct filter_plugin filter_plugin;

extern const filter_plugin* filter_plugin_list[];

#ifdef __cplusplus
extern "C" {
#endif

const filter_plugin* filter_plugin_get(const strbuf* name);

int filter_plugin_global_init(void);
void filter_plugin_global_deinit(void);

#ifdef __cplusplus
}
#endif

#endif
