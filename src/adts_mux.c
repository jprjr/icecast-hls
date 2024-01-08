#include "adts_mux.h"

#include <string.h>

#include "bitwriter.h"
#include "channels.h"

const adts_mux adts_mux_zero = ADTS_MUX_ZERO;

void adts_mux_init(adts_mux* s) {
    *s = adts_mux_zero;
}

int adts_mux_set_sample_rate(adts_mux* mux, unsigned int sample_rate) {
    switch(sample_rate) {
        case 96000: mux->sample_rate_index = 0x00; break;
        case 88200: mux->sample_rate_index = 0x01; break;
        case 64000: mux->sample_rate_index = 0x02; break;
        case 48000: mux->sample_rate_index = 0x03; break;
        case 44100: mux->sample_rate_index = 0x04; break;
        case 32000: mux->sample_rate_index = 0x05; break;
        case 24000: mux->sample_rate_index = 0x06; break;
        case 22050: mux->sample_rate_index = 0x07; break;
        case 16000: mux->sample_rate_index = 0x08; break;
        case 12000: mux->sample_rate_index = 0x09; break;
        case 11025: mux->sample_rate_index = 0x0A; break;
        case  8000: mux->sample_rate_index = 0x0B; break;
        case  7350: mux->sample_rate_index = 0x0C; break;
        default: return -1;
    }
    return 0;
}

int adts_mux_set_channel_layout(adts_mux* mux, uint64_t channel_layout) {
    switch(channel_layout) {
        case LAYOUT_MONO: mux->ch_index = 1; break;
        case LAYOUT_STEREO: mux->ch_index = 2; break;
        case LAYOUT_3_0: mux->ch_index = 3; break;
        case LAYOUT_4_0: mux->ch_index = 4; break;
        case LAYOUT_5_0: mux->ch_index = 5; break;
        case LAYOUT_5_1: mux->ch_index = 6; break;
        case LAYOUT_7_1: mux->ch_index = 7; break;
        default: return -1;
    }
    return 0;
}

int adts_mux_set_profile(adts_mux* mux, unsigned int profile) {
    if(profile == 0) return -1;
    if(profile > 4) return -1;

    mux->profile = profile - 1;
    return 0;
}

int adts_mux_encode_packet(adts_mux* mux, const void* data, size_t len) {
    bitwriter bw = BITWRITER_ZERO;

    if(len + 7 > ADTS_MAX_PACKET_SIZE) return -1;

    bw.buffer = mux->buffer;
    bw.len = 8191;

    /* syncword */
    bitwriter_add(&bw, 12, 0x0fff);

    /* version, 0 for mpeg-4, 1 for mpeg-2 */
    bitwriter_add(&bw, 1, mux->version);

    /* layer, always 0 */
    bitwriter_add(&bw, 2, 0);

    /* protection absence */
    bitwriter_add(&bw, 1, 1);

    /* profile (AOT - 1) */
    bitwriter_add(&bw, 2, mux->profile);

    /* frequency index */
    bitwriter_add(&bw, 4, mux->sample_rate_index);

    /* private bit */
    bitwriter_add(&bw, 1, 0);

    /* channel config */
    bitwriter_add(&bw, 3, mux->ch_index);

    /* originality */
    bitwriter_add(&bw, 1, mux->originality);

    /* home */
    bitwriter_add(&bw, 1, mux->home);

    /* copyright */
    bitwriter_add(&bw, 1, mux->copyright);

    /* copyright_start */
    bitwriter_add(&bw, 1, mux->copyright_start);

    /* frame length */
    bitwriter_add(&bw, 13, len + 7);

    /* buffer fullness (we just mark as vbr / NA) */
    bitwriter_add(&bw, 11, 0x7FF);

    /* number of AAC frames minues 1 */
    bitwriter_add(&bw, 2, 0);

    bitwriter_align(&bw);

    memcpy(&mux->buffer[bw.pos], data, len);
    mux->len = bw.pos + len;

    return 0;
}
