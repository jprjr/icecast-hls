#ifndef UTILS_INCLUDE_GUARD
#define UTILS_INCLUDE_GUARD

#include <stdint.h>

static inline uint64_t rescale_duration(uint64_t duration, uint64_t src_rate, uint64_t dest_rate) {
    //return (duration / src_rate * dest_rate) + ((duration % src_rate * dest_rate + (src_rate/2)) / src_rate);
    return (duration / src_rate * dest_rate) + ((duration % src_rate * dest_rate ) / src_rate);
    /* return duration * dest_rate / src_rate; */
}

#endif
