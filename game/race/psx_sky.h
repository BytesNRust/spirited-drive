/* port: the PS1 look's sky (re/notes/psx-look.md). At dawn and dusk the PS1 has its own sky per track: Z<TRACK><A|C>.INF, the
 * text of the PC's <TRACK>INFO with a dithering flag and the blend's ratio where the ring type was and no time-of-day line (the
 * gradient's two colours, the ring's offset and height), and Z<TRACK><A|C>.PSH, a SHPP directory whose horizon tiles hrz0..hrz7
 * are 8bpp images each through its own 256-entry CLUT, side by side as the panorama. Read at the race's start from the user's own
 * files; nothing of them is in the repo. */
#ifndef TNFS_PSX_SKY_H
#define TNFS_PSX_SKY_H

#include <stddef.h>
#include <stdint.h>

enum { PSX_SKY_TILES = 8, PSX_SKY_GEOMETRY = 8 };

typedef struct {
    float geometry[PSX_SKY_GEOMETRY];   /* vert offset, z offset, height and radius, near then far each (the INFO's order) */
    int top[3], bottom[3];              /* the gradient's colours, 8 bits a channel */
    int dither, blend;                  /* "dithering flag", "ratio of the blend" */
    int ring_y, ring_height;            /* "ring y offset", "ring height" */
    int fields;                         /* how many values the file gave, of 14 (a colour is one) */
} PsxSkyInf;

/* The INF text: its values are the lines that do not start with '#' (blank lines are skipped), the PC INFO's order. 0 when the
 * eight decimals and the two colours are not all there; later values missing stay 0. */
int psx_sky_inf_parse(const uint8_t *file, size_t size, PsxSkyInf *out);

typedef struct {
    int width, height;   /* the panorama: the tiles side by side, hrz0 first */
    uint8_t *rgb;        /* width x height x 3, owned */
} PsxSky;

/* The panorama of a PSH's hrz0..hrz7, which must all be there as 8bpp images of one size. 0 (and rgb NULL) otherwise. */
int psx_sky_panorama(const uint8_t *psh, size_t size, PsxSky *out);
void psx_sky_free(PsxSky *sky);

/* An order-sensitive sum of the bytes, for comparing two readings of a file (31-multiplier hash, each byte plus one). */
uint32_t psx_sky_checksum(const uint8_t *bytes, size_t count);

#endif
