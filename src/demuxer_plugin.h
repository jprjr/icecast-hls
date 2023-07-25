#ifndef DEMUXER_PLUGIN_H
#define DEMUXER_PLUGIN_H

#include <stddef.h>

#include "strbuf.h"
#include "input.h"
#include "packet.h"
#include "tag.h"

typedef int (*demuxer_plugin_init)(void);
typedef void (*demuxer_plugin_deinit)(void);

typedef size_t (*demuxer_plugin_size)(void);
typedef int (*demuxer_plugin_create)(void* userdata);

typedef int (*demuxer_plugin_config)(void* userdata, const strbuf* key, const strbuf* value);

/* perform any super basic probes like, check for magic numbers etc */
typedef int (*demuxer_plugin_open)(void* userdata, input* in);

/* called repeatedly as long as it returns 0.
 * return 1 to signal end-of-file and to close everything out.
 * return 2 to signal end-of-stream, flush/reset decoders and call run again.
 * anything else is an error and aborts everything immediately.
 *
 * all flushing/closing is *not* handled by the demuxer plugin, that's the job
 * of the enclosing module. */
typedef int (*demuxer_plugin_run)(void* userdata, const tag_handler* thandler, const packet_receiver* receiver);

typedef void (*demuxer_plugin_close)(void* userdata);

struct demuxer_plugin {
    const strbuf name;
    demuxer_plugin_size size;
    demuxer_plugin_init init;
    demuxer_plugin_deinit deinit;
    demuxer_plugin_create create;
    demuxer_plugin_config config;
    demuxer_plugin_open open;
    demuxer_plugin_close close;
    demuxer_plugin_run run;
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
