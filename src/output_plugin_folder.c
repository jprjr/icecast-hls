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

#define LOG_PREFIX "[output:folder]"
#include "logger.h"

static STRBUF_CONST(plugin_name,"folder");

struct plugin_userdata {
    hls hls;
    strbuf foldername;
    strbuf initname;
    strbuf picture_filename; /* used to track the current out-of-band picture */
    strbuf scratch;
    strbuf wide;
    uint8_t init;
    int pictureflag;
};

typedef struct plugin_userdata plugin_userdata;

static int directory_create(plugin_userdata* userdata, const strbuf* foldername) {
#ifdef DR_WINDOWS
    DWORD last_error;
    int r = -1;
    userdata->wide.len = 0;
    if(strbuf_wide(&userdata->wide,foldername) != 0) goto cleanup;
    r = CreateDirectoryW((wchar_t*)userdata->wide.x,NULL) ? 0 : -1;
    if(r == -1) {
        last_error = GetLastError();
        if(last_error == ERROR_ALREADY_EXISTS) r = 0;
    }
    cleanup:
    return r;
#else
    int r;
    (void) userdata;
    errno = 0;
    r = mkdir((const char *)foldername->x,S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IXOTH);
    if(r == -1) {
        if(errno == EEXIST) {
            r = 0;
            errno = 0;
        }
        else {
            log_error("unable to create folder %s: %s",
              (const char *)foldername->x,
              strerror(errno));
        }
    }
    return r;
#endif
}

static int file_delete(plugin_userdata* userdata, const strbuf* filename) {
#ifdef DR_WINDOWS
    int r = -1;
    userdata->wide.len = 0;
    if(strbuf_wide(&userdata->wide,filename) != 0) goto cleanup;
    r = DeleteFileW((wchar_t*)userdata->wide.x) ? 0 : -1;
    cleanup:
    return r;
#else
    (void)userdata;
    return unlink((const char*)filename->x);
#endif
}

static int file_rename(plugin_userdata* userdata, const strbuf* oldname, const strbuf* newname) {
#ifdef DR_WINDOWS
    int r = -1;
    strbuf oldname_wide = STRBUF_ZERO;
    strbuf newname_wide = STRBUF_ZERO;
    userdata->wide.len = 0;
    if(strbuf_wide(&userdata->wide,oldname) != 0) goto cleanup;
    oldname_wide.len = userdata->wide.len;
    if(strbuf_wide(&userdata->wide,newname) != 0) goto cleanup;
    oldname_wide.x = userdata->wide.x;
    newname_wide.x = userdata->wide.x + oldname_wide.len;
    newname_wide.len = userdata->wide.len - oldname_wide.len;
    r = MoveFileExW((wchar_t*)oldname_wide.x, (wchar_t*)newname_wide.x, MOVEFILE_REPLACE_EXISTING) ? 0 : -1;
    return r
#else
    /* c standard states that renaming to an existing file is implementation-defined,
     * POSIX clarifies that it should succeed and be an atomic operation (assuming no permissions
     * issues etc).
     */
    (void)userdata;
    return rename((const char*)oldname->x, (const char*)newname->x);
#endif
}

static FILE* file_open(plugin_userdata* userdata, const strbuf* filename) {
    FILE* f = NULL;
#ifdef DR_WINDOWS
    userdata->wide.len = 0;

    /* since we'll include the terminating zero we don't
     * need to manually terminate this after calling strbuf_wide */
    if(strbuf_wide(&userdata->wide,filename) != 0) goto cleanup;
    f = _wfopen((wchar_t *)userdata->wide.x, L"wb");
#else
    (void)userdata;
    f = fopen((const char *)filename->x,"wb");
#endif
    if(f == NULL) {
        fprintf(stderr,"[output:folder] error opening file: %.*s\n",(int)filename->len,filename->x);
    }

#ifdef DR_WINDOWS
    cleanup:
#endif

    return f;
}

static void plugin_close(void* userdata) {
    plugin_userdata* ud = (plugin_userdata*)userdata;
    strbuf_free(&ud->foldername);
    strbuf_free(&ud->initname);
    strbuf_free(&ud->picture_filename);
    strbuf_free(&ud->scratch);
    strbuf_free(&ud->wide);
    hls_free(&ud->hls);
}

static int plugin_init(void) {
    return 0;
}

static void plugin_deinit(void) {
    return;
}

