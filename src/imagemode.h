#ifndef IMAGEMODE_H
#define IMAGEMODE_H

/* so we can do things like:
 * mode = DESTINATION_IMAGE_MODE_KEEP | DESTINATION_IMAGE_MODE_INBAND
 * to say we want to keep images, and keep them in-band, versus
 * mode = DESTINATION_IMAGE_MODE_KEEP
 * which will keep them but move them out-of-band.
 * if KEEP is not set images are deleted, regardless of other flags */
enum image_mode {
    IMAGE_MODE_UNSET  = 0x00,
    IMAGE_MODE_KEEP   = 0x01,
    IMAGE_MODE_INBAND = 0x02,
};

typedef enum image_mode image_mode;

#endif
