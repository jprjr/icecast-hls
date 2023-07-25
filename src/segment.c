#include "segment.h"
#include <stdio.h>
#include <stdlib.h>

int segment_receiver_open_null(void* handle, const segment_source* source) {
    (void)handle;
    (void)source;
    fprintf(stderr,"[app error] segment_receiver open not set\n");
    abort();
    return -1;
}

int segment_receiver_submit_segment_null(void* handle, const segment* segment) {
    (void)handle;
    (void)segment;
    fprintf(stderr,"[app error] segment_receiver submit_segment not set\n");
    abort();
    return -1;
}

int segment_receiver_submit_tags_null(void* handle, const taglist* tags) {
    (void)handle;
    (void)tags;
    fprintf(stderr,"[app error] segment_receiver submit_tags not set\n");
    abort();
    return -1;
}

int segment_receiver_flush_null(void* handle) {
    (void)handle;
    fprintf(stderr,"[app error] segment_receiver flush not set\n");
    abort();
    return -1;
}

int segment_receiver_reset_null(void* handle) {
    (void)handle;
    fprintf(stderr,"[app error] segment_receiver reset not set\n");
    abort();
    return -1;
}

int segment_source_set_params_null(void* segment_producer, const segment_params* params) {
    (void)segment_producer;
    (void)params;
    fprintf(stderr,"[app error] segment_producer set_params not set\n");
    abort();
    return -1;
}

int segment_receiver_get_segment_info_null(const void* segment_receiver, const segment_source_info* info, segment_params* params) {
    (void)segment_receiver;
    (void)info;
    (void)params;
    fprintf(stderr,"[app error] segment_receiver get_segment_params not set\n");
    abort();
    return -1;
}

int segment_source_set_params_ignore(void* segment_producer, const segment_params* params) {
    (void)segment_producer;
    (void)params;
    return 0;
}

const segment_receiver segment_receiver_zero = SEGMENT_RECEIVER_ZERO;
const segment_source segment_source_zero = SEGMENT_SOURCE_ZERO;
const segment_params segment_params_zero = SEGMENT_PARAMS_ZERO;
const segment segment_zero = SEGMENT_ZERO;
