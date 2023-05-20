#ifndef FILTER_H
#define FILTER_H

#include "filter_plugin.h"
#include "frame.h"
#include "strbuf.h"
#include "ich_time.h"

struct filter {
    void* userdata;
    const filter_plugin* plugin;
    frame_receiver frame_receiver; /* where to send output frames */
    size_t counter;
    ich_time ts;
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

int filter_open(filter*, const frame_source*);

int filter_submit_frame(filter*, const frame*);
int filter_flush(const filter*);
void filter_dump_counters(const filter* filter, const strbuf* prefix);

#ifdef __cplusplus
}
#endif

#endif
