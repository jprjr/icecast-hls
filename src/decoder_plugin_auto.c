#include "decoder_plugin_auto.h"

#include "strbuf.h"

#include <stdlib.h>

#define LOG_PREFIX "[decoder:auto]"
#include "logger.h"

static STRBUF_CONST(plugin_name, "auto");

static STRBUF_CONST(plugin_name_miniflac, "miniflac");
static STRBUF_CONST(plugin_name_avcodec, "avcodec");

struct plugin_userdata {
    const decoder_plugin* plugin;
    void* handle;
    taglist config;
};

typedef struct plugin_userdata plugin_userdata;

static int plugin_init(void) { return 0; }
static void plugin_deinit(void) { return; }
static size_t plugin_size(void) { return sizeof(plugin_userdata); }

static int plugin_create(void *ud) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    taglist_init(&userdata->config);
    userdata->plugin = NULL;
    userdata->handle = NULL;
    return 0;
}

static void plugin_close(void* ud) {
    plugin_userdata* userdata = (plugin_userdata*)ud;

    if(userdata->handle != NULL) {
        userdata->plugin->close(userdata->handle);
        free(userdata->handle);
        userdata->handle = NULL;
        userdata->plugin = NULL;
    }

    taglist_free(&userdata->config);
}

static int plugin_open(void* ud, const packet_source* src, const frame_receiver* dest) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    int r;
    const tag* t;
    size_t i;
    size_t len;

    log_debug("open, codec=%s",codec_name(src->codec));

    switch(src->codec) {
        case CODEC_TYPE_FLAC: {
            userdata->plugin = decoder_plugin_get(&plugin_name_miniflac); break;
        }
        default: userdata->plugin = decoder_plugin_get(&plugin_name_avcodec);
    }

    if(userdata->plugin == NULL) {
        log_error("unable to find plugin to decode %s",
          codec_name(src->codec));
        return -1;
    }

    userdata->handle = malloc(userdata->plugin->size());
    if(userdata->handle == NULL) {
        userdata->plugin = NULL;
        logs_fatal("error creating plugin userdata");
        return -1;
    }
    if( (r = userdata->plugin->create(userdata->handle)) != 0) {
        return r;
    }

    len = taglist_len(&userdata->config);
    for(i=0;i<len;i++) {
        t = taglist_get_tag(&userdata->config,i);
        if( (r = userdata->plugin->config(userdata->handle,&t->key,&t->value)) != 0) return r;
    }

    return userdata->plugin->open(userdata->handle,src,dest);
}

static int plugin_decode(void* ud, const packet* src, const frame_receiver* dest) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    return userdata->plugin->decode(userdata->handle,src,dest);
}

static int plugin_flush(void* ud, const frame_receiver* dest) {
    plugin_userdata* userdata = (plugin_userdata*)ud;

    return userdata->plugin->flush(userdata->handle,dest);
}

static int plugin_reset(void* ud) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    logs_info("resetting");

    userdata->plugin->close(userdata->handle);
    userdata->handle = NULL;
    userdata->plugin = NULL;

    return 0;
}

static int plugin_config(void* ud, const strbuf* key, const strbuf* value) {
    /* store the config for later, when we instantiate a plugin */
    plugin_userdata* userdata = (plugin_userdata*)ud;
    log_debug("configuring %.*s=%.*s",
      (int)key->len,
      (const char *)key->x,
      (int)value->len,
      (const char *)value->x);
    return taglist_add(&userdata->config,key,value);
}


const decoder_plugin decoder_plugin_auto = {
    &plugin_name,
    plugin_size,
    plugin_init,
    plugin_deinit,
    plugin_create,
    plugin_config,
    plugin_open,
    plugin_close,
    plugin_decode,
    plugin_flush,
    plugin_reset,
};

