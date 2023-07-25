#ifndef OUTPUT_H
#define OUTPUT_H

#include "output_plugin.h"
#include "ich_time.h"

struct output {
    void* userdata; /* plugin-specific userdata */
    const output_plugin* plugin; /* plugin currently in use */
    size_t counter;
    ich_time ts;
    int opened;
};

typedef struct output output;

#ifdef __cplusplus
extern "C" {
#endif

/* performs any needed global init/deinit for output plugins */
int output_global_init(void);
void output_global_deinit(void);

void output_init(output*);
void output_free(output*);

int output_create(output *, const strbuf* plugin_name);

int output_config(const output*, const strbuf* name, const strbuf* value);

int output_open(output*, const segment_source* source);

int output_set_time(const output*, const ich_time*);

int output_submit_segment(output*, const segment*);
int output_submit_tags(const output*, const taglist*);
int output_submit_picture(const output*, const picture*, picture*);
int output_flush(const output*);

/* doesn't really reset state, instead it signals a discontinuity */
int output_reset(const output*);

int output_get_segment_info(const output*, const segment_source_info* info, segment_params* params);

void output_dump_counters(const output*, const strbuf*);

#ifdef __cplusplus
}
#endif

#endif
