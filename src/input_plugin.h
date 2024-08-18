#ifndef INPUT_PLUGIN_H
#define INPUT_PLUGIN_H

#include <stddef.h>

#include "strbuf.h"
#include "tag.h"

/* perform global-init/deinit type stuff on a plugin */
typedef int (*input_plugin_init)(void);
typedef void (*input_plugin_deinit)(void);

/* get the size of the userdata that needs to be allocated */
typedef size_t (*input_plugin_size)(void);

/* create (really - initialize) the plugin userdata */
typedef int (*input_plugin_create)(void* userdata);

/* configure a plugin with entries from the INI file */
typedef int (*input_plugin_config)(void* userdata, const strbuf* key, const strbuf* value);

/* try to open the input */
typedef int (*input_plugin_open)(void* userdata);

/* close a plugin */
typedef void (*input_plugin_close)(void* userdata);

/* read data from a plugin instance */
typedef size_t (*input_plugin_read)(void* userdata, void* dest, size_t len, const tag_handler*);

struct input_plugin {
    const strbuf* name;
    input_plugin_size size;
    input_plugin_init init;
    input_plugin_deinit deinit;
    input_plugin_create create;
    input_plugin_config config;
    input_plugin_open open;
    input_plugin_close close;
    input_plugin_read read;
};

typedef struct input_plugin input_plugin;

extern const input_plugin* input_plugin_list[];

#ifdef __cplusplus
extern "C" {
#endif

const input_plugin* input_plugin_get(const strbuf* name);

int input_plugin_global_init(void);
void input_plugin_global_deinit(void);

#ifdef __cplusplus
}
#endif

#endif
