#include "output_plugin_file.h"

#include "strbuf.h"
#include "thread.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

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

#define LOG0(s) fprintf(stderr,"[output:file] "s"\n")
#define LOG1(s, a) fprintf(stderr,"[output:file] "s"\n", (a))
#define LOG2(s, a, b) fprintf(stderr,"[output:file] "s"\n", (a), (b))
#define LOG3(s, a, b, c) fprintf(stderr,"[output:file] "s"\n", (a), (b), (c))
#define LOGS(s, a) LOG2(s, (int)(a).len, (const char *)(a).x )
#define LOGS1(s, a, b) LOG3(s, (int)(a).len, (const char *)(a).x, b )
#define LOGERRNO(s) LOG1(s": %s", strerror(errno))

#define TRY(exp, act) if(!(exp)) { act; }
#define TRY0(exp, act) if( (r = (exp)) != 0 ) { act; goto cleanup; }
#define TRYNULL(exp, act) if( (exp) == NULL) { act; r=-1; goto cleanup; }
#define TRYS(exp) TRY0(exp, LOG0("out of memory"); abort())

/* keep a global counter for writing out picture files. We could
 * have multiple file destinations going to the same folder,
 * this ensures multiple threads don't stop on each other */

static thread_atomic_uint_t counter;

static STRBUF_CONST(plugin_name,"file");

struct file_userdata {
    strbuf filename;
    strbuf basename;
    unsigned int fragment_duration;
    FILE *f;
};

typedef struct file_userdata file_userdata;

static FILE* file_open(const strbuf* filename) {
    FILE* f = NULL;
    int r = 0;
#ifdef DR_WINDOWS
    strbuf w = STRBUF_ZERO;
    /* since we'll include the terminating zero we don't
     * need to manually terminate this after calling strbuf_wide */
    TRYS(strbuf_wide(&w,filename))
    TRYNULL(f = _wfopen((wchar_t *)w.x, L"wb"),
      LOGS1("error opening file %.*s: %s", (*filename), strerror(errno)));
#else
    TRYNULL(f = fopen((const char *)filename->x,"wb"),
      LOGS1("error opening file %.*s: %s", (*filename), strerror(errno)));
#endif

    cleanup:
#ifdef DR_WINDOWS
    strbuf_free(&w);
#endif

    (void)r;
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
}

static int plugin_init(void) {
    thread_atomic_uint_store(&counter,0);
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
    strbuf_init(&userdata->basename);
    userdata->fragment_duration = 1000;

    return 0;
}

static int plugin_get_segment_info(const void*  ud, const segment_source_info* info, segment_params* params) {
    file_userdata* userdata = (file_userdata*)ud;
    (void)info;

    params->segment_length = userdata->fragment_duration;

    return 0;
}

static int plugin_open(void* ud, const segment_source* source) {
    int r;
    strbuf tmp = STRBUF_ZERO;
    (void)source;
#if defined(_WIN32) || defined(_WIN64) || defined(_MSC_VER)
    strbuf tmp2 = STRBUF_ZERO;
#endif
    strbuf* t = NULL;
    r = 0;

    file_userdata* userdata = (file_userdata*)ud;

    if(userdata->f != NULL) { /* already open, probably called after a flush */
        return 0;
    }

    TRY(userdata->filename.len != 0, LOG0("no filename given"); r=-1; goto cleanup);
    /* error already logged in file open, we'll just flush stderr */
    TRYNULL(userdata->f = file_open(&userdata->filename), fflush(stderr));

    if(strbuf_rchrbuf(&tmp,&userdata->filename,'/') == 0
#if defined(_WIN32) || defined(_WIN64) || defined(_MSC_VER)
        || strbuf_rchrbuf(&tmp2,&userdata->filename,'\\') == 0
#endif
    ) {
        t = &tmp;
#if defined(_WIN32) || defined(_WIN64) || defined(_MSC_VER)
        if(tmp2.x > tmp.x) t = &tmp2;
#endif
        TRYS(membuf_append(&userdata->basename,userdata->filename.x,userdata->filename.len - t->len));
    }

    cleanup:
    return r;
}

