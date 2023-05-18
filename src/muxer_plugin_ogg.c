#include "muxer_plugin_ogg.h"

#include <stdlib.h>
#include <errno.h>
#include <string.h>

#define LOG0(s) fprintf(stderr,"[muxer:ogg] "s"\n")
#define LOG1(s, a) fprintf(stderr,"[muxer:ogg] "s"\n", (a))
#define LOG2(s, a, b) fprintf(stderr,"[muxer:ogg] "s"\n", (a), (b))
#define LOGS(s, a) LOG2(s, (int)(a).len, (const char *)(a).x )

#define LOGERRNO(s) LOG1(s": %s", strerror(errno))

#define TRYNULL(exp, act) if( (exp) == NULL) { act; r=-1; goto cleanup; }
#define TRY(exp, act) if(!(exp)) { act; r=-1; goto cleanup ; }
#define TRY0(exp, act) if( (r = (exp)) != 0 ) { act; goto cleanup; }
#define TRYS(exp) TRY0(exp, LOG0("out of memory"); abort())

#include "muxer_plugin_ogg_opus.h"
#include "muxer_plugin_ogg_flac.h"

struct muxer_plugin_ogg_userdata {
    const muxer_plugin* plugin;
    void* handle;
};

typedef struct muxer_plugin_ogg_userdata muxer_plugin_ogg_userdata;

static void* muxer_plugin_ogg_create(void) {
    muxer_plugin_ogg_userdata* userdata = (muxer_plugin_ogg_userdata*)malloc(sizeof(muxer_plugin_ogg_userdata));
    if(userdata == NULL) return NULL;
    userdata->plugin = NULL;
    userdata->handle= NULL;
    return userdata;
}

static void muxer_plugin_ogg_close(void* ud) {
    muxer_plugin_ogg_userdata* userdata = (muxer_plugin_ogg_userdata*)ud;
    if(userdata->plugin != NULL) {
        userdata->plugin->close(userdata->handle);
    }
    free(userdata);
}

static int muxer_plugin_ogg_open(void* ud, const packet_source* source, const segment_receiver* dest) {
    muxer_plugin_ogg_userdata* userdata = (muxer_plugin_ogg_userdata*)ud;
    switch(source->codec) {
        case CODEC_TYPE_OPUS: {
            userdata->plugin = &muxer_plugin_ogg_opus;
            break;
        }
        case CODEC_TYPE_FLAC: {
            userdata->plugin = &muxer_plugin_ogg_flac;
            break;
        }
        default: {
            LOG1("unsupported codec %s",codec_name(source->codec));
            return -1;
        }
    }

    userdata->handle = userdata->plugin->create();
    if(userdata->handle == NULL) {
        LOGERRNO("error allocating sub-plugin");
        return -1;
    }

    return userdata->plugin->open(userdata->handle, source, dest);
}

static int muxer_plugin_ogg_submit_dsi(void* ud, const strbuf* data, const segment_receiver* dest) {
    muxer_plugin_ogg_userdata* userdata = (muxer_plugin_ogg_userdata*)ud;
    return userdata->plugin->submit_dsi(userdata->handle,data,dest);
}

static int muxer_plugin_ogg_submit_packet(void* ud, const packet* p, const segment_receiver* dest) {
    muxer_plugin_ogg_userdata* userdata = (muxer_plugin_ogg_userdata*)ud;
    return userdata->plugin->submit_packet(userdata->handle,p,dest);
}

static int muxer_plugin_ogg_submit_tags(void* ud, const taglist* t, const segment_receiver* dest) {
    muxer_plugin_ogg_userdata* userdata = (muxer_plugin_ogg_userdata*)ud;
    return userdata->plugin->submit_tags(userdata->handle,t,dest);
}

static int muxer_plugin_ogg_flush(void* ud, const segment_receiver* dest) {
    muxer_plugin_ogg_userdata* userdata = (muxer_plugin_ogg_userdata*)ud;
    return userdata->plugin->flush(userdata->handle,dest);
}

static int muxer_plugin_ogg_get_caps(void* ud, packet_receiver_caps* caps) {
    (void)ud;
    caps->has_global_header = 1;
    return 0;
}

static int muxer_plugin_ogg_config(void* ud, const strbuf* key, const strbuf* value) {
    (void)ud;
    (void)value;
    LOGS("unknown key %.*s",(*key));
    return -1;
}

static int muxer_plugin_ogg_init(void) {
    int r = -1;

    TRY0(muxer_plugin_ogg_opus.init(), LOG0("error initializing ogg_opus"));
    TRY0(muxer_plugin_ogg_flac.init(), LOG0("error initializing ogg_flac"));

    r = 0;
    cleanup:
    return r;
}

static void muxer_plugin_ogg_deinit(void) {
    muxer_plugin_ogg_opus.deinit();
}

const muxer_plugin muxer_plugin_ogg = {
    {.a = 0, .len = 3, .x = (uint8_t*)"ogg" },
    muxer_plugin_ogg_init,
    muxer_plugin_ogg_deinit,
    muxer_plugin_ogg_create,
    muxer_plugin_ogg_config,
    muxer_plugin_ogg_open,
    muxer_plugin_ogg_close,
    muxer_plugin_ogg_submit_dsi,
    muxer_plugin_ogg_submit_packet,
    muxer_plugin_ogg_submit_tags,
    muxer_plugin_ogg_flush,
    muxer_plugin_ogg_get_caps,
};

