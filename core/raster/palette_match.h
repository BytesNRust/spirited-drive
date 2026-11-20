/* RGB to palette index for an 8-bit surface (NFS_WIN.EXE 00473140). The game
 * passes a packed colour whose bits 0-7, 8-15 and 16-23 are the first, second
 * and third channel (0x00BBGGRR); bits 24-31 are ignored.
 *
 * The original reads the surface type from 004c78cd and branches on it; this
 * is its 8-bit path (the types whose entry in 004c7c50 is 4 or 8), with the
 * state it reads given as arguments.
 * port: 00473140's 15-bit, 16-bit, 24-bit and 32-bit surface paths, which
 * repack the channels into the pixel format, are not ported: the game always runs at 640 x 480 x 8 (00477980). */
#ifndef TNFS_PALETTE_MATCH_H
#define TNFS_PALETTE_MATCH_H

#include <stdint.h>

enum {
    PALETTE_MATCH_INVERSE_SIZE = 32768,  /* 32 x 32 x 32 entries */
    PALETTE_MATCH_ENTRIES = 256,
    PALETTE_MATCH_ENTRY_BYTES = 4
};

/* The state 00473140 reads. */
typedef struct {
    const uint8_t *inverse;  /* 004c5ff4: inverse colour table, or NULL for the search */
    const uint8_t *palette;  /* 0052f178: 256 entries of 4 bytes, the first 3 are the channels */
    const uint8_t *enabled;  /* 004c5ff8: 256 flags, non-zero when the entry may be chosen */
} PaletteMatch;

/* With the inverse table: its entry for the top 5 bits of each channel,
 * index (c2 >> 3) << 10 | (c1 >> 3) << 5 | c0 >> 3. */
uint8_t palette_match_inverse(const uint8_t inverse[PALETTE_MATCH_INVERSE_SIZE], uint32_t rgb);

/* Without it: the enabled entry with the least squared distance over the three
 * channels. An entry must beat the best so far strictly, starting from 99999,
 * so the lowest index wins ties and a colour farther than that from every
 * enabled entry (or with none enabled) gives 0. */
int palette_match_nearest(const uint8_t palette[PALETTE_MATCH_ENTRIES * PALETTE_MATCH_ENTRY_BYTES],
                          const uint8_t enabled[PALETTE_MATCH_ENTRIES], uint32_t rgb);

/* 00473140 for an 8-bit surface: the inverse table when m->inverse is set,
 * else the search. Returns 0 for an unusable argument. */
int palette_match_rgb(const PaletteMatch *m, uint32_t rgb);

#endif
