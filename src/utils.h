#ifndef UTILS_INCLUDE_GUARD
#define UTILS_INCLUDE_GUARD

#include <stdint.h>

static inline uint64_t gcd(uint64_t a, uint64_t b) {
    uint64_t tmp;
    while(a) {
        tmp = a;
        a = b % a;
        b = tmp;
    }
    return b;
}

static inline uint64_t lcm(uint64_t a, uint64_t b) {
    return a / gcd(a, b) * b;
}

static inline uint64_t rescale_duration(uint64_t duration, uint64_t src_rate, uint64_t dest_rate) {
    return duration * dest_rate / src_rate;
}

typedef struct {
    uint64_t i;
    uint64_t max;
    uint64_t src_rate;
    uint64_t segment_samples;
    uint64_t frame_len;
} chunker;

static inline chunker chunker_create(uint64_t src_rate, uint64_t segment_samples, uint64_t frame_len) {
    uint64_t l = lcm(src_rate,frame_len);
    uint64_t m = lcm(segment_samples,l);
    chunker c = { 0, m / gcd(src_rate,segment_samples), src_rate, segment_samples, frame_len };
    return c;
}

static inline uint64_t chunker_next(chunker* c) {
    if(c->segment_samples % c->frame_len == 0) {
        return c->segment_samples / c->frame_len;
    }
    uint64_t ret = rescale_duration( c->i+1, c->frame_len, c->segment_samples) -
                   rescale_duration( c->i, c->frame_len, c->segment_samples);
    c->i++;
    if(c->i == c->max) c->i = 0;
    return ret;
}

#endif
