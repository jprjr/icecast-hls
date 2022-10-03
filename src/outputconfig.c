#include "outputconfig.h"

#include <stdio.h>

int outputconfig_null_submit(void *ud, const outputconfig* c) {
    (void)ud;
    (void)c;

    fprintf(stderr,"app error: outputconfig handler not set\n");
    return -1;
}

static int outputconfig_ignore_cb(void *ud, const outputconfig* c) {
    (void)ud;
    (void)c;

    return 0;
}

const outputconfig_handler outputconfig_ignore = { .submit = outputconfig_ignore_cb, .userdata = NULL };
