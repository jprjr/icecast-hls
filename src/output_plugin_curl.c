#include "output_plugin_curl.h"

#include <curl/curl.h>

#include "strbuf.h"
#include "membuf.h"
#include "hls.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#define LOG0(s) fprintf(stderr,"[output:curl] "s"\n")
#define LOG1(s,a) fprintf(stderr,"[output:curl] "s"\n",(a))
#define LOG2(s,a,b) fprintf(stderr,"[output:curl] "s"\n",(a),(b))
#define LOGS(s,a) LOG2(s,(int)(a).len,(char *)(a).x)

#define LOGERRNO(s) LOG1(s": %s", strerror(errno))
#define LOGCURLE(s,e) LOG1(s": %s", curl_easy_strerror(e))
#define LOGINT(s, i) LOG1(s": %d", i)

static STRBUF_CONST(plugin_name,"curl");

struct output_plugin_curl_userdata {
    hls hls;
    strbuf tmp;
    strbuf url;
    strbuf picture_filename;

    strbuf aws_region;
    strbuf aws_service;
    strbuf aws_access_key_id;
    strbuf aws_secret_access_key;
    strbuf http_username;
    strbuf http_password;

    strbuf aws_param;
    strbuf http_userpwd;

    strbuf put_headers;
    strbuf delete_headers;
    strbuf shared_headers;

    uint8_t pictureflag;
    CURL* handle;
    long verbose;
    uint8_t aws;
    uint8_t delete;
    struct curl_slist* headers;
};

typedef struct output_plugin_curl_userdata output_plugin_curl_userdata;

struct output_plugin_curl_read_s {
    size_t pos;
    const membuf* data;
};
typedef struct output_plugin_curl_read_s output_plugin_curl_read_s;

static size_t output_plugin_curl_readdata(char* buffer, size_t size, size_t nitems, void* ud) {
    output_plugin_curl_read_s* r = (output_plugin_curl_read_s*)ud;

    size *= nitems;
    if(r->pos + size > r->data->len) {
        size = r->data->len - r->pos;
    }
    if(size > 0) {
        memcpy(buffer,&r->data->x[r->pos],size);
    }
    r->pos += size;
    return size;
}

static int append_headers(output_plugin_curl_userdata* userdata, const strbuf* header) {
    strbuf t = STRBUF_ZERO;
    struct curl_slist* slist_temp = NULL;

    t.x = header->x;
    t.len = header->len;

    while(t.len > 0) {
        if( (slist_temp = curl_slist_append(userdata->headers, (const char *)t.x)) == NULL) {
            LOG1("error appending header %s", (const char*)t.x);
            return -1;
        }
        userdata->headers = slist_temp;
        if(strbuf_chrbuf(&t,&t,'\0') != 0) break;

        t.len--;
        t.x++;
    }
    return 0;
}


