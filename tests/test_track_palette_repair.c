/* The track palette repair (port): CL1_R01.FAM's colour 2 is the pure green its mirror's FAM has as rock grey. */
#include "game/race/track_palette_repair.h"
#include "test.h"

#include <string.h>

static void colour(uint8_t pal[TRACK_PALETTE_BYTES], int i, int r, int g, int b)
{
    pal[i * 3] = (uint8_t)r, pal[i * 3 + 1] = (uint8_t)g, pal[i * 3 + 2] = (uint8_t)b;
}

int main(void)
{
    uint8_t pal[TRACK_PALETTE_BYTES], was[TRACK_PALETTE_BYTES];
    for (int i = 0; i < 256; i++) colour(pal, i, i / 4, (i * 3) / 16 + 1, i / 5);

    /* the stray green at colour 2 becomes the mirror FAM's rock grey (105, 101, 101 in 8 bits), nothing else moves */
    colour(pal, 2, 0, 63, 0);
    memcpy(was, pal, sizeof pal);
    CHECK_EQ_INT(track_palette_repair(pal), 1);
    CHECK_EQ_INT(pal[6], 26);
    CHECK_EQ_INT(pal[7], 25);
    CHECK_EQ_INT(pal[8], 25);
    CHECK(memcmp(pal, was, 6) == 0);
    CHECK(memcmp(pal + 9, was + 9, sizeof pal - 9) == 0);

    /* once repaired it is left alone */
    memcpy(was, pal, sizeof pal);
    CHECK_EQ_INT(track_palette_repair(pal), 0);
    CHECK(memcmp(pal, was, sizeof pal) == 0);

    /* only colour 2 and only that pure green: a near green, another colour's green, colour 255's key are theirs */
    colour(pal, 2, 0, 62, 0);
    memcpy(was, pal, sizeof pal);
    CHECK_EQ_INT(track_palette_repair(pal), 0);
    CHECK(memcmp(pal, was, sizeof pal) == 0);
    colour(pal, 2, 28, 21, 24);
    colour(pal, 3, 0, 63, 0);
    colour(pal, 255, 0, 63, 0);
    memcpy(was, pal, sizeof pal);
    CHECK_EQ_INT(track_palette_repair(pal), 0);
    CHECK(memcmp(pal, was, sizeof pal) == 0);
    colour(pal, 2, 1, 63, 0);
    memcpy(was, pal, sizeof pal);
    CHECK_EQ_INT(track_palette_repair(pal), 0);
    CHECK(memcmp(pal, was, sizeof pal) == 0);

    return TEST_REPORT();
}
