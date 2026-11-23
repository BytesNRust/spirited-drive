/* Copy to game repo for a car's Colors: the card's colours written into the car's row of TABLE in the game repo's core/formats/car_paint.c (the table the game's Color box and the replays' colour
 * numbers come from). The rewrite is by text and strict: the table's declaration and the car's row must be as the file has them (one `{ "STEM", COUNT, { r, g, b },` line, then the paints' list, each
 * `{ "Name", { r, g, b } }`, closed by `} },`), or it refuses in words and writes nothing. The car's own colour (the exe's, the 3 numbers after the count) and every other row stay as they are; the
 * paints are laid out the way the file lays them (wrapped under 125 columns), so the same colours rewrite to the same bytes. */
#ifndef TNFS_TRACK_EDITOR_CAR_TABLE_REWRITE_H
#define TNFS_TRACK_EDITOR_CAR_TABLE_REWRITE_H

#include "formats/mod_ini.h"

#include <stddef.h>

#include "formats/car_paint.h"

#ifdef __cplusplus
extern "C" {
#endif

enum { CAR_TABLE_MAX_PAINTS = CAR_PAINT_MAX, CAR_TABLE_LIST = 1200 };   /* the game's table: CAR_PAINT_MAX colours a car (8) */

/* The file's `src` (its text, `len` bytes) with `stem`'s row holding the card's `count` paints (1..CAR_TABLE_MAX_PAINTS; names without a quote or a backslash, printable ASCII).
 * Returns a malloc'd NUL-ended text (free it) when the row differs, with `*changed` 1; the same text copy with `*changed` 0 when the row already has these paints. `before` and `after` (CAR_TABLE_LIST bytes each,
 * when not NULL) are the row's paints as "Name #rrggbb, Name #rrggbb". NULL with `why` in words when the format is not the expected, the row is not there, or the paints cannot be written. */
char *car_table_rewrite(const char *src, size_t len, const char *stem, const ModPaint *paints, int count, int *changed, char *before, char *after, char *why, size_t why_cap);

#ifdef __cplusplus
}
#endif

#endif
