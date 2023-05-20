#include "destination.h"

#define CONFIGURING_UNKNOWN 0
#define CONFIGURING_FILTER 1
#define CONFIGURING_ENCODER 2
#define CONFIGURING_MUXER 3
#define CONFIGURING_OUTPUT 4

static strbuf DEFAULT_FILTER = { .a = 0, .len = 6, .x = (uint8_t*)"buffer" };
static strbuf DEFAULT_ENCODER = { .a = 0, .len = 6, .x = (uint8_t*)"exhale" };
static strbuf DEFAULT_MUXER = { .a = 0, .len = 4, .x = (uint8_t*)"fmp4" };

int destination_global_init(void) {
    int r;
    /* filter global init was handled in the source global init */
    if( (r = encoder_global_init()) != 0) return r;
    if( (r = muxer_global_init()) != 0) return r;
    if( (r = output_global_init()) != 0) return r;
    return 0;
}

void destination_global_deinit(void) {
    /* filter global deinit was handled in the source global init */
    encoder_global_deinit();
    muxer_global_deinit();
    output_global_deinit();
    return;
}

void destination_init(destination* dest) {
    strbuf_init(&dest->source_id);
    strbuf_init(&dest->tagmap_id);
    filter_init(&dest->filter);
    encoder_init(&dest->encoder);
    muxer_init(&dest->muxer);
    output_init(&dest->output);
    dest->source = NULL;
    dest->tagmap = NULL;
    dest->configuring = 0;
    dest->map_flags.mergemode = TAGMAP_MERGE_IGNORE;
    dest->map_flags.unknownmode = TAGMAP_UNKNOWN_IGNORE;
    dest->map_flags.passthrough = 0;
    dest->image_mode = 0;
}

void destination_free(destination* dest) {
    strbuf_free(&dest->source_id);
    filter_free(&dest->filter);
    encoder_free(&dest->encoder);
    muxer_free(&dest->muxer);
    output_free(&dest->output);
    return;
}

int destination_submit_frame(destination* dest, const frame* frame) {
    return filter_submit_frame(&dest->filter, frame);
}

int destination_flush(const destination* dest) {
    return filter_flush(&dest->filter);
}

int destination_submit_tags(const destination* dest, const taglist* tags) {
    return muxer_submit_tags(&dest->muxer, tags);
}

