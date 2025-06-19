#ifndef CHUNKER_INCLUDE_GUARD
#define CHUNKER_INCLUDE_GUARD

#include "lcm.h"
#include "utils.h"

typedef struct {
    uint64_t i;
    uint64_t max; /* stores how many segments to generate before resetting */
    uint64_t src_rate;
    uint64_t segment_samples;
    uint64_t frame_len;
} chunker;

static inline chunker chunker_create(uint64_t src_rate, uint64_t segment_samples, uint64_t frame_len) {
    chunker c = { 0, 0, src_rate, segment_samples, frame_len };
    if(c.frame_len != 0) {
        if(c.segment_samples % c.frame_len) {
            uint64_t l = lcm(src_rate,frame_len);
            uint64_t m = lcm(segment_samples,l);
            c.max = m / gcd(src_rate, segment_samples);
        }
    }
    return c;
}

static inline uint64_t chunker_next(chunker* c) {
    if(c->frame_len == 0) {
        return c->segment_samples;
    }

    if(c->segment_samples % c->frame_len == 0) {
        return c->segment_samples;
    }

    uint64_t ret = rescale_duration( c->i+1, c->frame_len, c->segment_samples) -
                   rescale_duration( c->i, c->frame_len, c->segment_samples);
    c->i++;

    if(c->i == c->max) c->i = 0;
    return ret * c->frame_len;
}

#endif
