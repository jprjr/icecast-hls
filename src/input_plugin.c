#include "input_plugin_stdin.h"
#include "input_plugin_file.h"

#ifndef INPUT_PLUGIN_CURL
#define INPUT_PLUGIN_CURL 0
#endif

#if INPUT_PLUGIN_CURL
#include "input_plugin_curl.h"
#endif

const input_plugin* input_plugin_list[] = {
    &input_plugin_stdin,
    &input_plugin_file,
#if INPUT_PLUGIN_CURL
    &input_plugin_curl,
#endif
    NULL
};

const input_plugin* input_plugin_get(const strbuf* name) {
    const input_plugin** plug = input_plugin_list;
    while(*plug != NULL) {
        if( strbuf_equals(name, &((*plug)->name)) ) return *plug;
        plug++;
    }
    return NULL;
}

int input_plugin_global_init(void) {
    int r;
    const input_plugin** plug;

    plug = input_plugin_list;
    while(*plug != NULL) {
        if( (r = (*plug)->init()) != 0) return r;
        plug++;
    }

    return 0;
}

void input_plugin_global_deinit(void) {
    const input_plugin** plug = input_plugin_list;

    while(*plug != NULL) {
        (*plug)->deinit();
        plug++;
    }

    return;
}

