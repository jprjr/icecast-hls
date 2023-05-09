#ifndef OUTPUT_PLUGIN_H
#define OUTPUT_PLUGIN_H

#include "strbuf.h"
#include "segment.h"
#include "picture.h"
#include "tag.h"
#include "ich_time.h"

/* perform global-init/deinit type stuff on a plugin */
typedef int (*output_plugin_init)(void);
typedef void (*output_plugin_deinit)(void);

typedef void* (*output_plugin_create)(void);
typedef void (*output_plugin_close)(void* userdata);

typedef int (*output_plugin_config)(void* userdata, const strbuf* key, const strbuf* value);
typedef int (*output_plugin_open)(void* userdata, const segment_source* source);

typedef int (*output_plugin_submit_segment)(void* userdata, const segment* segment);
typedef int (*output_plugin_submit_picture)(void* userdata, const picture* src, picture* out);
typedef int (*output_plugin_submit_tags)(void* userdata, const taglist* tags);
typedef int (*output_plugin_flush)(void* userdata);
typedef int (*output_plugin_set_time)(void* userdata, const ich_time* now);

typedef unsigned int (*output_plugin_get_segment_length)(void* userdata);

struct output_plugin {
    const strbuf name;
    output_plugin_init init;
    output_plugin_deinit deinit;
    output_plugin_create create;
    output_plugin_config config;
    output_plugin_open open;
    output_plugin_close close;
    output_plugin_set_time set_time;
    output_plugin_submit_segment submit_segment;
    output_plugin_submit_picture submit_picture;
    output_plugin_submit_tags submit_tags;
    output_plugin_flush flush;
};

typedef struct output_plugin output_plugin;

extern const output_plugin* output_plugin_list[];

#ifdef __cplusplus
extern "C" {
#endif

const output_plugin* output_plugin_get(const strbuf* name);

int output_plugin_global_init(void);
void output_plugin_global_deinit(void);

#ifdef __cplusplus
}
#endif

#endif
