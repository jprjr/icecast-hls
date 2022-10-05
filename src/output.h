#ifndef OUTPUT_H
#define OUTPUT_H

#include "output_plugin.h"

struct output {
    void* userdata; /* plugin-specific userdata */
    const output_plugin* plugin; /* plugin currently in use */
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

int output_open(const output*, const segment_source* source);

int output_set_time(const output*, const ich_time*);

/* called during the muxer's setup, specifies stuff like the file
 * extensions, mimetypes, and time_base */
int output_submit_segment(const output*, const segment*);
int output_submit_picture(const output*, const picture*, picture*);
int output_flush(const output*);


#ifdef __cplusplus
}
#endif

#endif
