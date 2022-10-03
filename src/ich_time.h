#ifndef ICH_TIME_H
#define ICH_TIME_H

#include <stdint.h>

/* calling this "ich_time" ("IC"cast "H"ls) so as
 * not to conflict with time.h
 */

struct ich_time {
    int64_t seconds;
    int64_t nanoseconds;
};

typedef struct ich_time ich_time;

struct ich_tm {
    uint32_t year;
    uint8_t month; /* 1 = jan */
    uint8_t   day;
    uint8_t  hour;
    uint8_t   min;
    uint8_t   sec;
    uint16_t mill;
};

typedef struct ich_tm ich_tm;


struct ich_frac {
    int64_t num; /* numerator */
    int64_t den; /* denominator, should always be positive */
};

typedef struct ich_frac ich_frac;

#ifdef __cplusplus
extern "C" {
#endif

/* get the current time and store it in ich_time */
int ich_time_now(ich_time*);

void ich_time_add(ich_time*, const ich_time*);

/* add a second specified in another time base (basically samples / samplerate) */
void ich_time_add_frac(ich_time*, const ich_frac*);

void ich_time_to_tm(ich_tm*, const ich_time*);

#ifdef __cplusplus
}
#endif

#endif
