#ifndef DECODER_H
#define DECODER_H

#include "decoder_plugin.h"

struct decoder {
    void* userdata;
    const decoder_plugin* plugin;
    tag_handler tag_handler;
    frame_handler frame_handler;
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
int decoder_open(const decoder* dec, const input* in, const audioconfig_handler* aconfig);

int decoder_set_frame_handler(decoder*, const frame_handler*);
int decoder_set_tag_handler(decoder*, const tag_handler*);

/* runs the decoder, keeps decoding frames and submitting them to the
 * frame handler until EOF/error */
int decoder_decode(const decoder* dec);

#ifdef __cplusplus
}
#endif

#endif
