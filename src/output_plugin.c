#include "output_plugin_stdout.h"
#include "output_plugin_file.h"
#include "output_plugin_folder.h"

#include <string.h>

const output_plugin* output_plugin_list[] = {
    &output_plugin_stdout,
    &output_plugin_file,
    &output_plugin_folder,
    NULL
};

const output_plugin* output_plugin_get(const strbuf* name) {
    const output_plugin** plug = output_plugin_list;
    while(*plug != NULL) {
        if( strbuf_equals(name, &((*plug)->name)) ) return *plug;
        plug++;
    }
    return NULL;
}

int output_plugin_global_init(void) {
    int r;
    const output_plugin** plug;

    plug = output_plugin_list;
    while(*plug != NULL) {
        if( (r = (*plug)->init()) != 0) return r;
        plug++;
    }

    return 0;
}

void output_plugin_global_deinit(void) {
    const output_plugin** plug = output_plugin_list;

    while(*plug != NULL) {
        (*plug)->deinit();
        plug++;
    }

    return;
}

