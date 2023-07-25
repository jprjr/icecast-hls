#include "input_plugin_curl.h"
#include "ich_time.h"

#include <curl/curl.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#define LOG0(s) fprintf(stderr,"[input:curl] "s"\n")
#define LOG1(s,a) fprintf(stderr,"[input:curl] "s"\n",(a))
#define LOG2(s,a,b) fprintf(stderr,"[input:curl] "s"\n",(a),(b))
#define LOGS(s,a) LOG2(s,(int)(a).len,(char *)(a).x)

#define LOGERRNO(s) LOG1(s": %s", strerror(errno))
#define LOGCURLE(s,e) LOG1(s": %s", curl_easy_strerror(e))
#define LOGINT(s, i) LOG1(s": %d", i)

static STRBUF_CONST(ICY_TITLE,"icy_title");
static STRBUF_CONST(ICY_NAME,"icy_name");
static STRBUF_CONST(ICY_GENRE,"icy_genre");
static STRBUF_CONST(ICY_DESCRIPTION,"icy_description");
static STRBUF_CONST(ICY_URL,"icy_url");

struct input_plugin_curl_userdata {
    strbuf url;
    strbuf buffer;
    strbuf tmp;
    taglist tags;
    CURL* handle;
    CURLM* mhandle;
    struct curl_slist* headers;
    unsigned int connect_timeout;
    unsigned int read_timeout;
    unsigned int ignore_icecast;
    int tags_sent;
    int icy_header;
    unsigned int metaint;
    unsigned int metaread;
    unsigned int in_headers;
    long verbose;
    size_t (*read)(struct input_plugin_curl_userdata*, void*, size_t, const tag_handler*, unsigned int timeout);
};

typedef struct input_plugin_curl_userdata input_plugin_curl_userdata;

static size_t input_plugin_curl_size(void) {
    return sizeof(input_plugin_curl_userdata);
}

static inline void trim(strbuf* s) {
    while(s->len > 0 && isspace(s->x[0])) {
        s->x++;
        s->len--;
    }
    while(s->len > 0 && (s->x[s->len-1] == '\0' || isspace(s->x[s->len-1]) )) {
        s->len--;
    }
}

static size_t input_plugin_curl_buffer(input_plugin_curl_userdata* userdata, size_t len, unsigned int timeout) {
    CURLMcode mc;
    int numfds;
    int still_running;
    ich_time now;
    ich_time deadline;
    size_t s;

    ich_time_now(&now);
    deadline = now;
    deadline.seconds += timeout / 1000;
    deadline.nanoseconds += (timeout % 1000) * 1000;
    s = userdata->buffer.len;

    while(userdata->buffer.len < len) {
        mc = curl_multi_perform(userdata->mhandle, &still_running);
        if(mc != 0) {
            LOGINT("error calling curl_multi_perform",  mc);
            return 0;
        }
        if(!still_running) break;

        do {
            mc = curl_multi_poll(userdata->mhandle, NULL, 0, timeout, &numfds);
            if(mc != 0) {
                LOGINT("error calling curl_multi_poll",mc);
                return 0;
            }
            ich_time_now(&now);
            if(ich_time_cmp(&now,&deadline) > 0) {
                LOG1("buffer timeout, bytes read: %lu", userdata->buffer.len - s);
                return userdata->buffer.len;
            }
        } while(numfds == 0);
    }

    return userdata->buffer.len;
}

static size_t input_plugin_curl_read_dummy(input_plugin_curl_userdata* userdata, void* dest, size_t len, const tag_handler* handler, unsigned int timeout) {
    CURLMcode mc;
    int numfds;
    int still_running;
    ich_time now;
    ich_time deadline;

    ich_time_now(&now);
    deadline = now;
    deadline.seconds += (userdata->connect_timeout / 1000);
    deadline.nanoseconds += (userdata->connect_timeout % 1000) * 1000;

    (void)timeout;

    while(userdata->in_headers) {
        mc = curl_multi_perform(userdata->mhandle, &still_running);
        if(mc != 0) {
            LOGINT("error calling curl_multi_perform",  mc);
            return 0;
        }
        if(!still_running) return 0;

        do {
            mc = curl_multi_poll(userdata->mhandle, NULL, 0, userdata->connect_timeout, &numfds);

            if(mc != 0) {
                LOGINT("error calling curl_multi_poll",mc);
                return 0;
            }
            ich_time_now(&now);
            if(ich_time_cmp(&now,&deadline) > 0) {
                LOG0("connection timeout, returning 0 bytes");
                return 0;
            }
        } while(numfds == 0);
    }

    return userdata->read(userdata, dest, len, handler, userdata->connect_timeout);
}


