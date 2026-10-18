/* The palette helpers the menus and the race share (00463df0, 00463e08), kept with the menus that use most of
 * them. */
#ifndef TNFS_PALETTE_PUT_H
#define TNFS_PALETTE_PUT_H

#include <stdint.h>

enum { PALETTE_PUT_BYTES = 0x300 };

/* 00463df0: zeroes the 0x300 bytes. */
void palette_put_clear(uint8_t pal[PALETTE_PUT_BYTES]);

/* 00463e08 (watcall pal, first, count, src, then the mode on the stack):
 * count colours (3 bytes each) from src into pal from colour first, as they
 * are when the mode's low word is 0x24 and shifted left 2 (6 to 8 bits)
 * otherwise, then the whole of pal to screen (00520dd8). first and count
 * are shorts; pal + 3 x first .. + 3 x count must lie in pal. */
void palette_put(uint8_t pal[PALETTE_PUT_BYTES], int first, int count, const uint8_t *src, int mode,
                 uint8_t screen[PALETTE_PUT_BYTES]);

#endif
