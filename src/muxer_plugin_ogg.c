#include "muxer_plugin_ogg.h"
#include "muxer_caps.h"

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

static STRBUF_CONST(plugin_name,"ogg");

struct muxer_plugin_ogg_userdata {
    const muxer_plugin* plugin;
    void* handle;
    uint8_t chaining;
    taglist config;
};

typedef struct muxer_plugin_ogg_userdata muxer_plugin_ogg_userdata;

static size_t muxer_plugin_ogg_size(void) {
    return sizeof(muxer_plugin_ogg_userdata);
}

static int muxer_plugin_ogg_create(void* ud) {
    muxer_plugin_ogg_userdata* userdata = (muxer_plugin_ogg_userdata*)ud;

    userdata->plugin = NULL;
    userdata->handle = NULL;
    taglist_init(&userdata->config);
    userdata->chaining = 1;

    return 0;
}

static int muxer_plugin_ogg_reset(void* ud) {
    muxer_plugin_ogg_userdata* userdata = (muxer_plugin_ogg_userdata*)ud;

    return userdata->plugin->reset(userdata->handle);
}


static void muxer_plugin_ogg_close(void* ud) {
    muxer_plugin_ogg_userdata* userdata = (muxer_plugin_ogg_userdata*)ud;

    muxer_plugin_ogg_reset(userdata);
    taglist_free(&userdata->config);
}

static int muxer_plugin_ogg_open(void* ud, const packet_source* source, const segment_receiver* dest) {
    muxer_plugin_ogg_userdata* userdata = (muxer_plugin_ogg_userdata*)ud;
    int r;
    const tag* t;
    size_t i;
    size_t len;

    if(userdata->plugin == NULL) { /* first open, or a codec change required re-open */
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
    } else if(userdata->plugin == &muxer_plugin_ogg_opus && source->codec != CODEC_TYPE_OPUS) {
        userdata->plugin->close(userdata->handle);
        free(userdata->handle);
        userdata->handle = NULL;
        switch(source->codec) {
            case CODEC_TYPE_FLAC: {
                userdata->plugin = &muxer_plugin_ogg_flac;
                break;
            }
            default: {
                LOG1("unsupported codec %s",codec_name(source->codec));
                return -1;
            }
        }
    } else if(userdata->plugin == &muxer_plugin_ogg_flac && source->codec != CODEC_TYPE_FLAC) {
        userdata->plugin->close(userdata->handle);
        free(userdata->handle);
        userdata->handle = NULL;
        switch(source->codec) {
            case CODEC_TYPE_OPUS: {
                userdata->plugin = &muxer_plugin_ogg_opus;
                break;
            }
            default: {
                LOG1("unsupported codec %s",codec_name(source->codec));
                return -1;
            }
        }
    }

    if(userdata->handle == NULL) {
        userdata->handle = malloc(userdata->plugin->size());
        if(userdata->handle == NULL) {
            userdata->plugin = NULL;
            LOGERRNO("error allocating sub-plugin");
            return -1;
        }
        if( (r = userdata->plugin->create(userdata->handle)) != 0) return r;

        len = taglist_len(&userdata->config);
        for(i=0;i<len;i++) {
            t = taglist_get_tag(&userdata->config,i);
            if( (r = userdata->plugin->config(userdata->handle,&t->key,&t->value)) != 0) return r;
        }
    }

    return userdata->plugin->open(userdata->handle, source, dest);
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

static uint32_t muxer_plugin_ogg_get_caps(void* ud) {
    muxer_plugin_ogg_userdata* userdata = (muxer_plugin_ogg_userdata*)ud;

    if(userdata->plugin == NULL) return MUXER_CAP_GLOBAL_HEADERS;
    return userdata->plugin->get_caps(userdata->handle);
}

static int muxer_plugin_ogg_config(void* ud, const strbuf* key, const strbuf* value) {
    muxer_plugin_ogg_userdata* userdata = (muxer_plugin_ogg_userdata*)ud;

    /* the one piece of config shared by everything is whether chaining is
     * enabled or not */

    if(strbuf_equals_cstr(key,"chaining")) {
        if(strbuf_truthy(value)) {
            userdata->chaining = 1;
            goto complete;
        }
        if(strbuf_falsey(value)) {
            userdata->chaining = 0;
            goto complete;
        }
        LOGS("unsupported value for chaining: %.*s",(*value));
        return -1;
    }

    complete:
    /* store the config for later, when we instantiate a plugin */
    return taglist_add(&userdata->config,key,value);
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
    muxer_plugin_ogg_flac.deinit();
}

static int muxer_plugin_ogg_get_segment_info(const void* ud, const packet_source_info* s, const segment_receiver* dest, packet_source_params* i) {
    (void)ud;

    segment_source_info s_info = SEGMENT_SOURCE_INFO_ZERO;
    segment_params s_params = SEGMENT_PARAMS_ZERO;

    s_info.time_base = s->time_base;
    s_info.frame_len = s->frame_len;

    dest->get_segment_info(dest->handle,&s_info,&s_params);
    i->segment_length = s_params.segment_length;
    i->packets_per_segment = s_params.packets_per_segment;
    return 0;
}

const muxer_plugin muxer_plugin_ogg = {
    plugin_name,
    muxer_plugin_ogg_size,
    muxer_plugin_ogg_init,
    muxer_plugin_ogg_deinit,
    muxer_plugin_ogg_create,
    muxer_plugin_ogg_config,
    muxer_plugin_ogg_open,
    muxer_plugin_ogg_close,
    muxer_plugin_ogg_submit_packet,
    muxer_plugin_ogg_submit_tags,
    muxer_plugin_ogg_flush,
    muxer_plugin_ogg_reset,
    muxer_plugin_ogg_get_caps,
    muxer_plugin_ogg_get_segment_info,
};