static int output_plugin_curl_hls_write(void* ud, const strbuf* filename, const membuf* data, const strbuf* mime) {
    output_plugin_curl_userdata* userdata = (output_plugin_curl_userdata*)ud;
    CURLcode c;
    struct curl_slist* slist_temp = NULL;
    output_plugin_curl_read_s r;

    if(data->len == 0) abort();

    curl_easy_reset(userdata->handle);

    r.pos = 0;
    r.data = data;


#define TRY(x) if(!(x)) return -1
#define TRYS(x) TRY( (x) == 0 )

    if(userdata->pictureflag) {
        if(userdata->picture_filename.len > 0) {
            TRYS(hls_expire_file(&userdata->hls, &userdata->picture_filename));
        }
        TRYS(strbuf_copy(&userdata->picture_filename, filename));
    }

    TRYS(strbuf_copy(&userdata->tmp, &userdata->url));
    TRYS(strbuf_cat(&userdata->tmp,filename));
    TRYS(strbuf_term(&userdata->tmp));

    if(userdata->aws_param.len > 0) {
        if( (c = curl_easy_setopt(userdata->handle, CURLOPT_AWS_SIGV4, userdata->aws_param.x)) != 0) {
            LOGCURLE("error setting aws sigv4", c);
            return -1;
        }
    }

    if(userdata->http_userpwd.len > 0) {
        if( (c = curl_easy_setopt(userdata->handle, CURLOPT_USERPWD, userdata->http_userpwd.x)) != 0) {
            LOGCURLE("error setting userpwd", c);
            return -1;
        }
    }

    if( (c = curl_easy_setopt(userdata->handle, CURLOPT_URL, userdata->tmp.x)) != 0) {
        LOGCURLE("error setting url", c);
        return -1;
    }

    if( (c = curl_easy_setopt(userdata->handle, CURLOPT_UPLOAD, 1L)) != 0) {
        LOGCURLE("error setting upload", c);
        return -1;
    }

    if( (c = curl_easy_setopt(userdata->handle, CURLOPT_INFILESIZE_LARGE, (curl_off_t) data->len)) != 0) {
        LOGCURLE("error setting file size", c);
        return -1;
    }

    if( (c = curl_easy_setopt(userdata->handle, CURLOPT_READFUNCTION, output_plugin_curl_readdata)) != 0) {
        LOGCURLE("error setting read callback", c);
        return -1;
    }

    if( (c = curl_easy_setopt(userdata->handle, CURLOPT_READDATA, &r)) != 0) {
        LOGCURLE("error setting read callback", c);
        return -1;
    }

    if( (c = curl_easy_setopt(userdata->handle, CURLOPT_FAILONERROR, 1L)) != 0) {
        LOGCURLE("error setting failonerror", c);
        return -1;
    }

    if( (c = curl_easy_setopt(userdata->handle, CURLOPT_VERBOSE, userdata->verbose)) != 0) {
        LOGCURLE("error setting verbose", c);
        return -1;
    }

    if(userdata->headers != NULL) {
        curl_slist_free_all(userdata->headers);
        userdata->headers = NULL;
    }

    if(append_headers(userdata, &userdata->shared_headers) != 0) {
        LOG0("error appending shared headers");
        return -1;
    }

    if(append_headers(userdata, &userdata->put_headers) != 0) {
        LOG0("error appending put headers");
        return -1;
    }

    userdata->tmp.len = 0;
    TRYS(strbuf_append_cstr(&userdata->tmp,"Content-Type: "));
    TRYS(strbuf_cat(&userdata->tmp,mime));
    TRYS(strbuf_term(&userdata->tmp));

    if( (slist_temp = curl_slist_append(userdata->headers, (const char *)userdata->tmp.x)) == NULL) {
        LOGS("error appending header %.*s", userdata->tmp);
        return -1;
    }
    userdata->headers = slist_temp;

    if( (c = curl_easy_setopt(userdata->handle, CURLOPT_HTTPHEADER, userdata->headers)) != 0) {
        LOGCURLE("error setting headers", c);
        return -1;
    }

    if( (c = curl_easy_perform(userdata->handle)) != 0) {
        LOGCURLE("error performing put", c);
        return -1;
    }

#undef TRY
#undef TRYS
    return 0;
}

