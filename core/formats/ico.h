/* Windows .ICO: the CD's NFSICONN.ICO (one 32 x 32 16-colour image) as RGBA for the window's icon.
 *
 *   u16 0, u16 type (1), u16 count; then count entries of 16 bytes:
 *     u8 width (0 = 256), u8 height, u8 colours, u8 0, u16 planes, u16 bpp, u32 size, u32 offset
 *   each image a BITMAPINFOHEADER (biHeight twice the image's: the XOR rows, then the AND mask's), the palette for
 *   1, 4 and 8 bits a pixel, the XOR rows bottom up, each padded to 4 bytes, then the 1-bit AND rows the same way.
 * PNG images inside an ICO are not read. */
#ifndef TNFS_ICO_H
#define TNFS_ICO_H

#include <stddef.h>
#include <stdint.h>

enum { ICO_MAX_SIDE = 256 };

/* The biggest BMP image of the file into `rgba` (w x h x 4 bytes, top row first; `capacity` bytes), its size in
 * *w and *h. 1 on success, 0 when the file is not an icon this reads or the image does not fit. */
int ico_decode(const uint8_t *data, size_t size, uint8_t *rgba, size_t capacity, int *w, int *h);

#endif
