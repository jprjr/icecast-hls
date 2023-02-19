#include "filter_plugin_avfilter.h"
#include "avframe_utils.h"

#include <libavfilter/avfilter.h>
#include <libavutil/samplefmt.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include <libavutil/opt.h>
#include <libavutil/frame.h>
#include <libavutil/channel_layout.h>

#include <errno.h>

struct plugin_userdata {
    AVFilterGraph *graph;
    AVFilterContext *buffersrc;
    AVFilterContext *buffersink;
    AVFilterInOut *inputs;
    AVFilterInOut *outputs;
    AVFrame *av_frame;
    frame frame;
    strbuf filter_string;
    int64_t last_pts;
    int last_nb_samples;
    frame_source src_config;
    frame_source dest_config;
};

typedef struct plugin_userdata plugin_userdata;

static int plugin_init(void) {
    return 0;
}

static void plugin_deinit(void) {
    return;
}

static void* plugin_create(void) {
    plugin_userdata* userdata = malloc(sizeof(plugin_userdata));
    if(userdata == NULL) return NULL;

    userdata->graph = NULL;
    userdata->buffersrc = NULL;
    userdata->buffersink = NULL;
    userdata->inputs = NULL;
    userdata->outputs = NULL;
    userdata->av_frame = NULL;
    userdata->last_pts = 0;
    userdata->last_nb_samples = 0;

    userdata->src_config = frame_source_zero;
    userdata->dest_config = frame_source_zero;

    strbuf_init(&userdata->filter_string);
    frame_init(&userdata->frame);

    return userdata;
}


static int plugin_config(void* ud, const strbuf* key, const strbuf* val) {
    int r;
    plugin_userdata* userdata = (plugin_userdata*)ud;
    if(strbuf_equals_cstr(key,"string")) {
        if(userdata->filter_string.len > 0) {
            fprintf(stderr,"[filter:avfilter] only 1 filter string is supported\n");
            return -1;
        }
        if( (r = strbuf_copy(&userdata->filter_string,val)) != 0) {
            fprintf(stderr,"[filter:avfilter] out of memory\n");
        }
        if( (r = strbuf_term(&userdata->filter_string)) != 0) {
            fprintf(stderr,"[filter:avfilter] out of memory\n");
        }
        return 0;
    }

    fprintf(stderr,"[filter:avfilter] unknown config key %.*s\n",
     (int)key->len,(char *)key->x);
    return -1;

}

static const char* default_in_name = "in";
static const char* default_out_name = "out";

static int plugin_graph_open(plugin_userdata* userdata) {
    const AVFilter* buffer_filter = avfilter_get_by_name("abuffer");
    const AVFilter* buffersink_filter = avfilter_get_by_name("abuffersink");
    const char* in_name = NULL;
    const char* out_name = NULL;
    char args[512];
    char layout[64];
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57,28,100)
    AVChannelLayout ch_layout;
#else
    uint64_t channel_layout;
    int64_t channel_layout_tmp;
#endif
    enum AVSampleFormat fmt;

    if(buffer_filter == NULL || buffersink_filter == NULL) {
        fprintf(stderr,"[filter:avfilter] unable to find abuffer and abuffersink filters\n");
        return -1;
    }

    userdata->graph = avfilter_graph_alloc();
    if(userdata->graph == NULL) {
        fprintf(stderr,"[filter:avfilter] out of memory\n");
        return -1;
    }

#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57,28,100)
    av_channel_layout_default(&ch_layout, userdata->src_config.channels);
    av_channel_layout_describe(&ch_layout, layout, sizeof(layout));
    av_channel_layout_uninit(&ch_layout);
#else
    if( (channel_layout_tmp = av_get_default_channel_layout(userdata->src_config.channels)) < 0) {
        av_strerror(channel_layout_tmp,args,sizeof(args));
        fprintf(stderr,"[filter:avfilter] unable to get channel layout: %s\n", args);
        return -1;
    }
    channel_layout = (uint64_t) channel_layout_tmp;
    av_get_channel_layout_string(layout,sizeof(layout),userdata->src_config.channels,channel_layout);
#endif

    snprintf(args,sizeof(args),
      "time_base=%u/%u:sample_rate=%u:sample_fmt=%s:channel_layout=%s",
        1, userdata->src_config.sample_rate, userdata->src_config.sample_rate,
        av_get_sample_fmt_name(samplefmt_to_avsampleformat(userdata->src_config.format)), layout);

    if(userdata->filter_string.len > 0) {
        if(avfilter_graph_parse_ptr(userdata->graph,(const char *)userdata->filter_string.x,
            &userdata->inputs, &userdata->outputs, NULL) < 0) {
            fprintf(stderr,"[filter:avfilter] unable to parse filter string\n");
            return -1;
        }
        in_name = userdata->inputs->name;
        out_name = userdata->outputs->name;
    }
    else {
        in_name = default_in_name;
        out_name = default_out_name;
    }

    if(avfilter_graph_create_filter(&userdata->buffersrc, buffer_filter,
        in_name,args,NULL,userdata->graph) < 0) {
        fprintf(stderr,"[filter:avfilter] unable to create buffersrc\n");
        return -1;
    }

    if(avfilter_graph_create_filter(&userdata->buffersink, buffersink_filter,
        out_name,NULL,NULL,userdata->graph) < 0) {
        fprintf(stderr,"[filter:avfilter] unable to create buffersink\n");
        return -1;
    }

    if(userdata->inputs != NULL) {
        if(avfilter_link(userdata->buffersrc, 0, userdata->inputs->filter_ctx, 0) < 0) {
            fprintf(stderr,"[filter:avfilter] unable to link buffersrc to graph filter\n");
            return -1;
        }
        if(avfilter_link(userdata->outputs->filter_ctx, 0, userdata->buffersink, 0) < 0) {
            fprintf(stderr,"[filter:avfilter] unable to link graph filter to buffersink\n");
            return -1;
        }
    } else {
        if(avfilter_link(userdata->buffersrc, 0, userdata->buffersink, 0) < 0) {
            fprintf(stderr,"[filter:avfilter] unable to link buffersrc to buffersink\n");
            return -1;
        }
    }

    if(userdata->dest_config.format != SAMPLEFMT_UNKNOWN) {
        fmt = samplefmt_to_avsampleformat(userdata->dest_config.format);

        if(av_opt_set_bin(userdata->buffersink, "sample_fmts",
            (uint8_t*)&fmt,sizeof(fmt), AV_OPT_SEARCH_CHILDREN) < 0) {
            fprintf(stderr,"[filter:avfilter] error setting buffersink format\n");
            return -1;
        }
    }

    if(avfilter_graph_config(userdata->graph,NULL) < 0) {
        fprintf(stderr,"[filter:avfilter] unable to configure graph\n");
        return -1;
    }

    return 0;
}