static size_t input_plugin_curl_read_nometaint(input_plugin_curl_userdata* userdata, void* dest, size_t len, const tag_handler* handler, unsigned int timeout) {
    size_t r;

    (void)handler;

    if( (r = input_plugin_curl_buffer(userdata,len,timeout)) == 0) return r;
    if(r > len) r = len;

    memcpy(dest,userdata->buffer.x,r);
    membuf_trim(&userdata->buffer,r);

    return r;
}

static int parse_icy_data(input_plugin_curl_userdata* userdata, size_t len, const tag_handler* handler) {
    strbuf t = STRBUF_ZERO;
    strbuf e = STRBUF_ZERO;
    strbuf q = STRBUF_ZERO;
    const strbuf* key = NULL;
    int r = 0;
    int f = 0;

    t.x = &userdata->buffer.x[1];
    t.len = len - 1;
    trim(&t);
    if(t.len == 0) return 0;

    while(t.len) {
        if(strbuf_casebegins_cstr(&t,"streamtitle='")) {
            key = &ICY_TITLE;
            t.x = &t.x[13];
            t.len -= 13;
        } else if(strbuf_casebegins_cstr(&t,"streamurl='")) {
            key = &ICY_URL;
            t.x = &t.x[11];
            t.len -= 11;
        } else {
            break;
        }
        if(t.len == 0) break;
        e = t;

        while(( r = strbuf_chrbuf(&e, &e, '\'')) == 0) {
            if(e.len < 2) break;
            /* found an apostrophe, look for the semicolon */
            if(e.x[1] != ';') {
                e.x++;
                e.len--;
                continue;
            }
            if(e.len == 2) break; /* end of data */

            q = e;
            q.x = &q.x[2];
            q.len -= 2;
            if(strbuf_casebegins_cstr(&q,"streamurl='") || strbuf_casebegins_cstr(&q,"streamtitle='")) {
                    /* we found the apostrphe, semicolon, and the beginning of the next tag */
                    break;
            }

            /* need to keep looking */
            e.x++;
            e.len--;
            continue;
        }

        if(r != 0) { /* never found an apostrophe */
            return 0;
        }

        q.x = t.x;
        q.len = e.x - t.x;

        if(q.len > 0) {
            taglist_clear(&userdata->tags, key);
            taglist_add(&userdata->tags,key,&q);
            f = 1;
        }

        /* advance t */
        t.x = &e.x[2];
        t.len = e.len - 2;
    }

    if(f) { /* some new tag was found */
        r = handler->cb(handler->userdata, &userdata->tags);
        userdata->tags_sent = 1;
    }

    return r;
}

static size_t input_plugin_curl_read_metaint(input_plugin_curl_userdata* userdata, void* dest, size_t len, const tag_handler* handler, unsigned int timeout) {
    size_t n;
    size_t s;
    size_t r;

    for(;;) {
        n = userdata->metaint - userdata->metaread;

        if(n == 0) {
            if(input_plugin_curl_buffer(userdata,1,timeout) < 1) return 0;
            s = userdata->buffer.x[0];
            if(s > 0) {
                s *= 16;
                if(input_plugin_curl_buffer(userdata,s+1,timeout) < s+1) return 0;
                if(!userdata->ignore_icecast) {
                    if(parse_icy_data(userdata, s, handler) != 0) return 0;
                }
            }
            membuf_trim(&userdata->buffer,s+1);
            userdata->metaread = 0;
            continue;
        }

        if(n > len) n = len;

        if( (r = input_plugin_curl_buffer(userdata,n,timeout)) == 0) return r;
        if(r > n) r = n; /* cap to max before we see metainfo */
        userdata->metaread += r;
        break;
    }

    if(r > len) r = len;

    memcpy(dest,userdata->buffer.x,r);
    membuf_trim(&userdata->buffer,r);

    return r;
}

