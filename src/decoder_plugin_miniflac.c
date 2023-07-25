#include "decoder_plugin_miniflac.h"
#include "miniflac.h"
#include "bitreader.h"

#include "strbuf.h"

#include <stdlib.h>

#define MAX_FLAC_CHANNELS 8

#define LOG0(fmt) fprintf(stderr, "[decoder:miniflac] " fmt "\n")
#define LOG1(fmt,a) fprintf(stderr, "[decoder:miniflac] " fmt "\n", (a))
#define LOG2(fmt,a,b) fprintf(stderr, "[decoder:miniflac] " fmt "\n", (a), (b))
#define LOG4(fmt,a,b,c,d) fprintf(stderr, "[decoder:miniflac] " fmt "\n", (a), (b), (c), (d))

static STRBUF_CONST(plugin_name, "miniflac");

struct plugin_userdata {
    miniflac_t m;
    frame frame;
};

typedef struct plugin_userdata plugin_userdata;

static int plugin_init(void) { return 0; }
static void plugin_deinit(void) { return; }

static size_t plugin_size(void) { return sizeof(plugin_userdata); }

static int plugin_create(void *ud) {
    plugin_userdata* userdata = (plugin_userdata*)ud;

    miniflac_init(&userdata->m, MINIFLAC_CONTAINER_NATIVE);
    frame_init(&userdata->frame);

    return 0;
}

static void plugin_close(void* ud) {
    plugin_userdata* userdata = (plugin_userdata*)ud;

    frame_free(&userdata->frame);
}

static int plugin_open(void* ud, const packet_source* src, const frame_receiver* dest) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    frame_source me = FRAME_SOURCE_ZERO;
    bitreader br;

    bitreader_init(&br);
    br.buffer = src->dsi.x;
    br.len    = src->dsi.len;

    /* we'll re-init on each open */
    miniflac_init(&userdata->m, MINIFLAC_CONTAINER_NATIVE);
    userdata->m.state = MINIFLAC_FRAME; /* we'll only ever feed frames */
    /* manually fill in data we've gotten from the STREAMINFO block */
    userdata->m.metadata.streaminfo.sample_rate = src->sample_rate;

    bitreader_discard(&br,16); /* minimum block size */
    bitreader_discard(&br,16); /* maximum block size */
    bitreader_discard(&br,24); /* minimum frame size */
    bitreader_discard(&br,24); /* maximum frame size */
    bitreader_discard(&br,20); /* sample rate */
    bitreader_discard(&br,3); /* chnanels */
    userdata->m.metadata.streaminfo.bps = bitreader_read(&br,5) + 1;

    me.handle = userdata;
    me.format = SAMPLEFMT_S32P;
    me.channel_layout = src->channel_layout;
    me.duration = src->frame_len;
    me.sample_rate = src->sample_rate;

    userdata->frame.channels = channel_count(src->channel_layout);
    userdata->frame.format = SAMPLEFMT_S32P;
    userdata->frame.sample_rate = src->sample_rate;
    userdata->frame.pts = 0;

    if(frame_ready(&userdata->frame) != 0) {
        LOG0("unable to prepare frame");
        return -1;
    }

    return dest->open(dest->handle,&me);
}

static int plugin_decode(void* ud, const packet* src, const frame_receiver* dest) {
    plugin_userdata* userdata = (plugin_userdata*)ud;

    int r;
    unsigned int i, j;
    uint32_t shift;
    uint32_t len;
    uint32_t used;
    uint32_t pos;
    int32_t* channel;
    int32_t* ptrs[MAX_FLAC_CHANNELS];
    MINIFLAC_RESULT res;

    len = src->data.len;
    used = 0;
    pos = 0;

    /* decode in a loop, just in case the demuxer
     * accidentally sent two packets */
    while( (res = miniflac_sync(&userdata->m, &src->data.x[pos], len, &used)) == MINIFLAC_OK) {
        len -= used;
        pos += used;

        /* set the frame duration using the parsed block size, rathern than src->duration */
        userdata->frame.duration = userdata->m.frame.header.block_size;
        if( (r = frame_buffer(&userdata->frame)) != 0) return r;

        for(i=0;i<userdata->frame.channels;i++) {
            ptrs[i] = (int32_t*)frame_get_channel_samples(&userdata->frame,i);
        }

        if( (res = miniflac_decode(&userdata->m, &src->data.x[pos], len, &used, ptrs)) != MINIFLAC_OK) {
            LOG1("error decoding: %d",res);
            return -1;
        }
        len -= used;
        pos += used;

        shift = 32 - userdata->m.frame.header.bps;
        for(i=0;i<userdata->m.frame.header.channels;i++) {
            channel = (int32_t *)frame_get_channel_samples(&userdata->frame,i);
            for(j=0;j<userdata->m.frame.header.block_size;j++) {
                channel[j] *= (1 << shift);
            }
        }

        if( (r = dest->submit_frame(dest->handle, &userdata->frame)) != 0) {
            return r;
        }
        userdata->frame.pts += userdata->m.frame.header.block_size;
    }

    if(res == MINIFLAC_CONTINUE) return 0;

    LOG1("miniflac_sync returned an error: %d",res);
    return -1;
}

static int plugin_flush(void* ud, const frame_receiver* dest) {
    (void)ud;
    (void)dest;
    return 0;
}

static int plugin_reset(void* ud) {
    (void)ud;
    return 0;
}

static int plugin_config(void* ud, const strbuf* key, const strbuf* val) {
    (void)ud;
    (void)key;
    (void)val;
    return 0;
}


const decoder_plugin decoder_plugin_miniflac = {
    plugin_name,
    plugin_size,
    plugin_init,
    plugin_deinit,
    plugin_create,
    plugin_config,
    plugin_open,
    plugin_close,
    plugin_decode,
    plugin_flush,
    plugin_reset,
};

