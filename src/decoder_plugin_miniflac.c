#include "decoder_plugin_miniflac.h"
#include "miniflac.h"

#include "pack_u32be.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAX_FLAC_CHANNELS 8

static const char* const plugin_errors[] = {
    "attempted to read an Ogg header packet and it's not FLAC",
    "subframe header specified a reserved type",
    "subframe header found a non-zero value in the reserved bit",
    "encountered an illegal value while parsing the fLaC stream marker",
    "a residual block used a reserved coding method",
    "a metadata header used a reserved type",
    "a metadata header used an invalid type",
    "the frame header lists a reserved sample size",
    "the frame header lists a reserved channel assignment",
    "the frame header sample size is invalid",
    "the frame header sample rate is invalid",
    "the frame header lists a reserved block size",
    "the second reserved bit was non-zero when parsing the frame header",
    "the first reserved bit was non-zero when parsing the frame header",
    "error when parsing a header sync code",
    "error in crc16 while decoding frame footer",
    "error in crc8 while decoding frame header",
    "generic error",
    "end-of-file",
    "ok",
    "end-of-metadata"
};

struct plugin_userdata {
    mflac_t m;
    frame frame; /* the frame we use in callbacks */
    uint32_t sample_rate;
    uint8_t channels;
    uint8_t bps;

    const input* input;
    strbuf tmpstr;
    taglist list;
    uint8_t empty_tags; /* if 1 we keep empty tags */
    uint8_t ignore_tags; /* if 1 we ignore all tags */
};

typedef struct plugin_userdata plugin_userdata;

static int plugin_init(void) {
    return 0;
}

static void plugin_deinit(void) {
    return;
}

static void* plugin_create(void) {
    plugin_userdata* userdata = (plugin_userdata*)malloc(sizeof(plugin_userdata));
    if(userdata == NULL) return userdata;
    frame_init(&userdata->frame);
    strbuf_init(&userdata->tmpstr);
    taglist_init(&userdata->list);
    userdata->input = NULL;
    userdata->empty_tags = 0;
    return userdata;
}

static int plugin_config(void* ud, const strbuf* key, const strbuf* value) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    if(strbuf_ends_cstr(key,"empty tags") || strbuf_ends_cstr(key,"empty-tags")) {
        if(strbuf_truthy(value)) {
            userdata->empty_tags = 1;
            return 0;
        }
        if(strbuf_falsey(value)) {
            userdata->empty_tags = 0;
            return 0;
        }
        if(strbuf_caseequals_cstr(value,"keep")) {
            userdata->empty_tags = 1;
            return 0;
        }
        if(strbuf_caseequals_cstr(value,"remove")) {
            userdata->empty_tags = 0;
            return 0;
        }
        fprintf(stderr,"[decoder:miniflac] unknown value for key %.*s: %.*s\n",
          (int)key->len,(char *)key->x,(int)value->len,(char *)value->x);
        return -1;
    }

    if(strbuf_ends_cstr(key,"ignore tags") || strbuf_ends_cstr(key,"ignore-tags")) {
        if(strbuf_truthy(value)) {
            userdata->ignore_tags = 1;
            return 0;
        }
        if(strbuf_falsey(value)) {
            userdata->ignore_tags = 0;
            return 0;
        }
        fprintf(stderr,"[decoder:miniflac] unknown value for key %.*s: %.*s\n",
          (int)key->len,(char *)key->x,(int)value->len,(char *)value->x);
        return -1;
    }

    fprintf(stderr,"[decoder:miniflac] unknown key %.*s\n",
      (int)key->len,(char *)key->x);
    return -1;
}

static void plugin_strerror(MINIFLAC_RESULT res) {
    fprintf(stderr,"[decoder:miniflac]: %s\n", plugin_errors[res+18]);
}

#define TRY(x) \
if( (res = (x)) != MFLAC_OK) { \
    plugin_strerror((MINIFLAC_RESULT)res); \
    return -1; \
}

#define TRYC(x) \
if( (res = (x)) != MFLAC_OK) { \
    plugin_strerror((MINIFLAC_RESULT)res); \
    goto cleanup; \
}

static size_t plugin_mflac_read(uint8_t* buffer, size_t len, void* ud) {
    plugin_userdata* userdata = (plugin_userdata*)ud;

    return input_read(userdata->input, buffer, len);
}

static int plugin_handle_source_params(void* ud, const frame_source_params* params) {
    (void)ud;
    switch(params->format) {
        case SAMPLEFMT_UNKNOWN: /* fall-through */
        case SAMPLEFMT_S32P: return 0;
        default: break;
    }
    fprintf(stderr,"[decoder:miniflac] an upstream source is trying to change our format\n");
    return -1;
}

