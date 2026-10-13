/* A shape mirrored left to right in place (0048f7a0; the rear-view
 * mirror's picture, from 0044c098). */
#ifndef TNFS_SHAPE_FLIP_H
#define TNFS_SHAPE_FLIP_H

#include <stdint.h>

/* 0048f7a0: each of the shape's rows (+0x10 on, 0049f530's pitch apart)
 * reversed pixel by pixel, the pixel size from its type's bits (004c7c50);
 * then its x hotspot (word +8) becomes width - it. Returns 0 and does
 * nothing for a compressed type (bit 7) or under 8 bits a pixel, where the
 * original aborts (00477a40). */
int shape_flip(uint8_t *shape);

#endif
