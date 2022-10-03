#include "outputinfo.h"
#include <stdio.h>

int outputinfo_null_submit(void *ud, const outputinfo* i) {
    (void)ud;
    (void)i;
    fprintf(stderr,"app error: outputinfo handler not set\n");
    return -1;
}

static int outputinfo_ignore_cb(void *ud, const outputinfo* i) {
    (void)ud;
    (void)i;

    return 0;
}

const outputinfo_handler outputinfo_ignore = { .submit = outputinfo_ignore_cb, .userdata = NULL };
