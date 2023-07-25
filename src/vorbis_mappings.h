#ifndef VORBIS_MAPPINGS_H
#define VORBIS_MAPPINGS_H

/* mapping between vorbis/wave ordering */
/* index = wave index, value = mpeg index */
static const uint8_t vorbis_channel_layout[9][8] = {
    /* invalid */
    { 255 },

    /* mono */
    { 0 },

    /* stereo */
     { 0, 1 },

    /* FL, FC, FR ->
     * FL, FR, FC */
     {  0,  2,  1 },

    /* FL, FR, BL, BR ->
       FL, FR, BL, BR */
     {  0,  1,  2,  3 },

    /* FL, FC, FR, BL, BR ->
       FL, FR, FC, BL, BR */
     {  0,  2,  1,  3,  4 },

    /* FL, FC, FR,  BL, BR, LFE ->
     * FL, FR, FC, LFE, BL,  BR */
     {  0,  2,  1,   5,  3,   4},

    /* FL, FC, FR,  SL, SR, BC, LFE ->
     * FL, FR, FC, LFE, BC, SL,  SR */
     {  0,  2,  1,   6,  5,  3,   4 },

    /* FL, FC, FR,  SL, SR, BL, BR, LFE ->
     * FL, FR, FC, LFE, BL, BR, SL,  SR */
     {  0,  2,  1,   7,  5,  6,  3,   4 },
};

#endif


