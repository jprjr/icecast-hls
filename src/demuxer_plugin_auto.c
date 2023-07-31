#include "demuxer_plugin_auto.h"
#include "input_plugin.h"

#include <stdlib.h>
#include <string.h>

#include "strbuf.h"
#include "membuf.h"

#define BUFFER_SIZE 8192

#define LOG_PREFIX "[demuxer:auto]"
#include "logger.h"

static STRBUF_CONST(input_plugin_name, "wrapper");
static STRBUF_CONST(plugin_name, "auto");

static STRBUF_CONST(plugin_name_ogg, "ogg");
static STRBUF_CONST(plugin_name_flac, "flac");
static STRBUF_CONST(plugin_name_avformat, "avformat");

struct plugin_userdata {
    input* input;

    membuf buffer;
    size_t pos;
    input input_wrapper;

    const demuxer_plugin* plugin;
    void* plugin_handle;
    taglist config;
};

typedef struct plugin_userdata plugin_userdata;

static size_t input_plugin_wrapper_read(void* ud, void* dest, size_t len, const tag_handler*) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    size_t i;
    size_t m;
    uint8_t* d = (uint8_t*)dest;

    i = 0;
    while(len) {
        m = len > userdata->buffer.len - userdata->pos ? userdata->buffer.len - userdata->pos : len;

        if(m == 0) {
            userdata->pos = 0;
            userdata->buffer.len = input_read(userdata->input,userdata->buffer.x,len > BUFFER_SIZE ? BUFFER_SIZE : len);

            if(userdata->buffer.len == 0) {
                return i;
            }
            m = userdata->buffer.len;
        }

        memcpy(&d[i],&userdata->buffer.x[userdata->pos],m);
        userdata->pos += m;
        i += m;
        len -= m;
    }

    return i;
}

static const input_plugin input_plugin_wrapper = {
    input_plugin_name,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    input_plugin_wrapper_read,
};

static size_t plugin_size(void) {
    return sizeof(plugin_userdata);
}

static int plugin_open(void* ud, input* in) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    const strbuf* plugin_name = NULL;
    const tag* t;
    size_t i;
    size_t len;
    int r;

    userdata->input = in;
    userdata->input_wrapper.plugin = &input_plugin_wrapper;
    userdata->input_wrapper.userdata = userdata;
    userdata->input_wrapper.counter = 0;
    userdata->input_wrapper.ts = in->ts;

    if(membuf_ready(&userdata->buffer, BUFFER_SIZE) != 0) {
        logs_fatal("out of memory");
        return -1;
    }

    while(userdata->buffer.len < 4) {
        if( (len = input_read(userdata->input, userdata->buffer.x, BUFFER_SIZE)) == 0) {
            logs_error("unable to read minimum probe bytes (4)");
            return -1;
        }
        userdata->buffer.len += len;
    }

    if(userdata->buffer.len >= 4) {
        if(memcmp(&userdata->buffer.x[0],"OggS",4) == 0) {
            logs_debug("detected format ogg");
            plugin_name = &plugin_name_ogg;
        } else if(memcmp(&userdata->buffer.x[0],"fLaC",4) ==0) {
            logs_debug("detected format FLAC");
            plugin_name = &plugin_name_flac;
        } else {
            logs_debug("unknown format, fallbing back to avformat");
            plugin_name = &plugin_name_avformat;
        }
    }

    if(plugin_name == NULL) {
        logs_error("unable to determine format");
        return -1;
    }

    if( (userdata->plugin = demuxer_plugin_get(plugin_name)) == NULL) {
        log_error("unable to load plugin %.*s",
          (int)plugin_name->len,(const char *)plugin_name->x);
        return -1;
    }

    userdata->plugin_handle = malloc(userdata->plugin->size());
    if(userdata->plugin_handle == NULL) {
        userdata->plugin = NULL;
        logs_fatal("unable to allocate plugin");
        return -1;
    }

    if( (r = userdata->plugin->create(userdata->plugin_handle)) != 0) {
        return r;
    }

    len = taglist_len(&userdata->config);
    for(i=0;i<len;i++) {
        t = taglist_get_tag(&userdata->config,i);
        if( (r = userdata->plugin->config(userdata->plugin_handle,&t->key,&t->value)) != 0) return r;
    }

    return userdata->plugin->open(userdata->plugin_handle, &userdata->input_wrapper);
}

static int plugin_run(void* ud, const tag_handler* t, const packet_receiver* r) {
    plugin_userdata* userdata = (plugin_userdata*)ud;

    return userdata->plugin->run(userdata->plugin_handle,t,r);
}

static int plugin_create(void *ud) {
    plugin_userdata* userdata = (plugin_userdata*)ud;

    userdata->input = NULL;
    userdata->plugin = NULL;
    userdata->plugin_handle = NULL;
    userdata->pos = 0;

    membuf_init(&userdata->buffer);
    taglist_init(&userdata->config);

    return 0;
}

static void plugin_close(void* ud) {
    plugin_userdata* userdata = (plugin_userdata*)ud;

    if(userdata->plugin != NULL) {
        userdata->plugin->close(userdata->plugin_handle);
        free(userdata->plugin_handle);
        userdata->plugin = NULL;
        userdata->plugin_handle = NULL;
    }
    taglist_free(&userdata->config);
    membuf_free(&userdata->buffer);
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

static int plugin_init(void) {
    return 0;
}

static void plugin_deinit(void) {
    return;
}

const demuxer_plugin demuxer_plugin_auto = {
    plugin_name,
    plugin_size,
    plugin_init,
    plugin_deinit,
    plugin_create,
    plugin_config,
    plugin_open,
    plugin_close,
    plugin_run,
};
