#include "output.h"
#include "output_plugin.h"

#include <stdio.h>

void output_init(output* out) {
    out->userdata = NULL;
    out->plugin = NULL;
}

void output_free(output* out) {
    if(out->userdata != NULL) {
        out->plugin->close(out->userdata);
    }
    out->userdata = NULL;
    out->plugin = NULL;
}

int output_create(output* out, const strbuf* name) {
    const output_plugin* plug;
    void* userdata;

    plug = output_plugin_get(name);
    if(plug == NULL) {
        fprintf(stderr,"[output] plugin \"%.*s\" not found\n",
          (int)name->len,(char *)name->x);
        return -1;
    }

    userdata = plug->create();
    if(userdata == NULL) {
        fprintf(stderr,"[output] error creating instance of \"%.*s\"\n",
          (int)name->len,(char *)name->x);
        return -1;
    }

    out->userdata = userdata;
    out->plugin = plug;

    return 0;
}

int output_get_segment_params(const output* out, const segment_source_info* info, segment_params* params) {
    int r = -1;
    uint64_t t;
    if(out->plugin == NULL || out->userdata == NULL) {
        fprintf(stderr,"[output] plugin not selected\n");
        return r;
    }
    if( (r = out->plugin->get_segment_params(out->userdata,info,params)) != 0) return r;

    if(params->segment_length == 0) {
        /* plugin didn't set a length so we set
         * a default of 1s */
        params->segment_length = 1000;
    }

    if(params->packets_per_segment == 0) {
        /* plugin didn't request a number of
         * packets per segment so we'll calculate
         * it here */
        t  = (uint64_t)params->segment_length;
        t *= (uint64_t)info->time_base;
        t /= (uint64_t)1000;
        t /= (uint64_t)info->frame_len;
        params->packets_per_segment = t;
    }

    /* just in case we had some very small value for segment length */
    if(params->packets_per_segment == 0) params->packets_per_segment = 1;

    return 0;
}

int output_open(output* out, const segment_source* source) {
    if(out->plugin == NULL || out->userdata == NULL) {
        fprintf(stderr,"[output] plugin not selected\n");
        return -1;
    }
    ich_time_now(&out->ts);
    out->counter = 0;
    return out->plugin->open(out->userdata, source);
}

int output_config(const output* out, const strbuf* name, const strbuf* value) {
    return out->plugin->config(out->userdata,name,value);
}

int output_set_time(const output* out, const ich_time* now) {
    return out->plugin->set_time(out->userdata,now);
}

int output_submit_segment(output* out, const segment* seg) {
    int r = out->plugin->submit_segment(out->userdata,seg);
    if(r == 0) {
        ich_time_now(&out->ts);
        out->counter++;
    }
    return r;
}

int output_submit_tags(const output* out, const taglist* tags) {
    return out->plugin->submit_tags(out->userdata,tags);
}

int output_submit_picture(const output* out, const picture* seg, picture* p) {
    return out->plugin->submit_picture(out->userdata,seg,p);
}

int output_flush(const output* out) {
    return out->plugin->flush(out->userdata);
}

int output_global_init(void) {
    return output_plugin_global_init();
}

void output_global_deinit(void) {
    output_plugin_global_deinit();
}

void output_dump_counters(const output* in, const strbuf* prefix) {
    ich_tm tm;
    ich_time_to_tm(&tm,&in->ts);

    fprintf(stderr,"%.*s output: outputs=%zu last_output=%4u-%02u-%02u %02u:%02u:%02u\n",
      (int)prefix->len,(const char*)prefix->x,
      in->counter,
      tm.year,tm.month,tm.day,
      tm.hour,tm.min,tm.sec);
}
