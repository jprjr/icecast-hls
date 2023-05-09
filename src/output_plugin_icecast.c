#include "output_plugin_icecast.h"
#include "socket.h"
#include "strbuf.h"
#include "membuf.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define LOG0(s) fprintf(stderr,"[output:icecast] "s"\n")
#define LOG1(s, a) fprintf(stderr,"[output:icecast] "s"\n", (a))
#define LOG2(s, a, b) fprintf(stderr,"[output:icecast] "s"\n", (a), (b))
#define LOGS(s, a) LOG2(s, (int)(a).len, (const char *)(a).x )

#define LOGERRNO(s) LOG1(s": %s", strerror(errno))

#define TRYNULL(exp, act) if( (exp) == NULL) { act; r=-1; goto cleanup; }
#define TRY(exp, act) if(!(exp)) { act; r=-1; goto cleanup ; }
#define TRY0(exp, act) if( (r = (exp)) != 0 ) { act; goto cleanup; }
#define TRYS(exp) TRY0(exp, LOG0("out of memory"); abort())

#define BASE64_ENCODE_IMPLEMENTATION
#include "base64encode.h"

struct output_plugin_icecast_userdata {
    SOCKET socket;
    strbuf host;
    strbuf port;
    strbuf mount;
    strbuf username;
    strbuf password;
    strbuf auth;
    strbuf song;
    strbuf scratch;
    strbuf mime_type;
    strbuf ice_name;
    strbuf ice_description;
    strbuf ice_url;
    strbuf ice_genre;
    strbuf ice_bitrate;
    strbuf ice_audio_info;
    strbuf ice_streamtitle;
    uint8_t ice_public;
};

typedef struct output_plugin_icecast_userdata output_plugin_icecast_userdata;

static int output_plugin_icecast_init(void) {
    return ich_socket_init();
}

static void output_plugin_icecast_deinit(void) {
    ich_socket_cleanup();
}

static void* output_plugin_icecast_create(void) {
    int r = -1;
    output_plugin_icecast_userdata* userdata = NULL;

    TRYNULL(userdata = (output_plugin_icecast_userdata*)malloc(sizeof(output_plugin_icecast_userdata)),
      LOGERRNO("error creating plugin"));

    userdata->socket = INVALID_SOCKET;

    strbuf_init(&userdata->host);
    strbuf_init(&userdata->port);
    strbuf_init(&userdata->mount);
    strbuf_init(&userdata->username);
    strbuf_init(&userdata->password);
    strbuf_init(&userdata->auth);
    strbuf_init(&userdata->song);
    strbuf_init(&userdata->scratch);
    strbuf_init(&userdata->mime_type);
    strbuf_init(&userdata->ice_name);
    strbuf_init(&userdata->ice_description);
    strbuf_init(&userdata->ice_url);
    strbuf_init(&userdata->ice_genre);
    strbuf_init(&userdata->ice_bitrate);
    strbuf_init(&userdata->ice_audio_info);
    strbuf_init(&userdata->ice_streamtitle);
    userdata->ice_public = 2; /* 2 = unmapped / don't specify */

    cleanup:
    (void)r;
    return userdata;
}

static void output_plugin_icecast_close(void* ud) {
    output_plugin_icecast_userdata* userdata = (output_plugin_icecast_userdata*)ud;
    if(userdata->socket != INVALID_SOCKET) {
        ich_socket_close(userdata->socket);
    }

    strbuf_free(&userdata->host);
    strbuf_free(&userdata->port);
    strbuf_free(&userdata->mount);
    strbuf_free(&userdata->username);
    strbuf_free(&userdata->password);
    strbuf_free(&userdata->auth);
    strbuf_free(&userdata->song);
    strbuf_free(&userdata->scratch);
    strbuf_free(&userdata->mime_type);
    strbuf_free(&userdata->ice_name);
    strbuf_free(&userdata->ice_description);
    strbuf_free(&userdata->ice_url);
    strbuf_free(&userdata->ice_genre);
    strbuf_free(&userdata->ice_bitrate);
    strbuf_free(&userdata->ice_audio_info);
    strbuf_free(&userdata->ice_streamtitle);

    free(ud);
}

