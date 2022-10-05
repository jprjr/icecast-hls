#include "output_plugin_folder.h"

#include "strbuf.h"
#include "membuf.h"
#include "thread.h"
#include "hls.h"

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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

/* keep a global counter for writing out picture files. We could
 * have multiple file destinations going to the same folder,
 * this ensures multiple threads don't stop on each other */

static thread_atomic_uint_t counter;

struct plugin_userdata {
    hls hls;
    strbuf foldername;
    strbuf initname;
    uint8_t init;
};

typedef struct plugin_userdata plugin_userdata;

static int directory_create(const strbuf* foldername) {
#ifdef DR_WINDOWS
    DWORD last_error;
    int r = -1;
    strbuf w = STRBUF_ZERO;
    if(strbuf_wide(&w,foldername) != 0) goto cleanup;
    r = CreateDirectoryW((wchar_t*)w.x,NULL) ? 0 : -1;
    if(r == -1) {
        last_error = GetLastError();
        if(last_error == ERROR_ALREADY_EXISTS) r = 0;
    }
    cleanup:
    strbuf_free(&w);
    return r;
#else
    int r;
    errno = 0;
    r = mkdir((const char *)foldername->x,S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IXOTH);
    if(r == -1) {
        if(errno == EEXIST) {
            r = 0;
            errno = 0;
        }
    }
    return r;
#endif
}

static int file_delete(const strbuf* filename) {
#ifdef DR_WINDOWS
    int r = -1;
    strbuf w = STRBUF_ZERO;
    if(strbuf_wide(&w,filename) != 0) goto cleanup;
    r = DeleteFileW((wchar_t*)w.x) ? 0 : -1;
    cleanup:
    strbuf_free(&w);
    return r;
#else
    return unlink((const char*)filename->x);
#endif
}

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
    if(strbuf_wide(&w,filename) != 0) goto cleanup;
    f = _wfopen((wchar_t *)w.x, L"wb");
#else
    f = fopen((const char *)filename->x,"wb");
#endif
    if(f == NULL) {
        fprintf(stderr,"[output:folder] error opening file: %.*s\n",(int)filename->len,filename->x);
    }

#ifdef DR_WINDOWS
    cleanup:
    strbuf_free(&w);
#endif

    return f;
}

static void plugin_close(void* userdata) {
    plugin_userdata* ud = (plugin_userdata*)userdata;
    strbuf_free(&ud->foldername);
    strbuf_free(&ud->initname);
    hls_free(&ud->hls);
    free(ud);
}

static int plugin_init(void) {
    thread_atomic_uint_store(&counter,0);
    return 0;
}

static void plugin_deinit(void) {
    return;
}

static int plugin_open(void* ud, const segment_source* source) {
    int r;
    plugin_userdata* userdata = (plugin_userdata*)ud;
    strbuf tmp = STRBUF_ZERO;

    if(userdata->foldername.len == 0) return -1;

    if( (r = strbuf_append(&tmp,(char *)userdata->foldername.x,userdata->foldername.len-1)) != 0) return -1;
    if( (r = strbuf_term(&tmp)) != 0) return -1;
    if(directory_create(&tmp) != 0) return r;
    strbuf_free(&tmp);

    return hls_open(&userdata->hls, source);
}

static int plugin_config(void* ud, const strbuf* key, const strbuf* value) {
    int r;
    plugin_userdata* userdata = (plugin_userdata*)ud;

    if(strbuf_equals_cstr(key,"folder")) {
        if( (r = strbuf_copy(&userdata->foldername,value)) != 0) return r;
        while(userdata->foldername.len && (userdata->foldername.x[userdata->foldername.len - 1] == '/'
#if defined(_WIN32) || defined(_WIN64) || defined(_MSC_VER)
            || userdata->foldername.x[userdata->foldername.len-1] == '\\'
#endif
        )) {

            userdata->foldername.len--;
        }
        if(userdata->foldername.len == 0) return -1;
#if defined(_WIN32) || defined(_WIN64) || defined(_MSC_VER)
        if(strbuf_append(&userdata->foldername,"\\",1) != 0) return r;
#else
        if(strbuf_append(&userdata->foldername,"/",1) != 0) return r;
#endif
        return 0;
    }

    if(strbuf_begins_cstr(key,"hls-")) return hls_configure(&userdata->hls,key,value);

    fprintf(stderr,"[output:folder] unknown key \"%.*s\"\n",(int)key->len,(const char *)key->x);
    return -1;
}