static int plugin_config(void* ud, const strbuf* key, const strbuf* val) {
    int r;
    file_userdata* userdata = (file_userdata*)ud;

    if(strbuf_equals_cstr(key,"file")) {
        TRYS(strbuf_copy(&userdata->filename,val));
        TRYS(strbuf_term(&userdata->filename));
        return 0;
    }

    if(strbuf_equals_cstr(key,"fragment-duration") || strbuf_equals_cstr(key,"fragment duration")) {
        errno = 0;
        userdata->fragment_duration = strbuf_strtoul(val,10);
        if(errno != 0) {
            LOGS("error parsing fragment-duration value %.*s",(*val));
            return -1;
        }
        if(userdata->fragment_duration == 0) {
            LOGS("invalid fragment-duration %.*s",(*val));
            return -1;
        }
        return 0;
    }

    LOGS("unknown key \"%.*s\"",(*key));

    cleanup:
    return -1;
}

static int plugin_submit_segment(void* ud, const segment* seg) {
    int r;
    file_userdata* userdata = (file_userdata*)ud;

    TRY0(fwrite(seg->data,1,seg->len,userdata->f) == seg->len ? 0 : -1,
      LOG1("error writing segment: %s", strerror(errno))
    );

    cleanup:
    return r;
}

static int plugin_submit_picture(void* ud, const picture* src, picture* out) {
    int r;
    file_userdata* userdata = (file_userdata*)ud;
    strbuf dest_filename = STRBUF_ZERO;
    int picture_id;
    const char *fmt_str;
    FILE* f = NULL;

    picture_id = thread_atomic_uint_inc(&counter) % 100000000;

    if(userdata->basename.len > 0) {
        TRYS(strbuf_cat(&dest_filename,&userdata->basename));
    }

    if(strbuf_ends_cstr(&src->mime,"/png")) {
        fmt_str = "%08u.png";
    } else if(strbuf_ends_cstr(&src->mime,"/jpg") || strbuf_ends_cstr(&src->mime,"jpeg")) {
        fmt_str = "%08u.jpg";
    } else if(strbuf_ends_cstr(&src->mime,"/gif")) {
        fmt_str = "%08u.gif";
    } else if(strbuf_equals_cstr(&src->mime,"image/")) {
        /* just assume it's JPG */
        fmt_str = "%08u.jpg";
    } else {
        LOGS("WARNING: unknown image mime type %.*s",src->mime);
        r = 0; goto cleanup;
    }

    TRYS(strbuf_sprintf(&dest_filename,fmt_str,picture_id));
    TRYS(strbuf_term(&dest_filename));

    TRYNULL(f = file_open(&dest_filename), fflush(stderr));

    TRY0(fwrite(src->data.x,1,src->data.len, f) == src->data.len ? 0 : -1,
      LOG1("error writing picture: %s", strerror(errno))
    );

    TRYS(membuf_append(&out->mime,"-->",3));
    TRYS(membuf_append(&out->data,&dest_filename.x[userdata->basename.len],12));
    TRYS(strbuf_copy(&out->desc,&src->desc));
    r = 0;

    cleanup:
    if(f != NULL) fclose(f);
    strbuf_free(&dest_filename);
    return r;
}

static int plugin_submit_tags(void* ud, const taglist* tags) {
    (void)ud;
    (void)tags;
    return 0;
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

static int plugin_reset(void* userdata) {
    (void)userdata;
    return 0;
}

const output_plugin output_plugin_file = {
    plugin_name,
    plugin_size,
    plugin_init,
    plugin_deinit,
    plugin_create,
    plugin_config,
    plugin_open,
    plugin_close,
    plugin_set_time,
    plugin_submit_segment,
    plugin_submit_picture,
    plugin_submit_tags,
    plugin_flush,
    plugin_reset,
    plugin_get_segment_info,
};