static int output_plugin_icecast_config(void* ud, const strbuf* key, const strbuf* val) {
    int r;
    strbuf k = STRBUF_ZERO;
    output_plugin_icecast_userdata* userdata = (output_plugin_icecast_userdata*)ud;

    if(strbuf_equals_cstr(key,"host")) {
        TRYS(strbuf_copy(&userdata->host,val));
        TRYS(strbuf_term(&userdata->host));
        return 0;
    }

    if(strbuf_equals_cstr(key,"port")) {
        TRYS(strbuf_copy(&userdata->port,val));
        TRYS(strbuf_term(&userdata->port));
        return 0;
    }

    if(strbuf_equals_cstr(key,"mount")) {
        TRYS(strbuf_copy(&userdata->mount,val));
        TRYS(strbuf_term(&userdata->mount));
        return 0;
    }

    if(strbuf_equals_cstr(key,"username")) {
        TRYS(strbuf_copy(&userdata->username,val));
        return 0;
    }

    if(strbuf_equals_cstr(key,"password")) {
        TRYS(strbuf_copy(&userdata->password,val));
        return 0;
    }

    if(key->len > 4 && (strbuf_begins_cstr(key,"ice") || strbuf_begins_cstr(key,"icy"))) {
        k.x = &key->x[4];
        k.len = key->len - 4;

        if(strbuf_ends_cstr(&k,"public")) {
            if(strbuf_truthy(val)) {
                userdata->ice_public = 1;
                return 0;
            }
            if(strbuf_falsey(val)) {
                userdata->ice_public = 0;
                return 0;
            }
            LOGS("error parsing verbose value: %.*s",(*val));
            return -1;
        }

        if(strbuf_ends_cstr(&k,"name")) {
            TRYS(strbuf_copy(&userdata->ice_name,val));
            TRYS(strbuf_term(&userdata->ice_name));
            return 0;
        }

        if(strbuf_ends_cstr(&k,"description")) {
            TRYS(strbuf_copy(&userdata->ice_description,val));
            TRYS(strbuf_term(&userdata->ice_description));
            return 0;
        }

        if(strbuf_ends_cstr(&k,"url")) {
            TRYS(strbuf_copy(&userdata->ice_url,val));
            TRYS(strbuf_term(&userdata->ice_url));
            return 0;
        }

        if(strbuf_ends_cstr(&k,"genre")) {
            TRYS(strbuf_copy(&userdata->ice_genre,val));
            TRYS(strbuf_term(&userdata->ice_genre));
            return 0;
        }

        if(strbuf_ends_cstr(&k,"bitrate")) {
            TRYS(strbuf_copy(&userdata->ice_bitrate,val));
            TRYS(strbuf_term(&userdata->ice_bitrate));
            return 0;
        }

        if(strbuf_ends_cstr(&k,"audio-info") || strbuf_ends_cstr(&k,"audio info")) {
            TRYS(strbuf_copy(&userdata->ice_audio_info,val));
            TRYS(strbuf_term(&userdata->ice_audio_info));
            return 0;
        }

        if(strbuf_ends_cstr(&k,"stream-title") || strbuf_ends_cstr(&k,"stream title") || strbuf_ends_cstr(&k,"streamtitle")) {
            TRYS(strbuf_copy(&userdata->ice_streamtitle,val));
            return 0;
        }

    }

    if(strbuf_equals_cstr(key,"mimetype") || strbuf_equals_cstr(key,"mime-type")) {
        TRYS(strbuf_copy(&userdata->mime_type,val));
        TRYS(strbuf_term(&userdata->mime_type));
        return 0;
    }

    if(strbuf_equals_cstr(key,"stream-title") || strbuf_equals_cstr(key,"stream title") || strbuf_equals_cstr(key,"streamtitle")) {
        TRYS(strbuf_copy(&userdata->ice_streamtitle,val));
        return 0;
    }

    LOGS("unknown key \"%.*s\"",(*key));

    cleanup:
    return -1;
}

