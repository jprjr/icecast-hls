#include "ich_time.h"

#define NANOPERMICRO  1000
#define MICROPERMILLI 1000
#define MILLIPERSEC   1000
#define NANOPERMILLI  (NANOPERMICRO * MICROPERMILLI)
#define NANOPERSEC    (NANOPERMILLI * MILLIPERSEC)
#define SECPERMIN  60
#define MINPERHOUR 60
#define HOURPERDAY 24
#define MINPERDAY  (MINPERHOUR * HOURPERDAY)
#define SECPERHOUR (SECPERMIN * MINPERHOUR)
#define SECPERDAY  (SECPERHOUR * HOURPERDAY)
#define NANOPERDAY (NANOPERSEC * SECPERDAY)

#if defined(_WIN32) || defined(_WIN64) || defined(_MSC_VER)
#define ICH_TIME_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#include <windows.h>
#else
#define ICH_TIME_UNIX
#include <time.h>
#endif

static const int year_days[2] = { 365, 366 };

static const int mon_days[2][12] = {
    {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
    {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
};

/* get the current time and store it in ich_time */
int ich_time_now(ich_time* t) {
#if defined(ICH_TIME_WINDOWS)
    FILETIME tm;
    ULARGE_INTEGER i;
    GetSystemTimePreciseAsFileTime(&tm);
    i.LowPart = tm.dwLowDateTime;
    i.HighPart = tm.dwHighDateTime;
    t->seconds =      i.QuadPart / 10000000ULL - 11644473600ULL;
    t->nanoseconds = (i.QuadPart % 10000000ULL) * 100;
    return 0;
#elif defined(ICH_TIME_UNIX)
    struct timespec tv;
    clock_gettime(CLOCK_REALTIME,&tv);
    t->seconds = tv.tv_sec;
    t->nanoseconds = tv.tv_nsec;
    return 0;
#endif
}

/* I'm just going to assume we're only ever adding positive time values */
void ich_time_add(ich_time* t, const ich_time* a) {
    t->nanoseconds += a->nanoseconds;
    t->seconds += a->seconds;

    while(t->nanoseconds >= NANOPERSEC) {
        t->seconds++;
        t->nanoseconds -= NANOPERSEC;
    }
}

/* add a second specified in another time base (basically samples / samplerate) */
void ich_time_add_frac(ich_time* t, const ich_frac* f) {
    /*
     * have: samples
     * need: nanoseconds
     * know: samples/seconds, nanoseconds/seconds
     *
     *               seconds    nanoseconds
     *     samples * -------- * ------------ = nanoseconds
     *               samples    seconds
     *
     */
    t->seconds += (f->num / f->den);
    t->nanoseconds += (f->num % f->den) * NANOPERSEC / f->den;

    while(t->nanoseconds >= NANOPERSEC) {
        t->seconds++;
        t->nanoseconds -= NANOPERSEC;
    }
}

static inline int isleapyear(int64_t y) {
    return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0);
}

void ich_time_to_tm(ich_tm* tm, const ich_time* t) {
    int64_t hour, min, sec;
    int64_t year, month;
    int64_t days, rem;
    int l;

    /* break out into days and seconds */
    days = t->seconds / SECPERDAY;
    rem  = t->seconds % SECPERDAY;

    /* the easy part, converting remaining seconds
     * in to hour, minute, second */
    hour = rem / SECPERHOUR;
    rem  = rem % SECPERHOUR;
    min  = rem / SECPERMIN;
    sec  = rem % SECPERMIN;

    year = 1970;
    l = isleapyear(year);

    while(days > year_days[l]) {
        days -= year_days[l];
        l = isleapyear(++year);
    }

    month = 0;
    while(days >= mon_days[l][month]) {
        days -= mon_days[l][month++];
    }

    tm->year  = year;
    tm->month = month+1;
    tm->day   = days+1;
    tm->hour  = hour;
    tm->min   = min;
    tm->sec   = sec;
    tm->mill  = t->nanoseconds / NANOPERMILLI;
}

int ich_time_cmp(const ich_time* a, const ich_time* b) {
    if(a->seconds == b->seconds) {
        if(a->nanoseconds == b->nanoseconds) return 0;
        return a->nanoseconds - b->nanoseconds;
    }
    return a->seconds - b->seconds;
}

void ich_time_sub(ich_time* res, const ich_time* a, const ich_time* b) {
    ich_time x, y;
    int nsec;

    x = *a;
    y = *b;

    if(x.nanoseconds < y.nanoseconds) {
        nsec = ((y.nanoseconds - x.nanoseconds) / NANOPERSEC) + 1;
        y.nanoseconds -= NANOPERSEC * nsec;
        y.seconds += nsec;
    }
    if(x.nanoseconds - y.nanoseconds > NANOPERSEC) {
        nsec = (y.nanoseconds - x.nanoseconds) / NANOPERSEC;
        y.nanoseconds += NANOPERSEC * nsec;
        y.seconds -= nsec;
    }
    res->seconds = x.seconds - y.seconds;
    res->nanoseconds = x.nanoseconds - y.nanoseconds;
}
