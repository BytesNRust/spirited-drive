/* The rear-view mirror's sky (0044bddc, from 0044c098 while the mirror's
 * picture is made): a solid colour under some road properties, else the
 * 'horz' strip tiled across the mirror between two solid bands. */
#ifndef TNFS_MIRROR_SKY_H
#define TNFS_MIRROR_SKY_H

#include <stdint.h>

typedef struct {
    void *ctx;
    uint8_t (*property)(void *ctx, int32_t node);   /* the road node's +7 (00505038, masked by 00504208) */
    void (*fill)(void *ctx, uint32_t colour);       /* 00482980: the clip rectangle filled */
    void (*rect)(void *ctx, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t colour);  /* 004829b0 */
    /* 00486720: the shape mapped onto the quad x0, y0 .. x3, y3. */
    void (*quad)(void *ctx, const uint8_t *shape, const int32_t points[8]);
} MirrorSkyOps;

typedef struct {
    int32_t node;               /* chase +0x38 (00511fe0) */
    int32_t pitch, yaw;         /* chase +0x40, +0x44 */
    int32_t high;               /* 00512018 */
    int32_t view_h;             /* *(00511fe8) +0x18's +0x20, halved for the middle */
    int32_t view_top;           /* *(00511fe8) +0x1c's +0xc */
    uint32_t enclosed_colour;   /* 0050b7c0: road properties 4, 7, 9 */
    uint32_t property8_colour;  /* 0050b79c: road property 8 */
    int16_t track;              /* 00524e85 (word): 5 puts the strip at -17 */
    int32_t flip;               /* 004fcd74: every other tile mirrored */
    int32_t width, height;      /* 00512034, 00512038: the mirror's size */
    uint8_t *bank;              /* 0051203c: holds 'horz' */
} MirrorSkyInputs;

/* 0044bddc: the node 3 behind the camera's: property 4 / 7 / 9 fills with
 * 0050b7c0, 8 with 0050b79c. Else 'horz' scaled to 256 pixels across (128 at
 * low resolution), its horizon row (+0xa) on the view's middle moved by the
 * pitch, tiled right to left from the yaw's offset; the rows above it in its
 * first pixel, below in its last. Missing 'horz' (the original aborts) or a
 * zero width (a divide fault) draws nothing. */
void mirror_sky(const MirrorSkyOps *ops, const MirrorSkyInputs *in);

#endif