static int input_plugin_curl_init(void) {
    return curl_global_init(CURL_GLOBAL_ALL);
}

static void input_plugin_curl_deinit(void) {
    curl_global_cleanup();
}

static int input_plugin_curl_create(void* ud) {
    input_plugin_curl_userdata* userdata = (input_plugin_curl_userdata*)ud;

    strbuf_init(&userdata->url);
    strbuf_init(&userdata->buffer);
    strbuf_init(&userdata->tmp);
    taglist_init(&userdata->tags);
    userdata->handle   = NULL;
    userdata->mhandle  = NULL;
    userdata->connect_timeout  = 2000;
    userdata->read_timeout  = 1000;
    userdata->headers  = NULL;
    userdata->metaint  = 0;
    userdata->metaread = 0;
    userdata->tags_sent = 0;
    userdata->verbose = 0;
    userdata->icy_header = 0;
    userdata->ignore_icecast = 0;
    userdata->in_headers = 1;
    userdata->read = input_plugin_curl_read_dummy;

    return 0;
}

static void input_plugin_curl_close(void* ud) {
    input_plugin_curl_userdata* userdata = (input_plugin_curl_userdata*)ud;

    if(userdata->mhandle != NULL) {
        curl_multi_remove_handle(userdata->mhandle, userdata->handle);
    }
    if(userdata->handle != NULL) {
        curl_easy_cleanup(userdata->handle);
    }
    if(userdata->mhandle != NULL) {
        curl_multi_cleanup(userdata->mhandle);
    }
    if(userdata->headers != NULL) {
        curl_slist_free_all(userdata->headers);
    }

    strbuf_free(&userdata->url);
    strbuf_free(&userdata->buffer);
    strbuf_free(&userdata->tmp);
    taglist_free(&userdata->tags);

    userdata->mhandle = NULL;
    userdata->handle = NULL;
    userdata->headers = NULL;
}

static int input_plugin_curl_config(void* ud, const strbuf* key, const strbuf* val) {
    int r;
    input_plugin_curl_userdata* userdata = (input_plugin_curl_userdata*)ud;
    struct curl_slist* slist_temp = NULL;

    if(strbuf_equals_cstr(key,"url")) {
        if( (r = strbuf_copy(&userdata->url,val)) != 0) {
            LOGERRNO("unable to copy url string");
            return r;
        }
        if( (r = strbuf_term(&userdata->url)) != 0) {
            LOGERRNO("unable to terminate url string");
            return r;
        }
        return 0;
    }

    if(strbuf_equals_cstr(key,"read timeout") ||
       strbuf_equals_cstr(key,"read-timeout") ||
       strbuf_equals_cstr(key,"read_timeout")) {
        errno = 0;
        userdata->read_timeout = strbuf_strtoul(val,10);
        if(errno != 0) {
            LOGS("error parsing read timeout value %.*s",(*val));
            return -1;
        }
        if(userdata->read_timeout == 0) {
            LOGS("invalid read timeout %.*s",(*val));
            return -1;
        }
        return 0;
    }

    if(strbuf_equals_cstr(key,"connect timeout") ||
       strbuf_equals_cstr(key,"connect-timeout") ||
       strbuf_equals_cstr(key,"connect_timeout")) {
        errno = 0;
        userdata->connect_timeout = strbuf_strtoul(val,10);
        if(errno != 0) {
            LOGS("error parsing connect timeout value %.*s",(*val));
            return -1;
        }
        if(userdata->connect_timeout == 0) {
            LOGS("invalid connect timeout %.*s",(*val));
            return -1;
        }
        return 0;
    }

    if(strbuf_equals_cstr(key,"verbose")) {
        if(strbuf_truthy(val)) {
            userdata->verbose = 1;
            return 0;
        }
        if(strbuf_falsey(val)) {
            userdata->verbose = 0;
            return 0;
        }
        LOGS("error parsing verbose value %.*s",(*val));
        return -1;
    }

    if(strbuf_equals_cstr(key,"ignore icy") ||
       strbuf_equals_cstr(key,"ignore-icy") ||
       strbuf_equals_cstr(key,"ignore_icy") ||
       strbuf_equals_cstr(key,"ignore icecast") ||
       strbuf_equals_cstr(key,"ignore-icecast") ||
       strbuf_equals_cstr(key,"ignore_icecast") ){
        if(strbuf_truthy(val)) {
            userdata->ignore_icecast = 1;
            return 0;
        }
        if(strbuf_falsey(val)) {
            userdata->ignore_icecast = 0;
            return 0;
        }
        LOGS("error parsing ignore-icecast value %.*s",(*val));
        return -1;
    }

    if(strbuf_equals_cstr(key,"header")) {
        if((r = strbuf_copy(&userdata->tmp,val)) != 0) {
            LOGERRNO("unable to copy header string");
            userdata->tmp.len = 0;
            return r;
        }
        if((r = strbuf_term(&userdata->tmp)) != 0) {
            LOGERRNO("unable to terminate header string");
            userdata->tmp.len = 0;
            return r;
        }
        slist_temp = curl_slist_append(userdata->headers,(const char *)userdata->tmp.x);
        userdata->tmp.len = 0;
        if(slist_temp == NULL) {
            LOG0("error appending header");
            return -1;
        }
        userdata->headers = slist_temp;
        if(strbuf_casebegins_cstr(val,"icy-metadata:")) {
            userdata->icy_header = 1;
        }
        return 0;
    }

    LOGS("unknown key: %.*s\n",*key);
    return -1;
}

