#include "filter_plugin.h"

#ifndef FILTER_PLUGIN_AVFILTER
#define FILTER_PLUGIN_AVFILTER 0
#endif

#if FILTER_PLUGIN_AVFILTER
#include "filter_plugin_avfilter.h"
#endif

#include "filter_plugin_passthrough.h"

const filter_plugin* filter_plugin_list[] = {
#if FILTER_PLUGIN_AVFILTER
    &filter_plugin_avfilter,
#endif
    &filter_plugin_passthrough,
    NULL,
};

const filter_plugin* filter_plugin_get(const strbuf* name) {
    const filter_plugin** plug = filter_plugin_list;
    while(*plug != NULL) {
        if( strbuf_equals(name, (*plug)->name) ) return *plug;
        plug++;
    }
    return NULL;
}

int filter_plugin_global_init(void) {
    int r;
    const filter_plugin** plug;

    plug = filter_plugin_list;
    while(*plug != NULL) {
        if( (r = (*plug)->init()) != 0) return r;
        plug++;
    }

    return 0;
}

void filter_plugin_global_deinit(void) {
    const filter_plugin** plug = filter_plugin_list;

    while(*plug != NULL) {
        (*plug)->deinit();
        plug++;
    }

    return;
}


