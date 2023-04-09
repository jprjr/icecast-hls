#ifndef ICH_AVPACKET_UTILS_H
#define ICH_AVPACKET_UTILS_H

/* provides an object that can convert our internal frames into AVFrames, and vice-versa */

#include "packet.h"

#include <libavcodec/avcodec.h>

#ifdef __cplusplus
extern "C" {
#endif

int packet_to_avpacket(AVPacket* out, const packet* in);

/* note: this does not actually allocate memory in the packet
 * since nobody will actually write to it anyway, it just
 * creates a reference */
int avpacket_to_packet(packet* out, const AVPacket* in);

#ifdef __cplusplus
}
#endif

#endif