static size_t input_plugin_curl_write_callback(char* ptr, size_t size, size_t nmemb, void* ud) {
    input_plugin_curl_userdata* userdata = (input_plugin_curl_userdata*)ud;

    if(size*nmemb == 0) return 0;

    if(strbuf_append(&userdata->buffer,ptr,size*nmemb) != 0) {
        LOGERRNO("error appending data to buffer");
        return 0;
    }

    return size*nmemb;
}


static size_t input_plugin_curl_header_callback(char* ptr, size_t size, size_t nmemb, void* ud) {
    input_plugin_curl_userdata* userdata = (input_plugin_curl_userdata*)ud;
    strbuf t = STRBUF_ZERO;

    if(size * nmemb == 0) return 0;
    if(size * nmemb == 2) { /* final header line */
        if(userdata->read != input_plugin_curl_read_metaint) userdata->read = input_plugin_curl_read_nometaint;
        userdata->in_headers = 0;
        return size * nmemb;
    }

    userdata->tmp.len = 0;
    if(strbuf_append(&userdata->tmp,ptr, size * nmemb) != 0) {
        LOGERRNO("error appending headers");
        return 0;
    }

    if(strbuf_casebegins_cstr(&userdata->tmp, "icy-metaint:")) {
        t.x = &userdata->tmp.x[12];
        t.len = userdata->tmp.len - 12;
        trim(&t);
        if(t.len == 0) {
            LOG0("metaint - no value");
            return 0;
        }
        errno = 0;
        userdata->metaint = strbuf_strtoul(&t,10);
        if(errno != 0) {
            LOGS("error parsing metaint value %.*s",t);
            return 0;
        }
        if(userdata->metaint == 0) {
            LOGS("invalid metaint %.*s",t);
            return 0;
        }
        userdata->read = input_plugin_curl_read_metaint;
    }
    else if(strbuf_casebegins_cstr(&userdata->tmp, "icy-name:")) {
        t.x = &userdata->tmp.x[9];
        t.len = userdata->tmp.len - 9;
        trim(&t);
        if(t.len == 0) {
            return size*nmemb;
        }
        if(taglist_add(&userdata->tags, &ICY_NAME, &t) != 0) {
            LOG0("error adding tag");
            return 0;
        }
    }
    else if(strbuf_casebegins_cstr(&userdata->tmp, "icy-genre:")) {
        t.x = &userdata->tmp.x[10];
        t.len = userdata->tmp.len - 10;
        trim(&t);
        if(t.len == 0) {
            return size*nmemb;
        }
        if(taglist_add(&userdata->tags, &ICY_GENRE, &t) != 0) {
            LOG0("error adding tag");
            return 0;
        }
    }
    else if(strbuf_casebegins_cstr(&userdata->tmp, "icy-description:")) {
        t.x = &userdata->tmp.x[16];
        t.len = userdata->tmp.len - 16;
        trim(&t);
        if(t.len == 0) {
            return size*nmemb;
        }
        if(taglist_add(&userdata->tags, &ICY_DESCRIPTION, &t) != 0) {
            LOG0("error adding tag");
            return 0;
        }
    }
    else if(strbuf_casebegins_cstr(&userdata->tmp, "icy-url:")) {
        t.x = &userdata->tmp.x[8];
        t.len = userdata->tmp.len - 8;
        trim(&t);
        if(t.len == 0) {
            return size*nmemb;
        }
        if(taglist_add(&userdata->tags, &ICY_URL, &t) != 0) {
            LOG0("error adding tag");
            return 0;
        }
    }

    return size*nmemb;
}

