#include "encoderinfo.h"
#include <stdio.h>

static int encoderinfo_submit_ignore(void* userdata, const encoderinfo* i) {
    (void)userdata;
    (void)i;
    return 0;
}

const encoderinfo_handler encoderinfo_ignore = { .submit = encoderinfo_submit_ignore, .userdata = NULL };

int encoderinfo_null_submit(void* userdata, const encoderinfo* i) {
    (void)userdata;
    (void)i;
    fprintf(stderr,"app error: encoderinfo handler not set\n");
    return -1;
}
