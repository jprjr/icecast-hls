#include "encoder_plugin.h"

#ifndef ENCODER_PLUGIN_AVCODEC
#define ENCODER_PLUGIN_AVCODEC 0
#endif

#ifndef ENCODER_PLUGIN_EXHALE
#define ENCODER_PLUGIN_EXHALE 0
#endif

#ifndef ENCODER_PLUGIN_FDK_AAC
#define ENCODER_PLUGIN_FDK_AAC 0
#endif

#ifndef ENCODER_PLUGIN_OPUS
#define ENCODER_PLUGIN_OPUS 0
#endif

#if ENCODER_PLUGIN_AVCODEC
#include "encoder_plugin_avcodec.h"
#endif

#if ENCODER_PLUGIN_EXHALE
#include "encoder_plugin_exhale.h"
#endif

#if ENCODER_PLUGIN_FDK_AAC
#include "encoder_plugin_fdk_aac.h"
#endif

#if ENCODER_PLUGIN_OPUS
#include "encoder_plugin_opus.h"
#endif

const encoder_plugin* encoder_plugin_list[] = {
#if ENCODER_PLUGIN_AVCODEC
    &encoder_plugin_avcodec,
#endif
#if ENCODER_PLUGIN_EXHALE
    &encoder_plugin_exhale,
#endif
#if ENCODER_PLUGIN_FDK_AAC
    &encoder_plugin_fdk_aac,
#endif
#if ENCODER_PLUGIN_OPUS
    &encoder_plugin_opus,
#endif
    NULL,
};

const encoder_plugin* encoder_plugin_get(const strbuf* name) {
    const encoder_plugin** plug = encoder_plugin_list;
    while(*plug != NULL) {
        if( strbuf_equals(name, &((*plug)->name)) ) return *plug;
        plug++;
    }
    return NULL;
}

int encoder_plugin_global_init(void) {
    int r;
    const encoder_plugin** plug;

    plug = encoder_plugin_list;
    while(*plug != NULL) {
        if( (r = (*plug)->init()) != 0) return r;
        plug++;
    }

    return 0;
}

void encoder_plugin_global_deinit(void) {
    const encoder_plugin** plug = encoder_plugin_list;

    while(*plug != NULL) {
        (*plug)->deinit();
        plug++;
    }

    return;
}



