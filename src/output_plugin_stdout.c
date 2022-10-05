#include "output_plugin_stdout.h"

#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64) || defined(_MSC_VER)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

/* global flag to make sure we only have 1
 * stdout output configured */

static uint8_t opened;

static int plugin_init(void) {
    opened = 0;
    return 0;
}

static void plugin_deinit(void) {
    return;
}

static void* plugin_create(void) {
    if(opened) {
        fprintf(stderr,"[output:stdout] only one instance of this plugin can be active at a time\n");
        return NULL;
    }
    opened = 1;
    return &opened;
}

static int plugin_config(void* userdata, const strbuf* key, const strbuf* value) {
    (void)key;
    (void)value;
    (void)userdata;
    return 0;
}

static int plugin_open(void* userdata, const segment_source* source) {
    (void)userdata;
    segment_source_params params = SEGMENT_SOURCE_PARAMS_ZERO;
#if defined(_WIN32) || defined(_WIN64) || defined(_MSC_VER)
    if(_setmode( _fileno(stdout), _O_BINARY) == -1) return -1;
#endif
    return source->set_params(source->handle, &params);
}

static void plugin_close(void* userdata) {
    (void)userdata;
    return;
}

static int plugin_submit_segment(void* userdata, const segment* seg) {
    (void)userdata;
    return fwrite(seg->data,1,seg->len,stdout) == seg->len ? 0 : -1;
}

static int plugin_submit_picture(void* userdata, const picture* src, picture* out) {
    (void)userdata;
    (void)src;
    (void)out;
    return 0;
}

static int plugin_set_time(void* userdata, const ich_time* now) {
    (void)userdata;
    (void)now;
    return 0;
}

static int plugin_flush(void* userdata) {
    (void)userdata;
    return 0;
}

const output_plugin output_plugin_stdout = {
    { .a = 0, .len = 6, .x = (uint8_t*)"stdout" },
    plugin_init,
    plugin_deinit,
    plugin_create,
    plugin_config,
    plugin_open,
    plugin_close,
    plugin_set_time,
    plugin_submit_segment,
    plugin_submit_picture,
    plugin_flush,
};
