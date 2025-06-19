#ifndef GCD_INCLUDE_GUARD
#define GCD_INCLUDE_GUARD

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

#endif