static int output_plugin_icecast_open(void* ud, const segment_source* source) {
    int r;
    strbuf t = STRBUF_ZERO;
    segment_source_params params = SEGMENT_SOURCE_PARAMS_ZERO;
    output_plugin_icecast_userdata* userdata = (output_plugin_icecast_userdata*)ud;

    TRY(userdata->host.len != 0, LOG0("no host given"));
    TRY(userdata->port.len != 0, LOG0("no port given"));
    TRY(userdata->mount.len != 0, LOG0("no mount given"));
    TRY(userdata->username.len != 0, LOG0("no username given"));
    TRY(userdata->password.len != 0, LOG0("no password given"));

    if(userdata->mime_type.len == 0) {
        TRYS(strbuf_copy(&userdata->mime_type,source->media_mimetype));
        TRYS(strbuf_term(&userdata->mime_type));
    }

    if(userdata->ice_streamtitle.len == 0) {
        TRYS(strbuf_append_cstr(&userdata->ice_streamtitle,"%a - %t"));
    }

    TRYS(strbuf_copy(&userdata->scratch,&userdata->username));
    TRYS(strbuf_append_cstr(&userdata->scratch,":"));
    TRYS(strbuf_cat(&userdata->scratch,&userdata->password));
    TRYS(strbuf_ready(&userdata->auth, userdata->scratch.len * 4 / 3 + 4));
    userdata->auth.len = userdata->auth.a;
    TRY0(base64encode(userdata->scratch.x, userdata->scratch.len, userdata->auth.x, &userdata->auth.len),
      LOG0("error encoding base64 value"));
    TRYS(strbuf_term(&userdata->auth));

    userdata->scratch.len = 0;
    TRYS(strbuf_sprintf(&userdata->scratch,"PUT %s HTTP/1.1\r\n",
        (const char *)userdata->mount.x));
    TRYS(strbuf_sprintf(&userdata->scratch,"User-Agent: icecast-hls/1.0\r\n"));
    TRYS(strbuf_sprintf(&userdata->scratch,"Host: %s\r\n",
        (const char *)userdata->host.x));
    TRYS(strbuf_sprintf(&userdata->scratch,"Authorization: Basic %s\r\n",
        (const char *)userdata->auth.x));
    TRYS(strbuf_sprintf(&userdata->scratch,"Content-Type: %s\r\n",
        (const char *)userdata->mime_type.x));
    if(userdata->ice_public == 0) {
        TRYS(strbuf_sprintf(&userdata->scratch,"ice-public: 0\r\n"));
    } else if(userdata->ice_public == 1) {
        TRYS(strbuf_sprintf(&userdata->scratch,"ice-public: 1\r\n"));
    }
    if(userdata->ice_name.len > 0) {
        TRYS(strbuf_sprintf(&userdata->scratch,"ice-name: %s\r\n",
          (const char *)userdata->ice_name.x));
    }
    if(userdata->ice_description.len > 0) {
        TRYS(strbuf_sprintf(&userdata->scratch,"ice-description: %s\r\n",
          (const char *)userdata->ice_description.x));
    }
    if(userdata->ice_url.len > 0) {
        TRYS(strbuf_sprintf(&userdata->scratch,"ice-url: %s\r\n",
          (const char *)userdata->ice_url.x));
    }
    if(userdata->ice_genre.len > 0) {
        TRYS(strbuf_sprintf(&userdata->scratch,"ice-genre: %s\r\n",
          (const char *)userdata->ice_genre.x));
    }
    if(userdata->ice_bitrate.len > 0) {
        TRYS(strbuf_sprintf(&userdata->scratch,"ice-bitrate: %s\r\n",
          (const char *)userdata->ice_bitrate.x));
    }
    if(userdata->ice_audio_info.len > 0) {
        TRYS(strbuf_sprintf(&userdata->scratch,"ice-audio-info: %s\r\n",
          (const char *)userdata->ice_audio_info.x));
    }
    TRYS(strbuf_sprintf(&userdata->scratch,"Expect: 100-continue\r\n\r\n"));

    TRY( (userdata->socket = ich_socket_connect((const char *)userdata->host.x, (const char *)userdata->port.x)) != INVALID_SOCKET, LOGERRNO("unable to connect to host"));

    TRY(ich_socket_send(userdata->socket,(const char *)userdata->scratch.x,userdata->scratch.len,5000) == (int)userdata->scratch.len,
      LOG1("error sending headers: %s", strerror(errno)));
    TRY( (r = ich_socket_recv(userdata->socket, (char *)userdata->scratch.x, userdata->scratch.len, 5000)) > 0,
      LOG1("error receiving response: %s",strerror(errno)));
    userdata->scratch.len = r;

    t.x = userdata->scratch.x;
    t.len = userdata->scratch.len;

    if(!strbuf_begins_cstr(&t,"HTTP/1.1 ")) {
        LOGS("expected an HTTP response code but got %.*s",
          userdata->scratch);
        r = -1;
        goto cleanup;
    }
    t.x += 9;
    t.len -= 9;
    if(*t.x++ != '1' ||
       *t.x++ != '0' ||
       *t.x   != '0') {
        LOGS("expected to receive HTTP/1.1 100 Continue got but %.*s",
          userdata->scratch);
        r = -1;
        goto cleanup;
    }

    /* all good to go! */
    /* set out segment length to 1ms, basically flush the packet as soon as its received */
    params.segment_length = 1;
    r = 0;

    cleanup:
    if(r == 0) r = source->set_params(source->handle, &params);
    return r;
}

