#include "filter_plugin_avfilter.h"
#include "avframe_utils.h"

#include <libavfilter/avfilter.h>
#include <libavutil/samplefmt.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include <libavutil/opt.h>
#include <libavutil/frame.h>
#include <libavutil/channel_layout.h>
#include "ffmpeg-versions.h"

#include <errno.h>

static STRBUF_CONST(plugin_name,"avfilter");

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

    uint64_t in_pts;
    uint64_t out_pts;

    frame_source src_config;
    samplefmt dest_format;
};

typedef struct plugin_userdata plugin_userdata;

static int plugin_init(void) {
#if ICH_AVFILTER_REGISTER_ALL
    avfilter_register_all();
#endif
    return 0;
}

static void plugin_deinit(void) {
    return;
}

static size_t plugin_size(void) {
    return sizeof(plugin_userdata);
}

static int plugin_create(void* ud) {
    plugin_userdata* userdata = (plugin_userdata*)ud;

    userdata->graph = NULL;
    userdata->buffersrc = NULL;
    userdata->buffersink = NULL;
    userdata->inputs = NULL;
    userdata->outputs = NULL;
    userdata->av_frame = NULL;
    userdata->last_pts = 0;
    userdata->last_nb_samples = 0;

    userdata->in_pts = 0;
    userdata->out_pts = 0;

    userdata->src_config = frame_source_zero;
    userdata->dest_format = SAMPLEFMT_UNKNOWN;

    strbuf_init(&userdata->filter_string);
    frame_init(&userdata->frame);

    return 0;
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
#if ICH_AVUTIL_CHANNEL_LAYOUT
    AVChannelLayout ch_layout;
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

#if ICH_AVUTIL_CHANNEL_LAYOUT
    av_channel_layout_from_mask(&ch_layout, userdata->src_config.channel_layout);
    av_channel_layout_describe(&ch_layout, layout, sizeof(layout));
    av_channel_layout_uninit(&ch_layout);
#else
    av_get_channel_layout_string(layout,sizeof(layout),av_get_channel_layout_nb_channels(userdata->src_config.channel_layout),userdata->src_config.channel_layout);
#endif

    snprintf(args,sizeof(args),
      "time_base=%u/%u:sample_rate=%u:sample_fmt=%s:channel_layout=%s",
        1, userdata->src_config.sample_rate, userdata->src_config.sample_rate,
        av_get_sample_fmt_name(samplefmt_to_avsampleformat(userdata->src_config.format)), layout);
    fprintf(stderr,"args: %s\n",args);

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

    if(userdata->dest_format != SAMPLEFMT_UNKNOWN) {
        fmt = samplefmt_to_avsampleformat(userdata->dest_format);

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

    userdata->in_pts = 0;

    return 0;
}

static void plugin_graph_close(plugin_userdata* userdata) {
    if(userdata->buffersrc != NULL) {
        avfilter_free(userdata->buffersrc);
        userdata->buffersrc = NULL;
    }
    if(userdata->buffersink != NULL) {
        avfilter_free(userdata->buffersink);
        userdata->buffersink = NULL;
    }
    if(userdata->inputs != NULL) avfilter_inout_free(&userdata->inputs);
    if(userdata->outputs != NULL) avfilter_inout_free(&userdata->outputs);
    if(userdata->graph  != NULL) avfilter_graph_free(&userdata->graph);
}

static int plugin_open(void* ud, const frame_source* source, const frame_receiver *dest) {
    plugin_userdata* userdata = (plugin_userdata*)ud;
    frame_source me = FRAME_SOURCE_ZERO;
#if ICH_AVUTIL_CHANNEL_LAYOUT
    AVChannelLayout ch_layout;
#else
    uint64_t channel_layout;
#endif

    userdata->src_config.format = source->format;
    userdata->src_config.channel_layout = source->channel_layout;
    userdata->src_config.sample_rate = source->sample_rate;

    userdata->av_frame = av_frame_alloc();
    if(userdata->av_frame == NULL) {
        fprintf(stderr,"[filter:avfilter] out of memory\n");
        return -1;
    }

    if(plugin_graph_open(userdata) != 0) {
        fprintf(stderr,"[filter:avfilter] error opening graph\n");
        return -1;
    }

#if ICH_AVUTIL_CHANNEL_LAYOUT
    if(av_buffersink_get_ch_layout(userdata->buffersink, &ch_layout) < 0) return -1;
#elif ICH_AVFILTER_BUFFERSINK_GET
    channel_layout = av_buffersink_get_channel_layout(userdata->buffersink);
#else
    channel_layout = userdata->buffersink->inputs[0]->channel_layout;
#endif

#if ICH_AVFILTER_BUFFERSINK_GET
    me.sample_rate = av_buffersink_get_sample_rate(userdata->buffersink);
#else
    me.sample_rate = userdata->buffersink->inputs[0]->sample_rate;
#endif

#if ICH_AVUTIL_CHANNEL_LAYOUT
    me.channel_layout = ch_layout.u.mask;
#else
    me.channel_layout = channel_layout;
#endif

#if ICH_AVFILTER_BUFFERSINK_GET
    me.format = avsampleformat_to_samplefmt(av_buffersink_get_format(userdata->buffersink));
#else
    me.format = avsampleformat_to_samplefmt(userdata->buffersink->inputs[0]->format);
#endif

    /* save our format for future re-opens on format change */
    userdata->dest_format = me.format;

    return dest->open(dest->handle,&me);
}

static int plugin_run(plugin_userdata* userdata, const frame_receiver* dest) {
    int r;
    int rr;

    while( (r = av_buffersink_get_frame(userdata->buffersink, userdata->av_frame)) >= 0) {
        if( (rr = avframe_to_frame(&userdata->frame,userdata->av_frame)) < 0) return rr;
        av_frame_unref(userdata->av_frame);
        userdata->frame.pts = userdata->out_pts;
        if( (rr = dest->submit_frame(dest->handle, &userdata->frame)) < 0) return rr;
        userdata->out_pts += userdata->frame.duration;
    }

    if( r == AVERROR(EAGAIN) ) return 0;
    if( r == AVERROR_EOF) return 0;

    return r;
}

static int plugin_flush(void* ud, const frame_receiver* dest) {
    int r;
    plugin_userdata* userdata = (plugin_userdata*)ud;

#if ICH_AVFILTER_BUFFERSRC_CLOSE
    if( (r = av_buffersrc_close(userdata->buffersrc,userdata->last_pts + userdata->last_nb_samples, 0)) < 0) return r;
#else
    if( (r = av_buffersrc_add_frame_flags(userdata->buffersrc,NULL,0)) < 0) return r;
#endif

    return plugin_run(userdata,dest);
}

static int plugin_reset(void* ud) {
    plugin_userdata* userdata = (plugin_userdata*)ud;

    plugin_graph_close(userdata);
    av_frame_free(&userdata->av_frame);
    frame_free(&userdata->frame);

    userdata->out_pts = 0;
    userdata->last_pts = 0;
    userdata->last_nb_samples = 0;
    userdata->dest_format = SAMPLEFMT_UNKNOWN;
    return 0;
}

static int plugin_submit_frame(void* ud, const frame* frame, const frame_receiver* dest) {
    int r;
    plugin_userdata* userdata = (plugin_userdata*)ud;

    /* plugin_flush + reset + open are only called if the sample rate or
     * channel count/layout changes, but if we have the input format
     * change we need to deal with that */

    if(frame->format != userdata->src_config.format) {
        userdata->src_config.format = frame->format;
        plugin_reset(userdata);
        if( (r = plugin_graph_open(userdata)) != 0) return r;
        if( (userdata->av_frame = av_frame_alloc()) == NULL) return -1;
        frame_init(&userdata->frame);
    }

    if( (r = frame_to_avframe(userdata->av_frame,frame,0,userdata->src_config.channel_layout)) < 0) return r;

    userdata->av_frame->pts = userdata->in_pts;
    r = av_buffersrc_write_frame(userdata->buffersrc,userdata->av_frame);
    userdata->in_pts += (uint64_t) userdata->av_frame->nb_samples;

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
}


const filter_plugin filter_plugin_avfilter = {
    plugin_name,
    plugin_size,
    plugin_init,
    plugin_deinit,
    plugin_create,
    plugin_config,
    plugin_open,
    plugin_close,
    plugin_submit_frame,
    plugin_flush,
    plugin_reset,
};