static void plugin_graph_close(plugin_userdata* userdata) {
    if(userdata->buffersrc != NULL) avfilter_free(userdata->buffersrc);
    if(userdata->buffersink != NULL) avfilter_free(userdata->buffersink);
    if(userdata->inputs != NULL) avfilter_inout_free(&userdata->inputs);
    if(userdata->outputs != NULL) avfilter_inout_free(&userdata->outputs);
    if(userdata->graph  != NULL) avfilter_graph_free(&userdata->graph);
}

static int plugin_handle_frame_source_params(void* ud, const frame_source_params *params) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    int r;

    if(params->format != userdata->dest_config.format) {
        plugin_graph_close(userdata);

        userdata->dest_config.format = params->format;
        if( (r = plugin_graph_open(userdata)) != 0) {
            fprintf(stderr,"[filter:avfilter] error re-opening graph\n");
            return r;
        }
    }

    if(params->duration != 0) {
        av_buffersink_set_frame_size(userdata->buffersink, params->duration);
    }

    return 0;
}

static int plugin_open(void* ud, const frame_source* source, const frame_receiver *dest) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57,28,100)
    AVChannelLayout ch_layout;
#else
    uint64_t channel_layout;
#endif

    userdata->src_config = *source;

    userdata->av_frame = av_frame_alloc();
    if(userdata->av_frame == NULL) {
        fprintf(stderr,"[filter:avfilter] out of memory\n");
        return -1;
    }

    if(plugin_graph_open(userdata) != 0) {
        fprintf(stderr,"[filter:avfilter] error opening graph\n");
        return -1;
    }

#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57,28,100)
    if(av_buffersink_get_ch_layout(userdata->buffersink, &ch_layout) < 0) return -1;
#else
    channel_layout = av_buffersink_get_channel_layout(userdata->buffersink);
#endif

    userdata->dest_config.sample_rate = av_buffersink_get_sample_rate(userdata->buffersink);
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57,28,100)
    userdata->dest_config.channels = ch_layout.nb_channels;
#else
    userdata->dest_config.channels = av_get_channel_layout_nb_channels(channel_layout);
#endif
    userdata->dest_config.format = avsampleformat_to_samplefmt(av_buffersink_get_format(userdata->buffersink));
    userdata->dest_config.handle = userdata;
    userdata->dest_config.set_params = plugin_handle_frame_source_params;

    return dest->open(dest->handle,&userdata->dest_config);
}

static int plugin_run(plugin_userdata* userdata, const frame_receiver* dest) {
    int r;
    int rr;

    while( (r = av_buffersink_get_frame(userdata->buffersink, userdata->av_frame)) >= 0) {
        if( (rr = avframe_to_frame(&userdata->frame,userdata->av_frame)) < 0) return rr;
        av_frame_unref(userdata->av_frame);

        if( (rr = dest->submit_frame(dest->handle, &userdata->frame)) < 0) return rr;
    }

    if( r == AVERROR(EAGAIN) ) return 0;

    if( r == AVERROR_EOF) {
        return dest->flush(dest->handle);
    }

    return r;
}

static int plugin_flush(void* ud, const frame_receiver* dest) {
    int r;
    plugin_userdata* userdata = (plugin_userdata*)ud;

    if( (r = av_buffersrc_close(userdata->buffersrc,userdata->last_pts + userdata->last_nb_samples, 0)) < 0) return r;

    return plugin_run(userdata,dest);
}

static int plugin_submit_frame(void* ud, const frame* frame, const frame_receiver* dest) {
    int r;
    plugin_userdata* userdata = (plugin_userdata*)ud;

    if( (r = frame_to_avframe(userdata->av_frame,frame)) < 0) return r;

    r = av_buffersrc_write_frame(userdata->buffersrc,userdata->av_frame);
    userdata->last_pts = userdata->av_frame->pts;
    userdata->last_nb_samples = userdata->av_frame->nb_samples;

    av_frame_unref(userdata->av_frame);
    if(r < 0) return r;

    return plugin_run(userdata,dest);
}

static void plugin_close(void* ud) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    if(userdata->av_frame != NULL) av_frame_free(&userdata->av_frame);
    plugin_graph_close(userdata);
    frame_free(&userdata->frame);
    strbuf_free(&userdata->filter_string);
    free(userdata);
}


const filter_plugin filter_plugin_avfilter = {
    { .a = 0, .len = 8, .x = (uint8_t*)"avfilter" },
    plugin_init,
    plugin_deinit,
    plugin_create,
    plugin_config,
    plugin_open,
    plugin_close,
    plugin_submit_frame,
    plugin_flush,
};
