#include "input_plugin_stdin.h"

#include <stdint.h>
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
 * stdin input configured */

static uint8_t opened;

static int plugin_init(void) {
    opened = 0;
    return 0;
}

static void plugin_deinit(void) {
    return;
}

static size_t plugin_size(void) {
    return sizeof(opened);
}

static int plugin_create(void* ud) {
    (void)ud;

    if(opened) {
        fprintf(stderr,"[input:stdin] only one instance of this plugin can be active at a time\n");
        return -1;
    }
    opened = 1;
    return 0;
}

static int plugin_config(void* userdata, const strbuf* key, const strbuf* value) {
    (void)key;
    (void)value;
    (void)userdata;
    return 0;
}

static int plugin_open(void* userdata) {
    (void)userdata;
#if defined(_WIN32) || defined(_WIN64) || defined(_MSC_VER)
    if(_setmode( _fileno(stdin), _O_BINARY) == -1) return -1;
#endif
    return 0;
}

static void plugin_close(void* userdata) {
    (void)userdata;
    return;
}

static size_t plugin_read(void* userdata, void* dest, size_t len, const tag_handler* handler) {
    (void)userdata;
    (void)handler;
    return fread(dest,1,len,stdin);
}

const input_plugin input_plugin_stdin = {
    { .a = 0, .len = 5, .x = (uint8_t*)"stdin" },
    plugin_size,
    plugin_init,
    plugin_deinit,
    plugin_create,
    plugin_config,
    plugin_open,
    plugin_close,
    plugin_read,
};
