#include "decoder.h"
#include "decoder_plugin.h"

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void decoder_init(decoder* dec) {
    dec->userdata = NULL;
    dec->plugin = NULL;
    dec->frame_receiver = frame_receiver_zero;
    frame_init(&dec->frame);
    dec->frame_source = frame_source_zero;
    dec->pts = 0;
}

void decoder_free(decoder* dec) {
    if(dec->userdata != NULL) {
        dec->plugin->close(dec->userdata);
        free(dec->userdata);
    }
    frame_free(&dec->frame);
    dec->userdata = NULL;
    dec->plugin = NULL;
}

int decoder_create(decoder* dec, const strbuf* name) {
    const decoder_plugin* plug;
    void* userdata;

    plug = decoder_plugin_get(name);
    if(plug == NULL) return -1;

    userdata = malloc(plug->size());
    if(userdata == NULL) return -1;

    dec->userdata = userdata;
    dec->plugin = plug;

    return dec->plugin->create(dec->userdata);
}

static int decoder_open_wrapper(void* ud, const frame_source* source) {
    decoder* dec = (decoder *)ud;
    int r;

    /* if the decoder is putting out a different format (or
     * is binary), we'll want to call reset on the receiver */

    switch(dec->frame_source.format) {
        default: {
            /* if we're spitting out the same sample rate
             * and channel count we don't need to reopen
             * the receiver, otherwise we'll need to re-open.
             * Note that we don't worry about the sample format since
             * sample formats are auto-converted as needed */
            if(dec->frame_source.channel_layout == source->channel_layout &&
               dec->frame_source.sample_rate == source->sample_rate) return 0;
        }
        /* fall-through */
        case SAMPLEFMT_BINARY: {
            if( (r = dec->frame_receiver.flush(dec->frame_receiver.handle)) != 0) return r;
            if( (r = dec->frame_receiver.reset(dec->frame_receiver.handle)) != 0) return r;
            dec->pts = 0;
        }
        /* fall-through */
        case SAMPLEFMT_UNKNOWN: {
            /* initial open or a re-open, save the format for later */
            dec->frame_source.format = source->format;
            dec->frame_source.channel_layout = source->channel_layout;
            dec->frame_source.sample_rate = source->sample_rate;
            break;
        }
    }

    return dec->frame_receiver.open(dec->frame_receiver.handle,source);
}

static int decoder_submit_frame_wrapper(void* ud, const frame* frame) {
    decoder* dec = (decoder *)ud;
    int r;

    if( (r = frame_copy(&dec->frame,frame)) != 0) return r;

    dec->frame.pts = dec->pts;

    if( (r = dec->frame_receiver.submit_frame(dec->frame_receiver.handle, &dec->frame)) != 0) return r;

    dec->pts += dec->frame.duration;
    return r;
}

int decoder_open(decoder* dec, const packet_source *src) {
    frame_receiver receiver = FRAME_RECEIVER_ZERO;

    if(dec->plugin == NULL || dec->userdata == NULL) {
        fprintf(stderr,"[decoder] unable to open: no plugin selected\n");
        return -1;
    }

    ich_time_now(&dec->ts);
    dec->counter = 0;

    receiver.handle = dec;
    receiver.open = decoder_open_wrapper;

    return dec->plugin->open(dec->userdata, src, &receiver);
}

int decoder_config(const decoder* dec, const strbuf* name, const strbuf* value) {
    return dec->plugin->config(dec->userdata,name,value);
}

int decoder_global_init(void) {
    return decoder_plugin_global_init();
}

void decoder_global_deinit(void) {
    decoder_plugin_global_deinit();
}

int decoder_submit_packet(decoder* dec, const packet* p) {
    int r;
    frame_receiver receiver = FRAME_RECEIVER_ZERO;

    receiver.handle = dec;
    receiver.submit_frame = decoder_submit_frame_wrapper;

    r = dec->plugin->decode(dec->userdata, p, &receiver);
    if(r == 0) {
        ich_time_now(&dec->ts);
        dec->counter++;
    }
    return r;
}

int decoder_flush(decoder* dec) {
    int r = dec->plugin->flush(dec->userdata, &dec->frame_receiver);
    if(r == 0) {
        ich_time_now(&dec->ts);
        dec->counter++;
    }
    return r;
}

int decoder_reset(const decoder* dec) {
    return dec->plugin->reset(dec->userdata);
}

void decoder_dump_counters(const decoder* in, const strbuf* prefix) {
    ich_tm tm;
    ich_time_to_tm(&tm,&in->ts);

    fprintf(stderr,"%.*s decoder: decodes=%zu last_read=%4u-%02u-%02u %02u:%02u:%02u\n",
      (int)prefix->len,(const char*)prefix->x,
      in->counter,
      tm.year,tm.month,tm.day,
      tm.hour,tm.min,tm.sec);
}