static int plugin_get_segment_info(const void* ud, const segment_source_info* info, segment_params* params) {
    const plugin_userdata* userdata = (const plugin_userdata*)ud;
    return hls_get_segment_info(&userdata->hls, info, params);
}

static int plugin_open(void* ud, const segment_source* source) {
    int r;
    plugin_userdata* userdata = (plugin_userdata*)ud;
    userdata->scratch.len = 0;

    if(userdata->foldername.len == 0) return -1;

    if( (r = strbuf_append(&userdata->scratch,(char *)userdata->foldername.x,userdata->foldername.len-1)) != 0) return -1;
    if( (r = strbuf_term(&userdata->scratch)) != 0) return -1;
    if( (r = directory_create(userdata, &userdata->scratch)) != 0) return r;

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
    FILE* f = NULL;
    strbuf final  = STRBUF_ZERO;
    strbuf tmp = STRBUF_ZERO;
    userdata->scratch.len = 0;

    (void)mime;

    if(data->len == 0) abort();


#define TRY(x) if(!(x)) goto cleanup;
#define TRYS(x) TRY( (r = (x)) == 0 )
    if(userdata->pictureflag) {
        if(userdata->picture_filename.len > 0) {
            TRYS(hls_expire_file(&userdata->hls,&userdata->picture_filename));
        }
        TRYS(strbuf_copy(&userdata->picture_filename,filename));
    }

    /* when we append the string to itself we don't want any reallocs so
     * get memory ahead of time */
    TRYS(strbuf_ready(&userdata->scratch,
        (userdata->foldername.len * 2) +
        (filename->len * 2) +
        (4) + /* for ".tmp" */
        (2) /* for 2 null terminators */ ));

    TRYS(strbuf_copy(&userdata->scratch,&userdata->foldername));
    TRYS(strbuf_cat(&userdata->scratch,filename));
    TRYS(strbuf_term(&userdata->scratch));
    final = userdata->scratch;

    TRYS(strbuf_cat(&userdata->scratch,&final));
    userdata->scratch.len--;
    TRYS(strbuf_append(&userdata->scratch,".tmp",4));
    TRYS(strbuf_term(&userdata->scratch));

    final.x = userdata->scratch.x;
    tmp.x  = final.x + final.len;
    tmp.len = userdata->scratch.len - final.len;

    r = -1;
    TRY( (f = file_open(userdata, &tmp)) != NULL);
    TRY( fwrite(data->x,1,data->len,f) == data->len );
    fclose(f); f = NULL;
    TRYS(file_rename(userdata, &tmp, &final));
#undef TRY

    r = 0;
    cleanup:
    if(f != NULL) fclose(f);
    return r;
}

static int plugin_hls_delete(void* ud, const strbuf* filename) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    int r;

#define TRY(x) if(!(x)) goto cleanup;
#define TRYS(x) TRY( (r = (x)) == 0 )
    TRYS(strbuf_copy(&userdata->scratch,&userdata->foldername));
    TRYS(strbuf_cat(&userdata->scratch,filename));
    TRYS(strbuf_term(&userdata->scratch))

    file_delete(userdata, &userdata->scratch);
#undef TRY

    r = 0;
    cleanup:
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

static int plugin_reset(void* ud) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    return hls_reset(&userdata->hls);
}

static int plugin_submit_picture(void* ud, const picture* src, picture* out) {
    int r;
    plugin_userdata* userdata = (plugin_userdata*)ud;

    userdata->pictureflag = 1;
    r = hls_submit_picture(&userdata->hls,src,out);
    userdata->pictureflag = 0;

    return r;
}

static size_t plugin_size(void) {
    return sizeof(plugin_userdata);
}

static int plugin_create(void* ud) {
    plugin_userdata* userdata = (plugin_userdata*)ud;

    strbuf_init(&userdata->foldername);
    strbuf_init(&userdata->initname);
    strbuf_init(&userdata->picture_filename);
    strbuf_init(&userdata->scratch);
    hls_init(&userdata->hls);
    userdata->pictureflag = 0;

    userdata->hls.callbacks.write  = plugin_hls_write;
    userdata->hls.callbacks.delete = plugin_hls_delete;
    userdata->hls.callbacks.userdata = userdata;

    return 0;
}

static int plugin_set_time(void* ud, const ich_time* now) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    memcpy(&userdata->hls.now,now,sizeof(ich_time));
    return 0;
}

static int plugin_submit_tags(void* ud, const taglist* tags) {
    (void)ud;
    (void)tags;
    return 0;
}

const output_plugin output_plugin_folder = {
    &plugin_name,
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
