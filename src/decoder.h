#ifndef DECODER_H
#define DECODER_H

#include "decoder_plugin.h"
#include "ich_time.h"

struct decoder {
    void* userdata;
    const decoder_plugin* plugin;
    tag_handler tag_handler;
    frame_receiver frame_receiver;
    size_t counter;
    ich_time ts;
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
int decoder_open(decoder* dec, input* in);

/* runs the decoder plugin for 1 frame of audio, submitting any
 * metadata it finds as it goes, returns 1 on EOF, -1 on error */
int decoder_run(decoder* dec);

void decoder_dump_counters(const decoder* dec, const strbuf* prefix);

#ifdef __cplusplus
}
#endif

#endif
