#include "destination.h"

#define CONFIGURING_UNKNOWN 0
#define CONFIGURING_FILTER 1
#define CONFIGURING_ENCODER 2
#define CONFIGURING_MUXER 3
#define CONFIGURING_OUTPUT 4

static strbuf DEFAULT_FILTER = { .a = 0, .len = 11, .x = (uint8_t*)"passthrough" };
static strbuf DEFAULT_ENCODER = { .a = 0, .len = 6, .x = (uint8_t*)"exhale" };
static strbuf DEFAULT_MUXER = { .a = 0, .len = 4, .x = (uint8_t*)"fmp4" };

int destination_global_init(void) {
    int r;
    if( (r = filter_global_init()) != 0) return r;
    if( (r = encoder_global_init()) != 0) return r;
    if( (r = muxer_global_init()) != 0) return r;
    if( (r = output_global_init()) != 0) return r;
    return 0;
}

void destination_global_deinit(void) {
    filter_global_deinit();
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
    dest->inband_images = 0;
}

void destination_free(destination* dest) {
    strbuf_free(&dest->source_id);
    filter_free(&dest->filter);
    encoder_free(&dest->encoder);
    muxer_free(&dest->muxer);
    output_free(&dest->output);
    return;
}

int destination_submit_frame(const destination* dest, const frame* frame) {
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

    outputconfig_handler handler_output  = OUTPUTCONFIG_HANDLER_ZERO;
    muxerconfig_handler  handler_muxer   =  MUXERCONFIG_HANDLER_ZERO;
    audioconfig_handler  handler_encoder =  AUDIOCONFIG_HANDLER_ZERO;
    audioconfig_handler  handler_filter  =  AUDIOCONFIG_HANDLER_ZERO;

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
    dest->filter.frame_handler.cb       = (frame_handler_callback)encoder_submit_frame;
    dest->filter.frame_handler.flush    = (frame_handler_flush_callback)encoder_flush;
    dest->filter.frame_handler.userdata = &dest->encoder;

    dest->encoder.packet_handler.cb       = (packet_handler_callback)muxer_submit_packet;
    dest->encoder.packet_handler.flush    = (packet_handler_flush_callback)muxer_flush;
    dest->encoder.packet_handler.userdata = &dest->muxer;

    dest->muxer.segment_handler.cb       = (segment_handler_callback)output_submit_segment;
    dest->muxer.segment_handler.flush    = (segment_handler_flush_callback)output_flush;
    dest->muxer.segment_handler.userdata = &dest->output;

    dest->muxer.inband_images = dest->inband_images;

    dest->muxer.picture_handler.cb       = (picture_handler_callback)output_submit_picture;
    dest->muxer.picture_handler.userdata = &dest->output;

    /* set up our config forwarders */
    handler_output.userdata = &dest->output;
    handler_output.submit   = output_open;
    muxer_set_outputconfig_handler(&dest->muxer,&handler_output);

    handler_muxer.userdata = &dest->muxer;
    handler_muxer.submit   = muxer_open;
    handler_muxer.submit_dsi   = muxer_submit_dsi;
    encoder_set_muxerconfig_handler(&dest->encoder,&handler_muxer);

    handler_encoder.userdata = &dest->encoder;
    handler_encoder.open     = encoder_open;
    filter_set_audioconfig_handler(&dest->filter,&handler_encoder);

    handler_filter.userdata = &dest->filter;
    handler_filter.open     = filter_open;

    return source_open_dest(dest->source, &handler_filter);
}

int destination_config(destination* dest, const strbuf* key, const strbuf* val) {
    strbuf tmp;
    int r = -1;

    strbuf_init(&tmp);

    if(strbuf_equals_cstr(key,"source")) {
        if( (r = strbuf_copy(&dest->source_id,val)) != 0) return r;
        r = 0; goto cleanup;
    }

    if(strbuf_equals_cstr(key,"tagmap")) {
        if( (r = strbuf_copy(&dest->tagmap_id,val)) != 0) return r;
        r = 0; goto cleanup;
    }

    if(strbuf_equals_cstr(key,"inband-images") || strbuf_equals_cstr(key,"inband images")) {
        if(strbuf_truthy(val)) {
            dest->inband_images = 1;
            r = 0; goto cleanup;
        }
        if(strbuf_falsey(val)) {
            dest->inband_images = 0;
            r = 0; goto cleanup;
        }
        fprintf(stderr,"[destination] unknown configuration value %.*s for option %.*s\n",
          (int)val->len,(const char *)val->x,
          (int)key->len,(const char *)key->x);
        r = -1; goto cleanup;
    }

    if(strbuf_equals_cstr(key,"unknown tags") || strbuf_equals_cstr(key,"unknown-tags")) {
        if(strbuf_equals_cstr(val,"ignore")) {
            dest->map_flags.unknownmode = TAGMAP_UNKNOWN_IGNORE;
            r = 0; goto cleanup;
        }
        if(strbuf_equals_cstr(val,"txxx")) {
            dest->map_flags.unknownmode = TAGMAP_UNKNOWN_TXXX;
            r = 0; goto cleanup;
        }
        fprintf(stderr,"[destination] unknown configuration value %.*s for option %.*s\n",
          (int)val->len,(const char *)val->x,
          (int)key->len,(const char *)key->x);
        r = -1; goto cleanup;
    }

    if(strbuf_equals_cstr(key,"duplicate tags") || strbuf_equals_cstr(key,"duplicate-tags")) {
        if(strbuf_equals_cstr(val,"ignore")) {
            dest->map_flags.mergemode = TAGMAP_MERGE_IGNORE;
            r = 0; goto cleanup;
        }
        if(strbuf_equals_cstr(val,"null")) {
            dest->map_flags.mergemode = TAGMAP_MERGE_NULL;
            r = 0; goto cleanup;
        }
        if(strbuf_equals_cstr(val,"semicolon")) {
            dest->map_flags.mergemode = TAGMAP_MERGE_SEMICOLON;
            r = 0; goto cleanup;
        }
        fprintf(stderr,"[destination] unknown configuration value %.*s for option %.*s\n",
          (int)val->len,(const char *)val->x,
          (int)key->len,(const char *)key->x);
        r = -1; goto cleanup;
        goto cleanup;
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

    switch(dest->configuring) {
        case CONFIGURING_FILTER: return filter_config(&dest->filter,key,val);
        case CONFIGURING_ENCODER: return encoder_config(&dest->encoder,key,val);
        case CONFIGURING_MUXER: return muxer_config(&dest->muxer,key,val);
        case CONFIGURING_OUTPUT: return output_config(&dest->output,key,val);
        case CONFIGURING_UNKNOWN: /* fall-through */
        default: break;
    }

    fprintf(stderr,"[destination] unknown configuration option %.*s\n",(int)key->len,(const char *)key->x);

    cleanup:
    strbuf_free(&tmp);
    return r;
}

