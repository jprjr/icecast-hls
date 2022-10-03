#include "decoder_plugin_miniflac.h"

const decoder_plugin* decoder_plugin_list[] = {
    &decoder_plugin_miniflac,
    NULL,
};

const decoder_plugin* decoder_plugin_get(const strbuf* name) {
    const decoder_plugin** plug = decoder_plugin_list;
    while(*plug != NULL) {
        if( strbuf_equals(name, &((*plug)->name)) ) return *plug;
        plug++;
    }
    return NULL;
}

int decoder_plugin_global_init(void) {
    int r;
    const decoder_plugin** plug;

    plug = decoder_plugin_list;
    while(*plug != NULL) {
        if( (r = (*plug)->init()) != 0) return r;
        plug++;
    }

    return 0;
}

void decoder_plugin_global_deinit(void) {
    const decoder_plugin** plug = decoder_plugin_list;

    while(*plug != NULL) {
        (*plug)->deinit();
        plug++;
    }

    return;
}

