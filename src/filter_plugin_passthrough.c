#include "filter_plugin_passthrough.h"

#include <stdlib.h>
#include <stdio.h>

/* this just passes audio straight through with no
 * buffering, format conversion, etc. Used as the
 * default filter in sources */

static STRBUF_CONST(plugin_name, "passthrough");

struct plugin_userdata {
    unsigned int opened;
};

typedef struct plugin_userdata plugin_userdata;

static int plugin_init(void) {
    return 0;
}

static void plugin_deinit(void) {
    return;
}

static size_t plugin_size(void) {
    return sizeof(plugin_userdata);
}

static int plugin_create(void* ud) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    userdata->opened = 0;
    return 0;
}

static void plugin_close(void* ud) {
    (void)ud;
}

static int plugin_config(void* ud, const strbuf* key, const strbuf* val) {
    (void)ud;
    (void)key;
    (void)val;
    return 0;
}

static int plugin_open(void* ud, const frame_source* source, const frame_receiver* dest) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    (void)userdata;

    return dest->open(dest->handle,source);
}

static int plugin_submit_frame(void* ud, const frame* f, const frame_receiver* dest) {
    (void)ud;
    return dest->submit_frame(dest->handle,f);
}

static int plugin_flush(void* ud, const frame_receiver* dest) {
    (void)ud;
    (void)dest;
    return 0;
}

static int plugin_reset(void* ud) {
    (void)ud;
    return 0;
}

const filter_plugin filter_plugin_passthrough = {
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

