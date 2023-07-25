#include "input_plugin_file.h"

#include "strbuf.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if defined(_WIN32) || defined(_WIN64) || defined(_MSC_VER)
#define DR_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#endif

struct file_userdata {
    strbuf filename;
    FILE *f;
};

typedef struct file_userdata file_userdata;

static FILE* file_open(const strbuf* filename) {
    FILE* f = NULL;
#ifdef DR_WINDOWS
    strbuf w = STRBUF_ZERO;

    /* since we'll include the terminating zero we don't
     * need to manually terminate this after calling strbuf_wide */
    if(strbuf_wide(&w,filename) != 0) goto cleanup;
    f = _wfopen((wchar_t *)w.x, L"rb");
#else
    f = fopen((const char *)filename->x,"rb");
#endif
    if(f == NULL) {
        printf("error opening file: %.*s\n",(int)filename->len,filename->x);
    }

#ifdef DR_WINDOWS
    cleanup:
    strbuf_free(&w);
#endif

    return f;
}

static void plugin_close(void* userdata) {
    file_userdata* ud = (file_userdata*)userdata;
    if(ud->f != NULL) {
        fclose(ud->f);
        ud->f = NULL;
    }
    strbuf_free(&ud->filename);
}

static int plugin_init(void) {
    return 0;
}

static void plugin_deinit(void) {
    return;
}

static size_t plugin_size(void) {
    return sizeof(file_userdata);
}

static int plugin_create(void* ud) {
    file_userdata* userdata = (file_userdata*)ud;

    userdata->f = NULL;
    strbuf_init(&userdata->filename);
    return 0;
}

static int plugin_open(void* ud) {
    file_userdata* userdata = (file_userdata*)ud;
    if(userdata->filename.len == 0) return -1;
    userdata->f = file_open(&userdata->filename);
    if(userdata->f == NULL) return -1;
    return 0;
}

static int plugin_config(void* ud, const strbuf* key, const strbuf* val) {
    int r;
    file_userdata* userdata = (file_userdata*)ud;

    if(strbuf_equals_cstr(key,"file")) {
        if( (r = strbuf_copy(&userdata->filename,val)) != 0) return r;
        if( (r = strbuf_term(&userdata->filename)) != 0) return r;
        return 0;
    }
    fprintf(stderr,"file plugin: unknown key \"%.*s\"\n",(int)key->len,(const char *)key->x);
    return -1;
}

static size_t plugin_read(void *ud, void* dest, size_t len, const tag_handler* handler) {
    file_userdata* userdata = (file_userdata*)ud;
    (void)handler;
    return fread(dest,1,len,userdata->f);
}

const input_plugin input_plugin_file = {
    { .a = 0, .len = 4, .x = (uint8_t*)"file" },
    plugin_size,
    plugin_init,
    plugin_deinit,
    plugin_create,
    plugin_config,
    plugin_open,
    plugin_close,
    plugin_read,
};
