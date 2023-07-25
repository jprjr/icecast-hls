#ifndef DECODER_H
#define DECODER_H

#include "decoder_plugin.h"
#include "ich_time.h"

struct decoder {
    void* userdata;
    const decoder_plugin* plugin;
    frame_receiver frame_receiver;
    frame_source frame_source;
    size_t counter;
    ich_time ts;
    frame frame;
    uint64_t pts;
};

typedef struct decoder decoder;

#ifdef __cplusplus
extern "C" {
#endif

/* performs any needed global init/deinit for decoder plugins */
int decoder_global_init(void);
void decoder_global_deinit(void);

/* performs any startup initialization, returns 0 on success */
void decoder_init(decoder* dec);

/* close out an decoder, free any resources */
void decoder_free(decoder* dec);

int decoder_create(decoder *dec, const strbuf* plugin_name);

int decoder_config(const decoder* dec, const strbuf* name, const strbuf* value);

/* try to open the decoder */
int decoder_open(decoder* dec, const packet_source* src);

int decoder_submit_packet(decoder* dec, const packet* p);

/* flush out any remaining frames */
int decoder_flush(decoder* dec);

/* reset the state to start decoding a new stream */
int decoder_reset(const decoder* dec);

void decoder_dump_counters(const decoder* dec, const strbuf* prefix);

#ifdef __cplusplus
}
#endif

#endif
