#include "encoder_plugin_passthrough.h"

#include "strbuf.h"

#include <stdio.h>
#include <stdlib.h>

#define LOG0(fmt) fprintf(stderr,"[encoder:passthrough] " fmt "\n")

static STRBUF_CONST(plugin_name, "passthrough");

struct plugin_userdata {
    unsigned int dummy;
};

typedef struct plugin_userdata plugin_userdata;

static int plugin_init(void) { return 0; }
static void plugin_deinit(void) { return; }
static size_t plugin_size(void) { return sizeof(plugin_userdata); }

static int plugin_create(void* ud) {
    (void)ud;
    return 0;
}

static int plugin_config(void* ud, const strbuf* key, const strbuf* value) {
    (void)ud;
    (void)key;
    (void)value;

    return 0;
}

static int plugin_open(void* ud, const frame_source* source, const packet_receiver* dest) {
    (void)ud;
    if(source->format != SAMPLEFMT_BINARY) {
        LOG0("passthrough encoder only supports packets");
        return -1;
    }

    return dest->open(dest->handle, &source->packet_source);
}

static void plugin_close(void* ud) {
    (void)ud;
}

static int plugin_submit_frame(void* ud, const frame* f, const packet_receiver* dest) {
    (void)ud;
    return dest->submit_packet(dest->handle, &f->packet);
}

static int plugin_flush(void* ud, const packet_receiver* dest) {
    (void)ud;
    (void)dest;
    return 0;
}

static int plugin_reset(void* ud) {
    (void)ud;
    return 0;
}

const encoder_plugin encoder_plugin_passthrough = {
    plugin_name,
    plugin_size,
    plugin_init,
    plugin_deinit,
    plugin_create,
    plugin_config,
    plugin_open,
    plugin_close,
    plugin_submit_frame,
    plugin_flush,
    plugin_reset,
};

