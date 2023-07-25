#ifndef MPEG_MAPPINGS_H

/* mapping between mpeg/wave channel ordering */
/* index = wave index, value = mpeg index */
static const uint8_t mpeg_channel_layout[9][8] = {
    /* invalid */
    { 255 },

    /* mono */
    { 0 },

    /* stereo */
    { 0, 1 },

    /* FC, FL, FR ->
     * FL, FR, FC */
     {  1,  2,  0 },

    /* FC, FL, FR, BC ->
     * FL, FR, FC, BC */
    {   1,  2,  0,  3 },

    /* FC, FL, FR, BL, BR ->
     * FL, FR, FC, BL, BR */
    {   1,  2,  0,  3,  4 },

    /* FC, FL, FR,  BL, BR, LFE ->
     * FL, FR, FC, LFE, BL,  BR */
    {   1,  2,  0,   5,  3,   4 },

    /* 7 channels, undefined */
    { 0, 0, 0, 0, 0, 0, 0 },

    /* FC, FL, FR,  SL, SR, BL, BR, LFE ->
     * FL, FR, FC, LFE, BL, BR, SL,  SR */
    {   1,  2,  0,   7,  5,  6,  3,   4 },
};

#endif