static int output_plugin_icecast_submit_segment(void* ud, const segment* seg) {
    int r;
    output_plugin_icecast_userdata* userdata = (output_plugin_icecast_userdata*)ud;

    TRY(ich_socket_send(userdata->socket,seg->data,seg->len,5000) == (int)seg->len,
      LOG1("error writing segment: %s", strerror(errno))
    );
    r = 0;

    cleanup:
    return r;
}

static int output_plugin_icecast_set_time(void* ud, const ich_time* now) {
    (void)ud;
    (void)now;
    return 0;
}

static int output_plugin_icecast_submit_picture(void* ud, const picture* src, picture* out) {
    (void)ud;
    (void)src;
    (void)out;
    return 0;
}

static int output_plugin_icecast_flush(void* ud) {
    (void)ud;
    return 0;
}

static const char alphabet[16] = "0123456789ABCDEF";
static const char rfc3986[256] = {
  '\0','\0', '\0', '\0', '\0', '\0', '\0', '\0',
  '\0','\0', '\0', '\0', '\0', '\0', '\0', '\0',
  '\0','\0', '\0', '\0', '\0', '\0', '\0', '\0',
  '\0','\0', '\0', '\0', '\0', '\0', '\0', '\0',
  '\0','\0', '\0', '\0', '\0', '\0', '\0', '\0',
  '\0','\0', '\0', '\0', '\0',  '-',  '.', '\0',
   '0', '1',  '2',  '3',  '4',  '5',  '6',  '7',
   '8', '9', '\0', '\0', '\0', '\0', '\0', '\0',
  '\0', 'A',  'B',  'C',  'D',  'E',  'F',  'G',
   'H', 'I',  'J',  'K',  'L',  'M',  'N',  'O',
   'P', 'Q',  'R',  'S',  'T',  'U',  'V',  'W',
   'X', 'Y',  'Z', '\0', '\0', '\0', '\0',  '_',
  '\0', 'a',  'b',  'c',  'd',  'e',  'f',  'g',
   'h', 'i',  'j',  'k',  'l',  'm',  'n',  'o',
   'p', 'q',  'r',  's',  't',  'u',  'v',  'w',
   'x', 'y',  'z', '\0', '\0', '\0',  '~', '\0',
  '\0','\0', '\0', '\0', '\0', '\0', '\0', '\0',
  '\0','\0', '\0', '\0', '\0', '\0', '\0', '\0',
  '\0','\0', '\0', '\0', '\0', '\0', '\0', '\0',
  '\0','\0', '\0', '\0', '\0', '\0', '\0', '\0',
  '\0','\0', '\0', '\0', '\0', '\0', '\0', '\0',
  '\0','\0', '\0', '\0', '\0', '\0', '\0', '\0',
  '\0','\0', '\0', '\0', '\0', '\0', '\0', '\0',
  '\0','\0', '\0', '\0', '\0', '\0', '\0', '\0',
  '\0','\0', '\0', '\0', '\0', '\0', '\0', '\0',
  '\0','\0', '\0', '\0', '\0', '\0', '\0', '\0',
  '\0','\0', '\0', '\0', '\0', '\0', '\0', '\0',
  '\0','\0', '\0', '\0', '\0', '\0', '\0', '\0',
  '\0','\0', '\0', '\0', '\0', '\0', '\0', '\0',
  '\0','\0', '\0', '\0', '\0', '\0', '\0', '\0',
  '\0','\0', '\0', '\0', '\0', '\0', '\0', '\0',
  '\0','\0', '\0', '\0', '\0', '\0', '\0', '\0',
};

static size_t rfc3986_len(const uint8_t* str, size_t len) {
    size_t l = 0;
    size_t i = 0;
    uint8_t x = 0;
    for(i=0;i<len;i++) {
        l++;
        x = rfc3986[str[i]];
        if(!x) {
            l += 2;
        }
    }
    return l;
}

/* assumes you've allocated a buffer large enough using rfc398_len */
static size_t rfc3986_enc(const uint8_t* src, size_t len, uint8_t* dest) {
    size_t i = 0;
    uint8_t x = 0;
    for(i=0;i<len;i++) {
        x = src[i];
        if(rfc3986[x]) {
            *dest++ = x;
        } else {
            *dest++ = '%';
            *dest++ = alphabet[x >> 4];
            *dest++ = alphabet[x & 0x0F];
        }
    }
    return 0;
}

static int rfc3986_buf(strbuf* dest, const strbuf* src) {
    size_t len;
    int r;
    len = rfc3986_len(src->x,src->len);
    if( (r = strbuf_readyplus(dest,len)) != 0) return r;

    rfc3986_enc(src->x,src->len,&dest->x[dest->len]);
    dest->len += len;
    return 0;
}

