#include "demuxer_plugin.h"

#include "demuxer_plugin_auto.h"
#include "demuxer_plugin_flac.h"
#include "demuxer_plugin_ogg.h"

#ifndef DEMUXER_PLUGIN_AVFORMAT
#define DEMUXER_PLUGIN_AVFORMAT 0
#endif

#if DEMUXER_PLUGIN_AVFORMAT
#include "demuxer_plugin_avformat.h"
#endif

const demuxer_plugin* demuxer_plugin_list[] = {
    &demuxer_plugin_auto,
    &demuxer_plugin_flac,
    &demuxer_plugin_ogg,
#if DEMUXER_PLUGIN_AVFORMAT
    &demuxer_plugin_avformat,
#endif
    NULL,
};

const demuxer_plugin* demuxer_plugin_get(const strbuf* name) {
    const demuxer_plugin** plug = demuxer_plugin_list;
    while(*plug != NULL) {
        if( strbuf_equals(name, &((*plug)->name)) ) return *plug;
        plug++;
    }
    return NULL;
}

int demuxer_plugin_global_init(void) {
    int r;
    const demuxer_plugin** plug;

    plug = demuxer_plugin_list;
    while(*plug != NULL) {
        if( (r = (*plug)->init()) != 0) return r;
        plug++;
    }

    return 0;
}

void demuxer_plugin_global_deinit(void) {
    const demuxer_plugin** plug = demuxer_plugin_list;

    while(*plug != NULL) {
        (*plug)->deinit();
        plug++;
    }

    return;
}