int destination_open(destination* dest, const ich_time* now) {
    int r;

    frame_receiver source_receiver = FRAME_RECEIVER_ZERO;

    /* ensure we have an output plugin selected, there's no default for that */
    if(dest->output.plugin == NULL) {
        fprintf(stderr,"[destination] no output plugin selected\n");
        return -1;
    }

    /* for everything else, create a default if not given */
    if(dest->filter.plugin == NULL) {
        if( (r = filter_create(&dest->filter, &DEFAULT_FILTER)) != 0) {
            fprintf(stderr,"[destination] unable to create filter plugin\n");
            return r;
        }
    }

    if(dest->encoder.plugin == NULL) {
        if( (r = encoder_create(&dest->encoder, &DEFAULT_ENCODER)) != 0) {
            fprintf(stderr,"[destination] unable to create encoder plugin\n");
            return r;
        }
    }

    if(dest->muxer.plugin == NULL) {
        if( (r = muxer_create(&dest->muxer, &DEFAULT_MUXER)) != 0) {
            fprintf(stderr,"[destination] unable to create muxer plugin\n");
            return r;
        }
    }

    if(output_set_time(&dest->output,now) != 0) {
        fprintf(stderr,"[destination] error setting output time\n");
        return -1;
    }

    /* set up for audio data forwards (frame -> packet -> segment -> output) */
    dest->filter.frame_receiver.open          = (frame_receiver_open_cb)encoder_open;
    dest->filter.frame_receiver.submit_frame  = (frame_receiver_submit_frame_cb)encoder_submit_frame;
    dest->filter.frame_receiver.flush         = (frame_receiver_flush_cb)encoder_flush;
    dest->filter.frame_receiver.handle        = &dest->encoder;

    dest->encoder.packet_receiver.open          = (packet_receiver_open_cb)muxer_open;
    dest->encoder.packet_receiver.submit_dsi    = (packet_receiver_submit_dsi_cb)muxer_submit_dsi;
    dest->encoder.packet_receiver.submit_packet = (packet_receiver_submit_packet_cb)muxer_submit_packet;
    dest->encoder.packet_receiver.flush         = (packet_receiver_flush_cb)muxer_flush;
    dest->encoder.packet_receiver.get_caps      = (packet_receiver_get_caps_cb)muxer_get_caps;
    dest->encoder.packet_receiver.handle        = &dest->muxer;

    dest->muxer.segment_receiver.open             = (segment_receiver_open_cb)output_open;
    dest->muxer.segment_receiver.submit_segment   = (segment_receiver_submit_segment_cb)output_submit_segment;
    dest->muxer.segment_receiver.submit_tags      = (segment_receiver_submit_tags_cb)output_submit_tags;
    dest->muxer.segment_receiver.flush            = (segment_receiver_flush_cb)output_flush;
    dest->muxer.segment_receiver.handle           = &dest->output;

    dest->muxer.image_mode = dest->image_mode;

    dest->muxer.picture_handler.cb       = (picture_handler_callback)output_submit_picture;
    dest->muxer.picture_handler.userdata = &dest->output;

    /* finally our dummy receiver for the source */
    source_receiver.open         = (frame_receiver_open_cb)filter_open;
    source_receiver.submit_frame = (frame_receiver_submit_frame_cb)filter_submit_frame;
    source_receiver.flush        = (frame_receiver_flush_cb)filter_flush;
    source_receiver.handle       = &dest->filter;

    /* let's gooooo */
    return source_open_dest(dest->source, &source_receiver);
}

