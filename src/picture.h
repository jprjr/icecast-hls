#ifndef PICTURE_H
#define PICTURE_H

#include "strbuf.h"
#include "membuf.h"

struct picture {
    strbuf mime;
    strbuf desc;
    strbuf data;
};

typedef struct picture picture;

#define PICTURE_ZERO { .mime = STRBUF_ZERO, .desc = STRBUF_ZERO, .data = STRBUF_ZERO }

/* the picture handler callback is called by muxers when they receive an APIC
 * FRAME - this will call a function in the respective output plugin, that
 * will (maybe) write out the image file to disk or whatever, and update
 * the output picture with new data to use instead. This could (in theory) be
 * a resized picture or, more likely, a link to a picture */

/* TODO maybe I actually could do that resizing idea? */

typedef int (*picture_handler_callback)(void*, const picture*,picture*);

struct picture_handler {
    picture_handler_callback cb;
    void* userdata;
};

typedef struct picture_handler picture_handler;

#endif
