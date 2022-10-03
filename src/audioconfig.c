#include "audioconfig.h"

#include <stdio.h>

int audioconfig_null_open(void *ud, const audioconfig* c) {
    (void)ud;
    (void)c;

    fprintf(stderr,"app error: audioconfig handler not set\n");
    return -1;
}

static int audioconfig_ignore_cb(void *ud, const audioconfig* c) {
    (void)ud;
    (void)c;

    return 0;
}

const audioconfig_handler audioconfig_ignore = { .open = audioconfig_ignore_cb, .userdata = NULL };