static int output_plugin_curl_hls_delete(void* ud, const strbuf* filename) {
    output_plugin_curl_userdata* userdata = (output_plugin_curl_userdata*)ud;
    CURLcode c;

    if(!userdata->delete) return 0;

#define TRY(x) if(!(x)) return -1
#define TRYS(x) TRY( (x) == 0 )
    curl_easy_reset(userdata->handle);

    TRYS(strbuf_copy(&userdata->tmp, &userdata->url));
    TRYS(strbuf_cat(&userdata->tmp,filename));
    TRYS(strbuf_term(&userdata->tmp));

    if(userdata->aws_param.len > 0) {
        if( (c = curl_easy_setopt(userdata->handle, CURLOPT_AWS_SIGV4, userdata->aws_param.x)) != 0) {
            LOGCURLE("error setting aws sigv4", c);
            return -1;
        }
    }

    if(userdata->http_userpwd.len > 0) {
        if( (c = curl_easy_setopt(userdata->handle, CURLOPT_USERPWD, userdata->http_userpwd.x)) != 0) {
            LOGCURLE("error setting userpwd", c);
            return -1;
        }
    }

    if(userdata->headers != NULL) {
        curl_slist_free_all(userdata->headers);
        userdata->headers = NULL;
    }

    if(append_headers(userdata, &userdata->shared_headers) != 0) {
        LOG0("error appending shared headers");
        return -1;
    }

    if(append_headers(userdata, &userdata->delete_headers) != 0) {
        LOG0("error appending delete headers");
        return -1;
    }

    if( (c = curl_easy_setopt(userdata->handle, CURLOPT_URL, userdata->tmp.x)) != 0) {
        LOGCURLE("error setting url", c);
        return -1;
    }

    if( (c = curl_easy_setopt(userdata->handle, CURLOPT_CUSTOMREQUEST, "DELETE")) != 0) {
        LOGCURLE("error setting DELETE method", c);
        return -1;
    }

    if( (c = curl_easy_setopt(userdata->handle, CURLOPT_FAILONERROR, 1L)) != 0) {
        LOGCURLE("error setting failonerror", c);
        return -1;
    }

    if( (c = curl_easy_setopt(userdata->handle, CURLOPT_VERBOSE, userdata->verbose)) != 0) {
        LOGCURLE("error setting verbose", c);
        return -1;
    }

    if( (c = curl_easy_perform(userdata->handle)) != 0) {
        LOGCURLE("error performing delete", c);
        return -1;
    }

#undef TRY
#undef TRYS
    return 0;
}

static int output_plugin_curl_init(void) {
    return curl_global_init(CURL_GLOBAL_ALL);
}

static void output_plugin_curl_deinit(void) {
    curl_global_cleanup();
}

static size_t output_plugin_curl_size(void) {
    return sizeof(output_plugin_curl_userdata);
}

static int output_plugin_curl_create(void* ud) {
    output_plugin_curl_userdata* userdata = (output_plugin_curl_userdata*)ud;

    strbuf_init(&userdata->tmp);
    strbuf_init(&userdata->url);
    strbuf_init(&userdata->picture_filename);

    strbuf_init(&userdata->aws_region);
    strbuf_init(&userdata->aws_service);
    strbuf_init(&userdata->aws_access_key_id);
    strbuf_init(&userdata->aws_secret_access_key);

    strbuf_init(&userdata->http_username);
    strbuf_init(&userdata->http_password);

    strbuf_init(&userdata->aws_param);
    strbuf_init(&userdata->http_userpwd);

    strbuf_init(&userdata->put_headers);
    strbuf_init(&userdata->delete_headers);
    strbuf_init(&userdata->shared_headers);

    hls_init(&userdata->hls);
    userdata->pictureflag = 0;

    userdata->hls.callbacks.write = output_plugin_curl_hls_write;
    userdata->hls.callbacks.delete = output_plugin_curl_hls_delete;
    userdata->hls.callbacks.userdata = userdata;
    userdata->handle = NULL;
    userdata->headers = NULL;
    userdata->verbose = 0;
    userdata->aws = 2; /* unset */
    userdata->delete = 1;

    return 0;
}

