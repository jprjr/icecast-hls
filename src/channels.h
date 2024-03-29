#ifndef CHANNELS_H
#define CHANNELS_H

#define CHANNEL_FRONT_LEFT               0x1ULL
#define CHANNEL_FRONT_RIGHT              0x2ULL
#define CHANNEL_FRONT_CENTER             0x4ULL
#define CHANNEL_LOW_FREQUENCY            0x8ULL
#define CHANNEL_BACK_LEFT               0x10ULL
#define CHANNEL_BACK_RIGHT              0x20ULL
#define CHANNEL_FRONT_LEFT_OF_CENTER    0x40ULL
#define CHANNEL_FRONT_RIGHT_OF_CENTER   0x80ULL
#define CHANNEL_BACK_CENTER            0x100ULL
#define CHANNEL_SIDE_LEFT              0x200ULL
#define CHANNEL_SIDE_RIGHT             0x400ULL
#define CHANNEL_TOP_CENTER             0x800ULL
#define CHANNEL_TOP_FRONT_LEFT        0x1000ULL
#define CHANNEL_TOP_FRONT_CENTER      0x2000ULL
#define CHANNEL_TOP_FRONT_RIGHT       0x4000ULL
#define CHANNEL_TOP_BACK_LEFT         0x8000ULL
#define CHANNEL_TOP_BACK_CENTER      0x10000ULL
#define CHANNEL_TOP_BACK_RIGHT       0x20000ULL

#define CHANNEL_IS_MONO(a) (a == CHANNEL_FRONT_LEFT || a == CHANNEL_FRONT_CENTER)

#define LAYOUT_MONO CHANNEL_FRONT_CENTER
#define LAYOUT_STEREO (CHANNEL_FRONT_LEFT | CHANNEL_FRONT_RIGHT)
#define LAYOUT_3_0 (CHANNEL_FRONT_LEFT | CHANNEL_FRONT_RIGHT | CHANNEL_FRONT_CENTER)
#define LAYOUT_4_0 (CHANNEL_FRONT_LEFT | CHANNEL_FRONT_RIGHT | CHANNEL_FRONT_CENTER | CHANNEL_BACK_CENTER)
#define LAYOUT_QUAD (CHANNEL_FRONT_LEFT | CHANNEL_FRONT_RIGHT | CHANNEL_BACK_LEFT | CHANNEL_BACK_RIGHT)
#define LAYOUT_5_0 (CHANNEL_FRONT_LEFT | CHANNEL_FRONT_RIGHT | CHANNEL_FRONT_CENTER | CHANNEL_BACK_LEFT | CHANNEL_BACK_RIGHT)
#define LAYOUT_5_1 (CHANNEL_FRONT_LEFT | CHANNEL_FRONT_RIGHT | CHANNEL_FRONT_CENTER | CHANNEL_LOW_FREQUENCY | CHANNEL_BACK_LEFT | CHANNEL_BACK_RIGHT)
#define LAYOUT_6_1 (CHANNEL_FRONT_LEFT | CHANNEL_FRONT_RIGHT | CHANNEL_FRONT_CENTER | CHANNEL_LOW_FREQUENCY | CHANNEL_BACK_CENTER | CHANNEL_SIDE_LEFT | CHANNEL_SIDE_RIGHT)
#define LAYOUT_7_1 (CHANNEL_FRONT_LEFT | CHANNEL_FRONT_RIGHT | CHANNEL_FRONT_CENTER | CHANNEL_LOW_FREQUENCY | CHANNEL_BACK_LEFT | CHANNEL_BACK_RIGHT | CHANNEL_SIDE_LEFT | CHANNEL_SIDE_RIGHT)

static inline
size_t channel_count(uint64_t channel_layout) {
    size_t channels;

    for(channels = 0; channel_layout; channel_layout >>= 1) {
        channels += channel_layout & 1;
    }

    return channels;
}

#endif
