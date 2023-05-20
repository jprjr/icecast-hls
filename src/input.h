#ifndef INPUT_H
#define INPUT_H

#include "input_plugin.h"
#include "strbuf.h"
#include "ich_time.h"

struct input {
    void* userdata; /* plugin-specific userdata */
    const input_plugin* plugin; /* plugin currently in use */
    tag_handler tag_handler;
    size_t counter;
    ich_time ts;
};

typedef struct input input;

#ifdef __cplusplus
extern "C" {
#endif

/* performs any needed global init/deinit for input plugins */
int input_global_init(void);
void input_global_deinit(void);

void input_init(input*);
void input_free(input*);

int input_create(input *in, const strbuf* plugin_name);

int input_config(const input* in, const strbuf* name, const strbuf* value);

/* try to open the input */
int input_open(input* in);

size_t input_read(input* in, void* dest, size_t len);

void input_dump_counters(const input* in, const strbuf* prefix);

#ifdef __cplusplus
}
#endif

#endif