static int output_plugin_curl_config(void* ud, const strbuf* key, const strbuf* value) {
    int r;
    output_plugin_curl_userdata* userdata = (output_plugin_curl_userdata*)ud;

    if(strbuf_begins_cstr(key,"hls-")) return hls_configure(&userdata->hls,key,value);

    if(strbuf_equals_cstr(key,"url")) {
        if( (r = strbuf_copy(&userdata->url,value)) != 0) return r;
        while(userdata->url.len && (userdata->url.x[userdata->url.len - 1] == '/')) {
            userdata->url.len--;
        }
        if(userdata->url.len == 0) {
            LOGS("invalid url: %.*s", (userdata->url));
            return -1;
        }
        if(strbuf_append(&userdata->url,"/",1) != 0) {
            LOGERRNO("error appending slash");
            return -1;
        }
        return 0;
    }

    if(strbuf_equals_cstr(key,"username")) {
        if( (r = strbuf_copy(&userdata->http_username,value)) != 0) {
            LOG0("error copying http username string");
            return r;
        }
        return 0;
    }

    if(strbuf_equals_cstr(key,"password")) {
        if( (r = strbuf_copy(&userdata->http_password,value)) != 0) {
            LOG0("error copying http password string");
            return r;
        }
        return 0;
    }

    if(strbuf_equals_cstr(key,"verbose")) {
        if(strbuf_truthy(value)) {
            userdata->verbose = 1;
            return 0;
        }
        if(strbuf_falsey(value)) {
            userdata->verbose = 0;
            return 0;
        }
        LOGS("error parsing verbose value %.*s",(*value));
        return -1;
    }

    if(strbuf_equals_cstr(key,"aws")) {
        if(strbuf_truthy(value)) {
            userdata->aws = 1;
            return 0;
        }
        if(strbuf_falsey(value)) {
            userdata->aws = 0;
            return 0;
        }
        LOGS("error parsing aws value %.*s",(*value));
        return -1;
    }

    if(strbuf_equals_cstr(key,"delete")) {
        if(strbuf_truthy(value)) {
            userdata->delete = 1;
            return 0;
        }
        if(strbuf_falsey(value)) {
            userdata->delete = 0;
            return 0;
        }
        LOGS("error parsing delete value %.*s",(*value));
        return -1;
    }

    if(strbuf_equals_cstr(key,"aws_region") ||
       strbuf_equals_cstr(key,"aws region") ||
       strbuf_equals_cstr(key,"aws-region")) {
        if( (r = strbuf_copy(&userdata->aws_region,value)) != 0) {
            LOG0("error copying aws region string");
            return r;
        }
        if(userdata->aws == 2) userdata->aws = 1;
        return 0;
    }

    if(strbuf_equals_cstr(key,"aws_service") ||
       strbuf_equals_cstr(key,"aws service") ||
       strbuf_equals_cstr(key,"aws-service")) {
        if( (r = strbuf_copy(&userdata->aws_service,value)) != 0) {
            LOG0("error copying aws service string");
            return r;
        }
        if(userdata->aws == 2) userdata->aws = 1;
        return 0;
    }

    if(strbuf_equals_cstr(key,"aws_access_key_id") ||
       strbuf_equals_cstr(key,"aws access key id") ||
       strbuf_equals_cstr(key,"aws-access-key-id")) {
        if( (r = strbuf_copy(&userdata->aws_access_key_id,value)) != 0) {
            LOG0("error copying aws access key id string");
            return r;
        }
        if(userdata->aws == 2) userdata->aws = 1;
        return 0;
    }

    if(strbuf_equals_cstr(key,"aws_secret_access_key") ||
       strbuf_equals_cstr(key,"aws secret access key") ||
       strbuf_equals_cstr(key,"aws-secret-access-key")) {
        if( (r = strbuf_copy(&userdata->aws_secret_access_key,value)) != 0) {
            LOG0("error copying aws secret access key string");
            return r;
        }
        if(userdata->aws == 2) userdata->aws = 1;
        return 0;
    }

    if(strbuf_equals_cstr(key,"put header") ||
       strbuf_equals_cstr(key,"put-header") ||
       strbuf_equals_cstr(key,"put_header")) {
        if( (r = strbuf_cat(&userdata->put_headers,value)) != 0) {
            LOG0("error copying put headers");
            return r;
        }
        if( (r = strbuf_term(&userdata->put_headers)) != 0) {
            LOG0("error terminating put headers");
            return r;
        }
        return 0;
    }

    if(strbuf_equals_cstr(key,"delete header") ||
       strbuf_equals_cstr(key,"delete-header") ||
       strbuf_equals_cstr(key,"delete_header")) {
        if( (r = strbuf_cat(&userdata->delete_headers,value)) != 0) {
            LOG0("error copying delete headers");
            return r;
        }
        if( (r = strbuf_term(&userdata->delete_headers)) != 0) {
            LOG0("error terminating delete headers");
            return r;
        }
        return 0;
    }

    if(strbuf_equals_cstr(key,"header")) {
        if( (r = strbuf_cat(&userdata->shared_headers,value)) != 0) {
            LOG0("error copying shared headers");
            return r;
        }
        if( (r = strbuf_term(&userdata->shared_headers)) != 0) {
            LOG0("error terminating shared headers");
            return r;
        }
        return 0;
    }

    LOGS("unknown key \"%.*s\"\n",(*key));
    return -1;
}

