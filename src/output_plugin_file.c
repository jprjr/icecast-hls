#include "output_plugin_file.h"

#include "strbuf.h"
#include "thread.h"

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

/* keep a global counter for writing out picture files. We could
 * have multiple file destinations going to the same folder,
 * this ensures multiple threads don't stop on each other */

static thread_atomic_uint_t counter;

struct file_userdata {
    strbuf filename;
    strbuf basename;
    FILE *f;
};

typedef struct file_userdata file_userdata;

static FILE* file_open(const strbuf* filename) {
    FILE* f;
#ifdef DR_WINDOWS
    strbuf w;
    w.x = NULL; w.a = 0; w.len = 0;
#endif
    f = NULL;

#ifdef DR_WINDOWS
    /* since we'll include the terminating zero we don't
     * need to manually terminate this after calling strbuf_wide */
    if(strbuf_wide(&w,&t) != 0) goto cleanup;
    f = _wfopen((wchar_t *)w.x, L"wb");
#else
    f = fopen((const char *)filename->x,"wb");
#endif
    if(f == NULL) {
        printf("[output:file] error opening file: %.*s\n",(int)filename->len,filename->x);
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
    strbuf_free(&ud->basename);
    free(ud);
}

static int plugin_init(void) {
    thread_atomic_uint_store(&counter,0);
    return 0;
}

static void plugin_deinit(void) {
    return;
}

static void* plugin_create(void) {
    file_userdata* userdata = (file_userdata*)malloc(sizeof(file_userdata));
    if(userdata == NULL) return userdata;

    userdata->f = NULL;
    strbuf_init(&userdata->filename);
    strbuf_init(&userdata->basename);
    return userdata;
}

static int plugin_open(void* ud, const outputconfig* config) {
    outputinfo info = OUTPUTINFO_ZERO;
    strbuf tmp = STRBUF_ZERO;
#if defined(_WIN32) || defined(_WIN64) || defined(_MSC_VER)
    strbuf tmp2 = STRBUF_ZERO;
#endif
    strbuf* t = NULL;

    file_userdata* userdata = (file_userdata*)ud;
    if(userdata->filename.len == 0) return -1;
    userdata->f = file_open(&userdata->filename);
    if(userdata->f == NULL) return -1;

    if(strbuf_rchrbuf(&tmp,&userdata->filename,'/') == 0
#if defined(_WIN32) || defined(_WIN64) || defined(_MSC_VER)
        || strbuf_rchrbuf(&tmp2,&userdata->filename,'\\') == 0
#endif
    ) {
        t = &tmp;
#if defined(_WIN32) || defined(_WIN64) || defined(_MSC_VER)
        if(tmp2.x > tmp.x) t = &tmp2;
#endif
        if(membuf_append(&userdata->basename,userdata->filename.x,userdata->filename.len - t->len + 1) != 0) return -1;
    }

    return config->info.submit(config->info.userdata, &info);
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

static int plugin_submit_segment(void* ud, const segment* seg) {
    file_userdata* userdata = (file_userdata*)ud;
    if(seg == NULL) return 0;

    return fwrite(seg->data,1,seg->len,userdata->f) == seg->len ? 0 : -1;
}

static int plugin_submit_picture(void* ud, const picture* src, picture* out) {
    int r;
    file_userdata* userdata = (file_userdata*)ud;
    strbuf dest_filename = STRBUF_ZERO;
    int picture_id;
    const char *fmt_str;
    FILE* f = NULL;

    picture_id = thread_atomic_uint_inc(&counter) % 100000000;

#define TRY(x) if( (r = (x)) != 0 ) goto cleanup

    if(userdata->basename.len > 0) {
        TRY(strbuf_cat(&dest_filename,&userdata->basename));
    }

    TRY(strbuf_readyplus(&dest_filename,13));

    if(strbuf_ends_cstr(&src->mime,"/png")) {
        fmt_str = "%08u.png";
    } else if(strbuf_ends_cstr(&src->mime,"/jpg") | strbuf_ends_cstr(&src->mime,"jpeg")) {
        fmt_str = "%08u.jpg";
    } else if(strbuf_ends_cstr(&src->mime,"/gif")) {
        fmt_str = "%08u.gif";
    } else {
        fprintf(stderr,"[output:file] unknown image mime type %.*s\n",
          (int)src->mime.len,(char*)src->mime.x);
        r = 0; goto cleanup;
    }

    snprintf((char *)&dest_filename.x[userdata->basename.len],13,fmt_str,picture_id);
    dest_filename.len += 13;
    /* snprintf terminated the string so we won't manually terminate it */

    f = file_open(&dest_filename);
    if(f == NULL) {
        r = -1;
        goto cleanup;
    }
    if(fwrite(src->data.x,1,src->data.len,f) != src->data.len) {
        r = -1;
        goto cleanup;
    }

    TRY(membuf_append(&out->mime,"-->",3));
    TRY(membuf_append(&out->data,&dest_filename.x[userdata->basename.len],12));
    TRY(strbuf_copy(&out->desc,&src->desc));
    r = 0;

    cleanup:
    if(f != NULL) fclose(f);
    strbuf_free(&dest_filename);
    return r;
}

static int plugin_set_time(void* userdata, const ich_time* now) {
    (void)userdata;
    (void)now;
    return 0;
}

static int plugin_flush(void* userdata) {
    (void)userdata;
    return 0;
}

const output_plugin output_plugin_file = {
    { .a = 0, .len = 4, .x = (uint8_t*)"file" },
    plugin_init,
    plugin_deinit,
    plugin_create,
    plugin_config,
    plugin_open,
    plugin_close,
    plugin_set_time,
    plugin_submit_segment,
    plugin_submit_picture,
    plugin_flush,
};