static int input_plugin_curl_open(void* ud) {
    CURLcode r;
    CURLMcode mc;
    struct curl_slist* slist_temp = NULL;
    input_plugin_curl_userdata* userdata = (input_plugin_curl_userdata*)ud;

    if(userdata->url.len == 0) {
        LOG0("url not set");
        return -1;
    }

    userdata->handle = curl_easy_init();
    if(userdata->handle == NULL) {
        LOG0("error creating curl handle");
        return -1;
    }

    userdata->mhandle = curl_multi_init();
    if(userdata->mhandle == NULL) {
        LOG0("error creating curl multi handle");
        return -1;
    }

    if(( r = curl_easy_setopt(userdata->handle, CURLOPT_URL, userdata->url.x)) != 0) {
        LOGCURLE("error setting URL", r);
        return -1;
    }

    if(( r = curl_easy_setopt(userdata->handle, CURLOPT_WRITEFUNCTION, input_plugin_curl_write_callback)) != 0) {
        LOGCURLE("error setting write callback", r);
        return -1;
    }

    if(( r = curl_easy_setopt(userdata->handle, CURLOPT_WRITEDATA, userdata)) != 0) {
        LOGCURLE("error setting write data", r);
        return -1;
    }

    if(( r = curl_easy_setopt(userdata->handle, CURLOPT_HEADERFUNCTION, input_plugin_curl_header_callback)) != 0) {
        LOGCURLE("error setting write callback", r);
        return -1;
    }

    if(( r = curl_easy_setopt(userdata->handle, CURLOPT_HEADERDATA, userdata)) != 0) {
        LOGCURLE("error setting write data", r);
        return -1;
    }

    if(userdata->ignore_icecast == 0 && !userdata->icy_header) { /* user didn't specify ignore_icy and didn't add the icy-metadata header  - we'll add */
        slist_temp = curl_slist_append(userdata->headers, "Icy-MetaData:1");
        if(slist_temp == NULL) {
            LOG0("error adding Icy-MetaData header");
            return -1;
        }
        userdata->headers = slist_temp;
    }

    if(userdata->headers != NULL) {
        if( (r = curl_easy_setopt(userdata->handle, CURLOPT_HTTPHEADER, userdata->headers)) != 0) {
            LOGCURLE("error setting headers", r);
            return -1;
        }
    }

    curl_easy_setopt(userdata->handle, CURLOPT_VERBOSE, userdata->verbose);
    curl_easy_setopt(userdata->handle, CURLOPT_FOLLOWLOCATION, 1L);

    if((mc = curl_multi_add_handle(userdata->mhandle, userdata->handle)) != 0) {
        LOG0("error adding easy handle to multi handle");
        return -1;
    }

    userdata->url.len = 0;
    return 0;
}

static size_t input_plugin_curl_read(void* ud, void* dest, size_t len, const tag_handler* handler) {
    input_plugin_curl_userdata* userdata = (input_plugin_curl_userdata*)ud;

    if( userdata->tags_sent == 0 &&
        !userdata->ignore_icecast &&
        taglist_len(&userdata->tags) > 0) {
        if(handler->cb(handler->userdata, &userdata->tags) != 0) return 0;
        userdata->tags_sent = 1;
    }

    return userdata->read(userdata, dest, len, handler, userdata->read_timeout);
}


const input_plugin input_plugin_curl = {
    { .a = 0, .len = 4, .x = (uint8_t*)"curl" },
    input_plugin_curl_size,
    input_plugin_curl_init,
    input_plugin_curl_deinit,
    input_plugin_curl_create,
    input_plugin_curl_config, /* config */
    input_plugin_curl_open,
    input_plugin_curl_close,
    input_plugin_curl_read,
};