static int output_plugin_curl_get_segment_info(const void* ud, const segment_source_info* info, segment_params* params) {
    const output_plugin_curl_userdata* userdata = (const output_plugin_curl_userdata*)ud;
    return hls_get_segment_info(&userdata->hls, info, params);
}

static int output_plugin_curl_open(void* ud, const segment_source* source) {
    char *envvar;
    output_plugin_curl_userdata* userdata = (output_plugin_curl_userdata*)ud;

    if(userdata->url.len == 0) {
        LOG0("url not provided");
        return -1;
    }

    if(userdata->http_password.len > 0) {
        if(userdata->http_username.len == 0) {
            LOG0("http password given without username");
            return -1;
        }
        if(strbuf_copy(&userdata->http_userpwd,&userdata->http_username) !=0) {
            LOG0("error appending username to userpwd");
            return -1;
        }
        if(strbuf_append_cstr(&userdata->http_userpwd,":") != 0) {
            LOG0("error appending colon to userpwd");
            return -1;
        }
        if(strbuf_cat(&userdata->http_userpwd,&userdata->http_password) !=0) {
            LOG0("error appending password to userpwd");
            return -1;
        }
        if(strbuf_term(&userdata->http_userpwd) != 0) {
            LOG0("error terminating userpwd string");
            return -1;
        }
    } else if(userdata->http_username.len > 0) {
        LOG0("http username given without password");
        return -1;
    }

    if(userdata->aws == 1) {
        /* first try to get the access key, ignore any username/password that was given */
        userdata->http_userpwd.len = 0;

        if(userdata->aws_access_key_id.len == 0) {
            envvar = getenv("AWS_ACCESS_KEY_ID");
            if(envvar == NULL) {
                LOG0("aws request but no AWS_ACCESS_KEY_ID given");
                return -1;
            }
            if(strbuf_append_cstr(&userdata->http_userpwd,envvar) != 0) {
                LOG0("error appending AWS_ACCESS_KEY_ID to userpwd");
                return -1;
            }
        } else {
            if(strbuf_cat(&userdata->http_userpwd,&userdata->aws_access_key_id) != 0) {
                LOG0("error appending AWS_ACCESS_KEY_ID to userpwd");
                return -1;
            }
        }
        if(strbuf_append_cstr(&userdata->http_userpwd,":") != 0) {
            LOG0("error appending coloin to userpwd");
            return -1;
        }
        if(userdata->aws_secret_access_key.len == 0) {
            envvar = getenv("AWS_SECRET_ACCESS_KEY");
            if(envvar == NULL) {
                LOG0("aws request but no AWS_SECRET_ACCESS_KEY given");
                return -1;
            }
            if(strbuf_append_cstr(&userdata->http_userpwd,envvar) != 0) {
                LOG0("error appending AWS_SECRET_ACCESS_KEY to userpwd");
                return -1;
            }
        } else {
            if(strbuf_cat(&userdata->http_userpwd,&userdata->aws_secret_access_key) != 0) {
                LOG0("error appending AWS_SECRET_ACCESS_KEY to userpwd");
                return -1;
            }
        }
        if(strbuf_term(&userdata->http_userpwd) != 0) {
            LOG0("error terminating userpwd");
            return -1;
        }

        if(strbuf_append_cstr(&userdata->aws_param,"aws:amz") != 0) {
            LOG0("error appending aws provider info");
            return -1;
        }
        if(userdata->aws_region.len > 0) {
            if(strbuf_append_cstr(&userdata->aws_param,":") != 0) {
                LOG0("error appending aws colon");
                return -1;
            }
            if(strbuf_cat(&userdata->aws_param,&userdata->aws_region) != 0) {
                LOG0("error appending aws region");
                return -1;
            }
            /* we only append the service if a region is given too */
            if(userdata->aws_service.len > 0) {
                if(strbuf_append_cstr(&userdata->aws_param,":") != 0) {
                    LOG0("error appending aws colon");
                    return -1;
                }
                if(strbuf_cat(&userdata->aws_param,&userdata->aws_service) != 0) {
                    LOG0("error appending aws service");
                    return -1;
                }
            }
        }
        if(strbuf_term(&userdata->aws_param) != 0) {
            LOG0("error terminating aws param");
            return -1;
        }
    }

    userdata->handle = curl_easy_init();
    if(userdata->handle == NULL) {
        LOG0("error creating curl handle");
        return -1;
    }

    return hls_open(&userdata->hls, source);
}

