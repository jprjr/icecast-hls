#ifndef FILTER_H
#define FILTER_H

#include "filter_plugin.h"
#include "frame.h"
#include "strbuf.h"
#include "audioconfig.h"

struct filter {
    void* userdata;
    const filter_plugin* plugin;
    frame_handler frame_handler; /* where to send output frames */
    audioconfig_handler audioconfig_handler; /* where to send audioconfig */
};

typedef struct filter filter;

#ifdef __cplusplus
extern "C" {
#endif

/* performs any needed global init/deinit */
int filter_global_init(void);
void filter_global_deinit(void);

void filter_init(filter*);
void filter_free(filter*);

int filter_create(filter*, const strbuf* plugin_name);
int filter_config(const filter*, const strbuf* name, const strbuf* value);

int filter_open(const filter*, const audioconfig*);

int filter_submit_frame(const filter*, const frame*);
int filter_flush(const filter*);

int filter_set_frame_handler(filter*, const frame_handler*);
int filter_set_audioconfig_handler(filter*, const audioconfig_handler*);


#ifdef __cplusplus
}
#endif

#endif
