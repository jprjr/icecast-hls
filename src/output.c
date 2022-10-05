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

int output_open(const output* out, const segment_source* source) {
    if(out->plugin == NULL || out->userdata == NULL) {
        fprintf(stderr,"[output] plugin not selected\n");
        return -1;
    }
    return out->plugin->open(out->userdata, source);
}

int output_config(const output* out, const strbuf* name, const strbuf* value) {
    return out->plugin->config(out->userdata,name,value);
}

int output_set_time(const output* out, const ich_time* now) {
    return out->plugin->set_time(out->userdata,now);
}

int output_submit_segment(const output* out, const segment* seg) {
    return out->plugin->submit_segment(out->userdata,seg);
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