int destination_config(destination* dest, const strbuf* key, const strbuf* val) {
    strbuf t = STRBUF_ZERO;
    int r = -1;
    int f;

    if(strbuf_equals_cstr(key,"source")) {
        if( (r = strbuf_copy(&dest->source_id,val)) != 0) return r;
        return 0;
    }

    if(strbuf_equals_cstr(key,"tagmap")) {
        if(strbuf_caseequals_cstr(val,"disable") ||
           strbuf_caseequals_cstr(val,"disabled") ||
           strbuf_caseequals_cstr(val,"false")) {
            dest->map_flags.passthrough = 1;
            return 0;
        }
        if( (r = strbuf_copy(&dest->tagmap_id,val)) != 0) return r;
        return 0;
    }

    if(strbuf_equals_cstr(key,"images")) {
        f = 0;
        if(strbuf_casecontains_cstr(val,"keep")) {
            dest->image_mode |= IMAGE_MODE_KEEP;
            f++;
        }

        if(strbuf_casecontains_cstr(val,"inband")) {
            dest->image_mode |= IMAGE_MODE_INBAND;
            f++;
        }

        if(strbuf_casecontains_cstr(val,"in-band")) {
            dest->image_mode |= IMAGE_MODE_INBAND;
            f++;
        }

        if(strbuf_casecontains_cstr(val,"out-of-band")) {
            dest->image_mode &= ~IMAGE_MODE_INBAND;
            f++;
        }

        if(strbuf_casecontains_cstr(val,"oob")) {
            dest->image_mode &= ~IMAGE_MODE_INBAND;
            f++;
        }

        if(strbuf_casecontains_cstr(val,"outofband")) {
            dest->image_mode &= ~IMAGE_MODE_INBAND;
            f++;
        }

        if(strbuf_casecontains_cstr(val,"remove")) {
            dest->image_mode = 0;
            f++;
        }

        if(f > 0) {
            return 0;
        }

        fprintf(stderr,"[destination] unknown configuration value %.*s for option %.*s\n",
          (int)val->len,(const char *)val->x,
          (int)key->len,(const char *)key->x);
        return -1;
    }

    if(strbuf_equals_cstr(key,"unknown tags") || strbuf_equals_cstr(key,"unknown-tags")) {
        if(strbuf_equals_cstr(val,"ignore")) {
            dest->map_flags.unknownmode = TAGMAP_UNKNOWN_IGNORE;
            return 0;
        }
        if(strbuf_equals_cstr(val,"txxx")) {
            dest->map_flags.unknownmode = TAGMAP_UNKNOWN_TXXX;
            return 0;
        }
        fprintf(stderr,"[destination] unknown configuration value %.*s for option %.*s\n",
          (int)val->len,(const char *)val->x,
          (int)key->len,(const char *)key->x);
        return -1;
    }

    if(strbuf_equals_cstr(key,"duplicate tags") || strbuf_equals_cstr(key,"duplicate-tags")) {
        if(strbuf_equals_cstr(val,"ignore")) {
            dest->map_flags.mergemode = TAGMAP_MERGE_IGNORE;
            return 0;
        }
        if(strbuf_equals_cstr(val,"null")) {
            dest->map_flags.mergemode = TAGMAP_MERGE_NULL;
            return 0;
        }
        if(strbuf_equals_cstr(val,"semicolon")) {
            dest->map_flags.mergemode = TAGMAP_MERGE_SEMICOLON;
            return 0;
        }
        fprintf(stderr,"[destination] unknown configuration value %.*s for option %.*s\n",
          (int)val->len,(const char *)val->x,
          (int)key->len,(const char *)key->x);
        return -1;
    }

    if(strbuf_equals_cstr(key,"filter")) {
        if( (r = filter_create(&dest->filter,val)) != 0) return r;
        dest->configuring = CONFIGURING_FILTER;
        return 0;
    }

    if(strbuf_equals_cstr(key,"encoder")) {
        if( (r = encoder_create(&dest->encoder,val)) != 0) return r;
        dest->configuring = CONFIGURING_ENCODER;
        return 0;
    }

    if(strbuf_equals_cstr(key,"muxer")) {
        if( (r = muxer_create(&dest->muxer,val)) != 0) return r;
        dest->configuring = CONFIGURING_MUXER;
        return 0;
    }

    if(strbuf_equals_cstr(key,"output")) {
        if( (r = output_create(&dest->output,val)) != 0) return r;
        dest->configuring = CONFIGURING_OUTPUT;
        return 0;
    }

    if(strbuf_begins_cstr(key,"filter-")) {
        t.x = &key->x[7];
        t.len = key->len - 7;
        return filter_config(&dest->filter,&t,val);
    }
    if(strbuf_begins_cstr(key,"encoder-")) {
        t.x = &key->x[8];
        t.len = key->len - 8;
        return encoder_config(&dest->encoder,&t,val);
    }
    if(strbuf_begins_cstr(key,"muxer-")) {
        t.x = &key->x[6];
        t.len = key->len - 6;
        return muxer_config(&dest->muxer,&t,val);
    }
    if(strbuf_begins_cstr(key,"output-")) {
        t.x = &key->x[7];
        t.len = key->len - 7;
        return output_config(&dest->output,&t,val);
    }

    switch(dest->configuring) {
        case CONFIGURING_FILTER: return filter_config(&dest->filter,key,val);
        case CONFIGURING_ENCODER: return encoder_config(&dest->encoder,key,val);
        case CONFIGURING_MUXER: return muxer_config(&dest->muxer,key,val);
        case CONFIGURING_OUTPUT: return output_config(&dest->output,key,val);
        case CONFIGURING_UNKNOWN: /* fall-through */
        default: break;
    }

    fprintf(stderr,"[destination] unknown configuration option %.*s\n",(int)key->len,(const char *)key->x);

    return r;
}

void destination_dump_counters(const destination* dest, const strbuf* prefix) {
    filter_dump_counters(&dest->filter,prefix);
    encoder_dump_counters(&dest->encoder,prefix);
    muxer_dump_counters(&dest->muxer,prefix);
    output_dump_counters(&dest->output,prefix);
}