static int plugin_open(void* ud, const input* in, const frame_receiver* dest) {
    MFLAC_RESULT res;
    plugin_userdata* userdata = (plugin_userdata*)ud;

    frame_source me = FRAME_SOURCE_ZERO;

    userdata->input = in;

    mflac_init(&userdata->m, MINIFLAC_CONTAINER_UNKNOWN, plugin_mflac_read, userdata);

    TRY(mflac_sync(&userdata->m))

    if(userdata->m.flac.state != MINIFLAC_METADATA || userdata->m.flac.metadata.header.type != MINIFLAC_METADATA_STREAMINFO) {
        fprintf(stderr,"[decoder:miniflac] expected a streaminfo block but found another kind\n");
        return -1;
    }

    TRY(mflac_streaminfo_sample_rate(&userdata->m,&userdata->sample_rate))
    TRY(mflac_streaminfo_channels(&userdata->m,&userdata->channels))
    TRY(mflac_streaminfo_bps(&userdata->m,&userdata->bps))

    if(userdata->channels > MAX_FLAC_CHANNELS) {
        fprintf(stderr,"[decoder:miniflac] streaminfo reported %u audio channels, maximum allowed is %u\n",userdata->channels,MAX_FLAC_CHANNELS);
        return -1;
    }

    userdata->frame.channels = userdata->channels;
    userdata->frame.format = SAMPLEFMT_S32P;


    if(frame_ready(&userdata->frame) != 0) {
        fprintf(stderr,"[decoder:miniflac] error allocating output frame\n");
        return -1;
    }

    me.channels = userdata->channels;
    me.sample_rate = userdata->sample_rate;
    me.format = SAMPLEFMT_S32P;
    me.handle = userdata;
    me.set_params = plugin_handle_source_params;

    if(dest->open(dest->handle,&me) != 0) {
        fprintf(stderr,"[decoder:miniflac] error opening audio destination\n");
        return -1;
    }

    return 0;
}

static int plugin_process_vorbis_comment(plugin_userdata *userdata) {
    MFLAC_RESULT res;
    int r;
    uint32_t comment_len;

    strbuf key;
    strbuf val;
    strbuf eq;

    while( (res = mflac_vorbis_comment_length(&userdata->m,&comment_len)) == MFLAC_OK) {
        if(strbuf_ready(&userdata->tmpstr,comment_len) != 0) {
            fprintf(stderr,"[decoder:miniflac] %u out of memory\n",__LINE__);
            abort();
            return -1;
        }

        TRY(mflac_vorbis_comment_string(&userdata->m,(char *)userdata->tmpstr.x,userdata->tmpstr.a,&comment_len))
        userdata->tmpstr.len = comment_len;

        if(strbuf_chrbuf(&eq,&userdata->tmpstr,'=') != 0) continue;

        val.x = &eq.x[1];
        val.len = eq.len - 1;

        key.x   = userdata->tmpstr.x;
        key.len = userdata->tmpstr.len - eq.len;
        if(key.len == 0) continue;
        if(val.len == 0 && !userdata->empty_tags) continue;

        strbuf_lower(&key);

        if( (r = taglist_add(&userdata->list,&key,&val)) != 0) return r;
    }

    if(res != MFLAC_METADATA_END) {
        plugin_strerror((MINIFLAC_RESULT)res);
        return -1;
    }

    return 0;
}

static int plugin_process_picture(plugin_userdata *userdata) {
    /* we'll basically just save the whole block into a tag named METADATA_PICTURE_BLOCK,
     * which means re-creating it in a membuf */
    int r = -1;
    MFLAC_RESULT res = MFLAC_OK;
    uint32_t u32 = 0;
    membuf t = MEMBUF_ZERO;
    strbuf key = STRBUF_ZERO;

    key.x   = (uint8_t*)"metadata_picture_block";
    key.len = strlen("metadata_picture_block");

    membuf_init(&t);

    TRYC(mflac_picture_type(&userdata->m,&u32))
    if(membuf_readyplus(&t,32) != 0) goto cleanup;
    pack_u32be(&t.x[t.len],u32);
    t.len += 4;

    TRYC(mflac_picture_mime_length(&userdata->m,&u32))
    if(membuf_readyplus(&t,u32) != 0) goto cleanup;
    pack_u32be(&t.x[t.len],u32);
    t.len += 4;

    TRYC(mflac_picture_mime_string(&userdata->m,(char *)&t.x[t.len],t.a,&u32));
    t.len += u32;

    TRYC(mflac_picture_description_length(&userdata->m,&u32))
    if(membuf_readyplus(&t,u32) != 0) goto cleanup;
    pack_u32be(&t.x[t.len],u32);
    t.len += 4;

    TRYC(mflac_picture_description_string(&userdata->m,(char *)&t.x[t.len],t.a,&u32));
    t.len += u32;

    TRYC(mflac_picture_width(&userdata->m,&u32));
    pack_u32be(&t.x[t.len],u32);
    t.len += 4;

    TRYC(mflac_picture_height(&userdata->m,&u32));
    pack_u32be(&t.x[t.len],u32);
    t.len += 4;

    TRYC(mflac_picture_colordepth(&userdata->m,&u32));
    pack_u32be(&t.x[t.len],u32);
    t.len += 4;

    TRYC(mflac_picture_totalcolors(&userdata->m,&u32));
    pack_u32be(&t.x[t.len],u32);
    t.len += 4;

    TRYC(mflac_picture_length(&userdata->m,&u32));
    if(membuf_readyplus(&t,u32) != 0) goto cleanup;
    pack_u32be(&t.x[t.len],u32);
    t.len += 4;

    TRYC(mflac_picture_data(&userdata->m,&t.x[t.len],t.a,&u32));
    t.len += u32;

    if( (r = taglist_add(&userdata->list,&key,&t)) != 0) goto cleanup;

    r = 0;
    cleanup:
    membuf_free(&t);
    return r;
}

