#ifndef ICH_AVUTIL_COMPAT
#define ICH_AVUTIL_COMPAT

#include <libavcodec/avutil.h>
#include "ffmpeg-versions.h"

#if !(ICH_AVUTIL_CHANNEL_LAYOUT)

#include "channels.h"

/* minimal struct that only supports native ordering */
enum AVChannelOrder {
    AV_CHANNEL_ORDER_UNSPEC,
    AV_CHANNEL_ORDER_NATIVE,
};

typedef struct AVChannelLayout {
    enum AVChannelOrder order;
    int nb_channels;
    union {
        uint64_t mask;
        uint64_t dummy;
    } u;
} AVChannelLayout;

static inline int av_channel_layout_from_mask(AVChannelLayout* layout, uint64_t mask) {
    layout->order = AV_CHANNEL_ORDER_NATIVE;
    layout->nb_channels = channel_count(mask);
    layout->u->mask = mask;
    return 0;
}


#endif

#endif
