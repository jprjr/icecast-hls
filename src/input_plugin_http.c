#include "input_plugin_http.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <ctype.h>

struct http_userdata {
    strbuf url;
    uint8_t do_icecast;
};

typedef struct http_userdata http_userdata;

static int plugin_init(void) {
    return 0;
}

static void plugin_deinit(void) {
    return;
}

static void* plugin_create(void) {
    http_userdata* userdata = malloc(sizeof(http_userdata));
    if(userdata == NULL) return userdata;
    userdata->do_icecast = 0;
    strbuf_init(&userdata->url);
    return userdata;
}

static int plugin_config(void* ud, const strbuf* key, const strbuf* value) {
    int r;
    http_userdata* userdata = (http_userdata*)ud;

    if(strbuf_equals_cstr(key,"icecast")) {
        if(strbuf_truthy(value)) {
            userdata->do_icecast = 1;
            return 0;
        }
        if(strbuf_falsey(value)) {
            userdata->do_icecast = 0;
            return 0;
        }
        fprintf(stderr,"Unknown value '%.*s' for key '%.*s'\n",
           (int)value->len, (const char*)value->x,
           (int)key->len,   (const char*)key->x);
        return -1;
    }

    if(strbuf_equals_cstr(key,"url")) {
        if( (r = strbuf_copy(&userdata->url,value)) != 0) return r;
        return 0;
    }

    fprintf(stderr,"error [http]: unknown key '%.*s'\n",
       (int)key->len, (const char *)key->x);
    return -1;
}

const input_plugin input_plugin_http = {
    { .a = 0, .len = 4, .x = (uint8_t*)"http" },
    plugin_init,
    plugin_deinit,
    plugin_create,
    plugin_config,
    NULL, /* open */
    NULL, /* close */
    NULL, /* read */
};