static int plugin_hls_write(void* ud, const strbuf* filename, const membuf* data, const strbuf* mime) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    int r;
    strbuf path = STRBUF_ZERO;
    FILE* f = NULL;

    (void)mime;

#define TRY(x) if(!(x)) goto cleanup;
#define TRYS(x) TRY( (r = (x)) == 0 )
    TRYS(strbuf_copy(&path,&userdata->foldername));
    TRYS(strbuf_cat(&path,filename));
    TRYS(strbuf_term(&path))

    TRY( (f = file_open(&path)) != NULL);
    TRY( fwrite(data->x,1,data->len,f) == data->len );
#undef TRY

    r = 0;
    cleanup:
    if(f != NULL) fclose(f);
    strbuf_free(&path);
    return r;
}

static int plugin_hls_delete(void* ud, const strbuf* filename) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    int r;
    strbuf path = STRBUF_ZERO;

#define TRY(x) if(!(x)) goto cleanup;
#define TRYS(x) TRY( (r = (x)) == 0 )
    TRYS(strbuf_copy(&path,&userdata->foldername));
    TRYS(strbuf_cat(&path,filename));
    TRYS(strbuf_term(&path))

    file_delete(&path);
#undef TRY

    r = 0;
    cleanup:
    strbuf_free(&path);
    return r;
}

static int plugin_submit_segment(void* ud, const segment* seg) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    return hls_add_segment(&userdata->hls,seg);
}

static int plugin_flush(void* ud) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    return hls_flush(&userdata->hls);
}

static int plugin_submit_picture(void* ud, const picture* src, picture* out) {
    int r;
    plugin_userdata* userdata = (plugin_userdata*)ud;
    strbuf dest_filename = STRBUF_ZERO;
    int picture_id;
    const char *fmt_str;
    FILE* f = NULL;

    picture_id = thread_atomic_uint_inc(&counter) % 100000000;

#define TRY(x) if( (r = (x)) != 0 ) goto cleanup

    TRY(strbuf_cat(&dest_filename,&userdata->foldername));

    TRY(strbuf_readyplus(&dest_filename,13));

    if(strbuf_ends_cstr(&src->mime,"/png")) {
        fmt_str = "%08u.png";
    } else if(strbuf_ends_cstr(&src->mime,"/jpg") | strbuf_ends_cstr(&src->mime,"jpeg")) {
        fmt_str = "%08u.jpg";
    } else if(strbuf_ends_cstr(&src->mime,"/gif")) {
        fmt_str = "%08u.gif";
    } else {
        fprintf(stderr,"[output:folder] unknown image mime type %.*s\n",
          (int)src->mime.len,(char*)src->mime.x);
        r = 0; goto cleanup;
    }

    snprintf((char *)&dest_filename.x[userdata->foldername.len],13,fmt_str,picture_id);
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
    TRY(strbuf_copy(&out->desc,&src->desc));
    TRY(membuf_append(&out->data,&dest_filename.x[userdata->foldername.len],12));
    r = 0;
#undef TRY

    cleanup:
    if(f != NULL) fclose(f);
    strbuf_free(&dest_filename);
    return r;
}


static void* plugin_create(void) {
    plugin_userdata* userdata = (plugin_userdata*)malloc(sizeof(plugin_userdata));
    if(userdata == NULL) return userdata;

    strbuf_init(&userdata->foldername);
    strbuf_init(&userdata->initname);
    hls_init(&userdata->hls);

    userdata->hls.callbacks.write  = plugin_hls_write;
    userdata->hls.callbacks.delete = plugin_hls_delete;
    userdata->hls.callbacks.userdata = userdata;

    return userdata;
}

static int plugin_set_time(void* ud, const ich_time* now) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    memcpy(&userdata->hls.now,now,sizeof(ich_time));
    return 0;
}

const output_plugin output_plugin_folder = {
    { .a = 0, .len = 6, .x = (uint8_t*)"folder" },
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
