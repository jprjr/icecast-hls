#ifndef LCM_INCLUDE_GUARD
#define LCM_INCLUDE_GUARD

#include "gcd.h"

static inline uint64_t lcm(uint64_t a, uint64_t b) {
    return a / gcd(a, b) * b;
}

#endif