static int plugin_process_metadata(plugin_userdata *userdata) {
    if(userdata->m.flac.metadata.header.type == MINIFLAC_METADATA_VORBIS_COMMENT) return plugin_process_vorbis_comment(userdata);
    if(userdata->m.flac.metadata.header.type == MINIFLAC_METADATA_PICTURE) return plugin_process_picture(userdata);
    return 0;
}

static int plugin_process_frame(plugin_userdata* userdata, const frame_receiver* dest) {
    int r;
    unsigned int i, j;
    uint32_t shift;
    int32_t* channel;
    int32_t* ptrs[MAX_FLAC_CHANNELS];
    MFLAC_RESULT res;

    if(dest == NULL || dest->submit_frame == NULL) return 0;

    /* sanity checks */
    if(userdata->sample_rate != userdata->m.flac.frame.header.sample_rate) {
        fprintf(stderr,"[decoder:miniflac] encountered a sample rate change, terminating\n");
        return -1;
    }

    if(userdata->channels != userdata->m.flac.frame.header.channels) {
        fprintf(stderr,"[decoder:miniflac] encountered a channels change, terminating\n");
        return -1;
    }

    /* redundant but maybe in the future, we could handle channel-count changes? */
    userdata->frame.channels    = userdata->m.flac.frame.header.channels;
    userdata->frame.duration    = userdata->m.flac.frame.header.block_size;
    userdata->frame.sample_rate = userdata->m.flac.frame.header.sample_rate;

    if( (r = frame_buffer(&userdata->frame)) != 0) return r;

    /* TODO make sure the FLAC channel ordering matches other codec channel orders */
    for(i=0;i<MAX_FLAC_CHANNELS;i++) {
        ptrs[i] = (int32_t*)frame_get_channel_samples(&userdata->frame,i);
    }

    TRY(mflac_decode(&userdata->m, ptrs))

    /* always upshift */
    shift = 32 - userdata->m.flac.frame.header.bps;
    for(i=0;i<userdata->m.flac.frame.header.channels;i++) {
        channel = (int32_t *)frame_get_channel_samples(&userdata->frame,i);
        for(j=0;j<userdata->m.flac.frame.header.block_size;j++) {
            channel[j] *= (1 << shift);
        }
    }

    r = dest->submit_frame(dest->handle,&userdata->frame);

    userdata->frame.pts += userdata->m.flac.frame.header.block_size;

    return r;
}

static int plugin_decode(void* ud, const tag_handler* tag_handler, const frame_receiver* frame_dest) {
    MFLAC_RESULT res;
    int r;
    plugin_userdata* userdata = (plugin_userdata*)ud;

    while( (res = mflac_sync(&userdata->m)) == MFLAC_OK) {
        if(! (userdata->m.flac.state == MINIFLAC_METADATA || userdata->m.flac.state == MINIFLAC_FRAME)) return -1;

        if(userdata->m.flac.state == MINIFLAC_METADATA) {
            if(userdata->ignore_tags) continue;
            /* we want to get through all metadata blocks to gather
             * pictures etc in a single go */
            taglist_reset(&userdata->list);
            while(userdata->m.flac.state == MINIFLAC_METADATA) {
                if( (r = plugin_process_metadata(userdata)) != 0) return r;
                if( (res = mflac_sync(&userdata->m)) != MFLAC_OK) goto end;
            }
            if(taglist_len(&userdata->list) > 0) {
                if( (r = tag_handler->cb(tag_handler->userdata, &userdata->list)) != 0) return r;
            }
        }

        if(userdata->m.flac.state == MINIFLAC_FRAME) {
            if( (r = plugin_process_frame(userdata, frame_dest)) != 0) return r;
        }
    }

    end:

    if(res != MFLAC_EOF) {
        plugin_strerror((MINIFLAC_RESULT)res);
        return -1;
    }

    return frame_dest->flush(frame_dest->handle);
}


static void plugin_close(void* ud) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    frame_free(&userdata->frame);
    strbuf_free(&userdata->tmpstr);
    taglist_free(&userdata->list);
    free(userdata);
    return;
}

const decoder_plugin decoder_plugin_miniflac = {
    { .a = 0, .len = 8, .x = (uint8_t *)"miniflac" },
    plugin_init,
    plugin_deinit,
    plugin_create,
    plugin_config,
    plugin_open,
    plugin_close,
    plugin_decode,
};
