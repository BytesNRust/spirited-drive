/* The palette helpers the menus and the race share (00463df0, 00463e08).
 * TODO(main: 00463df0, 00463e08): they belong to the shared graphics layer;
 * kept here until main has them. */
#ifndef TNFS_PALETTE_PUT_H
#define TNFS_PALETTE_PUT_H

#include <stdint.h>

enum { PALETTE_PUT_BYTES = 0x300 };

/* 00463df0: zeroes the 0x300 bytes. */
void palette_put_clear(uint8_t pal[PALETTE_PUT_BYTES]);

#endif
