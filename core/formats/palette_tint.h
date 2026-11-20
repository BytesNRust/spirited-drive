/* port: the tint of the time of day on a palette's colours (004471dc, 004473d8), the part that needs no race state: the car editor's night view shares it
 * (the race's RacePalette keeps the tint and the "ready" flag, game/race/race_palette.h). */
#ifndef TNFS_PALETTE_TINT_H
#define TNFS_PALETTE_TINT_H

#include <stdint.h>

/* 0050b774..0050b787: the tint 004471dc gives the race's colours (the time of day). */
typedef struct {
    int32_t brightness;   /* 0050b774: 16.16 */
    int32_t red;          /* 0050b778: x / 256, negatives become 0 */
    int32_t saturation;   /* 0050b77c: 16.16 toward the brightest channel, at most 1 */
    int32_t green;        /* 0050b780 */
    int32_t blue;         /* 0050b784 */
} PaletteTint;

/* The 6-bit colours `colours` (3 bytes each, `first` .. `end`) tinted in place. `t` is written to as the exe does: a negative scale becomes 0, a saturation above 1 becomes 1. */
void palette_tint_colours(PaletteTint *t, int32_t first, int32_t end, uint8_t *colours);

#endif
