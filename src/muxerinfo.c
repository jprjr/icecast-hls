#include "muxerinfo.h"

#include <stdio.h>

int muxerinfo_null_submit(void *ud, const muxerinfo* i) {
    (void)ud;
    (void)i;

    fprintf(stderr,"app error: muxerinfo handler not set\n");
    return -1;
}

static int muxerinfo_ignore_cb(void *ud, const muxerinfo* i) {
    (void)ud;
    (void)i;

    return 0;
}

const muxerinfo_handler muxerinfo_ignore = {
    .submit = muxerinfo_ignore_cb,
    .userdata = NULL,
};
