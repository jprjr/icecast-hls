#include "output_plugin_stdout.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64) || defined(_MSC_VER)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

#define LOG0(s) fprintf(stderr,"[output:stdout] "s"\n")
#define LOG1(s, a) fprintf(stderr,"[output:stdout] "s"\n", (a))

#define TRY0(exp, act) if( (r = (exp)) != 0 ) { act; goto cleanup; }

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
        LOG0("only one instance of this plugin can be active at a time");
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

static int plugin_get_segment_info(const void* userdata, const segment_source_info* info, segment_params* params) {
    (void)userdata;
    (void)info;
    (void)params;
    return 0;
}

static int plugin_open(void* userdata, const segment_source* source) {
    int r;
    (void)userdata;
    (void)source;
#if defined(_WIN32) || defined(_WIN64) || defined(_MSC_VER)
    TRY0(_setmode( _fileno(stdout), _O_BINARY), LOG0("error setting stdout mode to binary") );
#else
    r = 0;
    /* just so the compiler doesn't complain this isn't used */
    goto cleanup;
#endif
    cleanup:
    return r;
}

static void plugin_close(void* userdata) {
    (void)userdata;
    return;
}

static int plugin_submit_segment(void* userdata, const segment* seg) {
    (void)userdata;
    int r;

    TRY0(fwrite(seg->data,1,seg->len,stdout) == seg->len ? 0 : -1,
      LOG1("error writing segment: %s", strerror(errno))
    );

    cleanup:
    return r;
}

static int plugin_submit_picture(void* userdata, const picture* src, picture* out) {
    (void)userdata;
    (void)src;
    (void)out;
    return 0;
}

static int plugin_submit_tags(void* ud, const taglist* tags) {
    (void)ud;
    (void)tags;
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
    plugin_submit_tags,
    plugin_flush,
    plugin_get_segment_info,
};
