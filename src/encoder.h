#ifndef ENCODER_H
#define ENCODER_H

#include "encoder_plugin.h"
#include "codecs.h"
#include "ich_time.h"

struct encoder {
    void* userdata;
    const encoder_plugin* plugin;
    packet_receiver packet_receiver;
    size_t counter;
    ich_time ts;
};

typedef struct encoder encoder;

#ifdef __cplusplus
extern "C" {
#endif


/* performs any needed global init/deinit */
int encoder_global_init(void);
void encoder_global_deinit(void);

void encoder_init(encoder*);
void encoder_free(encoder*);

int encoder_create(encoder*, const strbuf* plugin_name);
int encoder_open(encoder*, const frame_source* source);

int encoder_config(const encoder*, const strbuf* name, const strbuf* value);

int encoder_submit_frame(encoder*, const frame*);
int encoder_flush(const encoder*);
void encoder_dump_counters(const encoder*, const strbuf*);

#ifdef __cplusplus
}
#endif

#endif