static void output_plugin_curl_close(void* ud) {
    output_plugin_curl_userdata* userdata = (output_plugin_curl_userdata*)ud;
    strbuf_free(&userdata->tmp);
    strbuf_free(&userdata->url);
    strbuf_free(&userdata->picture_filename);

    strbuf_free(&userdata->aws_region);
    strbuf_free(&userdata->aws_service);
    strbuf_free(&userdata->aws_access_key_id);
    strbuf_free(&userdata->aws_secret_access_key);

    strbuf_free(&userdata->http_username);
    strbuf_free(&userdata->http_password);

    strbuf_free(&userdata->put_headers);
    strbuf_free(&userdata->delete_headers);
    strbuf_free(&userdata->shared_headers);

    strbuf_free(&userdata->aws_param);
    strbuf_free(&userdata->http_userpwd);

    hls_free(&userdata->hls);

    if(userdata->handle != NULL) {
        curl_easy_cleanup(userdata->handle);
    }

    if(userdata->headers != NULL) {
        curl_slist_free_all(userdata->headers);
    }

}

static int output_plugin_curl_set_time(void* ud, const ich_time* now) {
    output_plugin_curl_userdata* userdata = (output_plugin_curl_userdata*)ud;
    memcpy(&userdata->hls.now,now,sizeof(ich_time));
    return 0;
}

static int output_plugin_curl_submit_segment(void* ud, const segment* seg) {
    output_plugin_curl_userdata* userdata = (output_plugin_curl_userdata*)ud;
    return hls_add_segment(&userdata->hls,seg);
}

static int output_plugin_curl_submit_picture(void* ud, const picture* src, picture* out) {
    int r;
    output_plugin_curl_userdata* userdata = (output_plugin_curl_userdata*)ud;

    userdata->pictureflag = 1;
    r = hls_submit_picture(&userdata->hls,src,out);
    userdata->pictureflag = 0;

    return r;
}

static int output_plugin_curl_flush(void* ud) {
    output_plugin_curl_userdata* userdata = (output_plugin_curl_userdata*)ud;
    return hls_flush(&userdata->hls);
}

static int output_plugin_curl_reset(void* ud) {
    output_plugin_curl_userdata* userdata = (output_plugin_curl_userdata*)ud;
    return hls_reset(&userdata->hls);
}

static int output_plugin_curl_submit_tags(void* ud, const taglist* tags) {
    (void)ud;
    (void)tags;
    return 0;
}

const output_plugin output_plugin_curl = {
    plugin_name,
    output_plugin_curl_size,
    output_plugin_curl_init,
    output_plugin_curl_deinit,
    output_plugin_curl_create,
    output_plugin_curl_config,
    output_plugin_curl_open,
    output_plugin_curl_close,
    output_plugin_curl_set_time,
    output_plugin_curl_submit_segment,
    output_plugin_curl_submit_picture,
    output_plugin_curl_submit_tags,
    output_plugin_curl_flush,
    output_plugin_curl_reset,
    output_plugin_curl_get_segment_info,
};
