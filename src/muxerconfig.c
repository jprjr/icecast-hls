#include "muxerconfig.h"

#include <stdio.h>

int muxerconfig_null_submit(void *ud, const muxerconfig* c) {
    (void)ud;
    (void)c;

    fprintf(stderr,"app error: muxerconfig handler not set\n");
    return -1;
}

int muxerconfig_null_dsi(void *ud, const membuf* dsi) {
    (void)ud;
    (void)dsi;

    fprintf(stderr,"app error: muxerconfig dsi handler not set\n");
    return -1;
}


static int muxerconfig_ignore_cb(void *ud, const muxerconfig* c) {
    (void)ud;
    (void)c;

    return 0;
}

static int muxerconfig_dsi_ignore_cb(void *ud, const membuf* dsi) {
    (void)ud;
    (void)dsi;

    return 0;
}


const muxerconfig_handler muxerconfig_ignore = {
    .submit = muxerconfig_ignore_cb,
    .submit_dsi = muxerconfig_dsi_ignore_cb,
    .userdata = NULL,
};
