#include "muxer_plugin_fmp4.h"
#include "muxer_plugin_adts.h"
#include "muxer_plugin_packedaudio.h"
#include "muxer_plugin_passthrough.h"
#include "muxer_plugin_ogg.h"

const muxer_plugin* muxer_plugin_list[] = {
    &muxer_plugin_fmp4,
    &muxer_plugin_packed_audio,
    &muxer_plugin_adts,
    &muxer_plugin_passthrough,
    &muxer_plugin_ogg,
    NULL,
};

const muxer_plugin* muxer_plugin_get(const strbuf* name) {
    const muxer_plugin** plug = muxer_plugin_list;
    while(*plug != NULL) {
        if( strbuf_equals(name, &((*plug)->name)) ) return *plug;
        plug++;
    }
    return NULL;
}

int muxer_plugin_global_init(void) {
    int r;
    const muxer_plugin** plug;

    plug = muxer_plugin_list;
    while(*plug != NULL) {
        if( (r = (*plug)->init()) != 0) return r;
        plug++;
    }

    return 0;
}

void muxer_plugin_global_deinit(void) {
    const muxer_plugin** plug = muxer_plugin_list;

    while(*plug != NULL) {
        (*plug)->deinit();
        plug++;
    }

    return;
}