static int rfc3986_buf_cstr(strbuf* dest, const char* str) {
    strbuf t = STRBUF_ZERO;
    t.x = (uint8_t *)str;
    t.len = strlen(str);
    return rfc3986_buf(dest,&t);
}

static int output_plugin_icecast_submit_tags(void *ud, const taglist* tags) {
    int r = 0;
    size_t i;
    uint8_t c;
    const tag* t = NULL;
    SOCKET s = INVALID_SOCKET;
    output_plugin_icecast_userdata* userdata = (output_plugin_icecast_userdata*)ud;
    size_t title_idx,artist_idx,album_idx,invalid_idx;

    invalid_idx = taglist_len(tags);
    title_idx   = taglist_find_cstr(tags,"TIT2",0);
    artist_idx  = taglist_find_cstr(tags,"TPE1",0);
    album_idx   = taglist_find_cstr(tags,"TALB",0);

    if(title_idx == invalid_idx &&
       artist_idx == invalid_idx &&
       album_idx == invalid_idx) return 0;

    i = 0;
    userdata->song.len = 0;
    userdata->scratch.len = 0;
    while(i < userdata->ice_streamtitle.len) {
        c = userdata->ice_streamtitle.x[i];
        t = NULL;
        if(c == '%') {
            i++;
            if(i == userdata->ice_streamtitle.len) break;
            c = userdata->ice_streamtitle.x[i];
            switch(c) {
                case '%': {
                    TRYS(strbuf_append_cstr(&userdata->scratch,"%"));
                    goto loopend;
                }
                case 't': {
                    if(title_idx != invalid_idx) {
                        t = taglist_get_tag(tags,title_idx);
                    }
                    break;
                }
                case 'a': {
                    if(artist_idx != invalid_idx) {
                        t = taglist_get_tag(tags,artist_idx);
                    }
                    break;
                }
                case 'A': {
                    if(album_idx != invalid_idx) {
                        t = taglist_get_tag(tags,album_idx);
                    }
                    break;
                }
                default: {
                    LOG1("unknown streamtitle character code %c", (char)c);
                    return -1;
                }
            }
            if(t != NULL) {
                TRYS(strbuf_cat(&userdata->scratch,&t->value));
            } else {
                TRYS(strbuf_append_cstr(&userdata->scratch,"unknown"));
            }
        } else {
            TRYS(strbuf_append(&userdata->scratch,(const char *)&c,1));
        }
        loopend:
        i++;
    }

    if(userdata->scratch.len == 0) return 0;

    TRYS(rfc3986_buf(&userdata->song,&userdata->scratch));

    /* we'll try to use the admin interface to set tags */
    userdata->scratch.len = 0;
    TRYS(strbuf_sprintf(&userdata->scratch,"GET /admin/metadata?mode=updinfo&mount=%s&song=%.*s HTTP/1.0\r\n",
        (const char *)userdata->mount.x,
        (int)userdata->song.len,(const char *)userdata->song.x));
    TRYS(strbuf_sprintf(&userdata->scratch,"Host: %s\r\n",
        (const char *)userdata->host.x));
    TRYS(strbuf_sprintf(&userdata->scratch,"User-Agent: icecast-hls/1.0\r\n"));
    TRYS(strbuf_sprintf(&userdata->scratch,"Authorization: Basic %s\r\n\r\n",
        (const char *)userdata->auth.x));

    TRY( (s = ich_socket_connect((const char *)userdata->host.x, (const char *)userdata->port.x)) != INVALID_SOCKET, LOGERRNO("unable to connect to host"));

    TRY(ich_socket_send(s,(const char *)userdata->scratch.x,userdata->scratch.len,5000) == (int)userdata->scratch.len,
      LOG1("error sending headers: %s", strerror(errno)));
    TRY( (r = ich_socket_recv(s, (char *)userdata->scratch.x, userdata->scratch.len, 5000)) > 0,
      LOG1("error receiving response: %s",strerror(errno)));

    cleanup:
    if(s != INVALID_SOCKET) ich_socket_close(s);
    return 0;
}

const output_plugin output_plugin_icecast = {
    { .a = 0, .len = 7, .x = (uint8_t*)"icecast" },
    output_plugin_icecast_init,
    output_plugin_icecast_deinit,
    output_plugin_icecast_create,
    output_plugin_icecast_config,
    output_plugin_icecast_open,
    output_plugin_icecast_close,
    output_plugin_icecast_set_time,
    output_plugin_icecast_submit_segment,
    output_plugin_icecast_submit_picture,
    output_plugin_icecast_submit_tags,
    output_plugin_icecast_flush,
};
