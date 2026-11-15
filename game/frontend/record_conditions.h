/* port (dep55): a Restomod record's conditions as one small two-line block, the same on the results and on Best Times: the weather and the
 * time of day ("STRM DUSK"), and under them, centred, the difficulty's word ("ORIGINAL"). An imported row (conditions not known) shows "-". */
#ifndef TNFS_RECORD_CONDITIONS_H
#define TNFS_RECORD_CONDITIONS_H

#include "game/sys/race_records.h"

#include <stdint.h>

/* how a screen writes the block: its words at num/den of the font's size from (x, y) (y their top), their width, the font's line at full size */
typedef struct ConditionsPen {
    void *ctx;
    void (*text)(void *ctx, const char *text, int32_t x, int32_t y, int32_t num, int32_t den);
    int32_t (*width)(void *ctx, const char *text, int32_t num, int32_t den);
    int32_t (*line_height)(void *ctx);
} ConditionsPen;

/* the block's two lines: "CLR DAY" / "ORIGINAL" (EASY, HARD, MIXED when rivals, police and traffic differ); "-" / "" when not known */
void record_conditions_lines(RecordMarks m, char *first, char *second, int size);
/* the block at num/den of the font's size: its right edge at `right`, the two lines centred on each other, the block centred on the
 * row of full-size text whose top is `row_y` (so it stays within that row's line) */
void record_conditions_draw(RecordMarks m, int32_t right, int32_t row_y, int32_t num, int32_t den, const ConditionsPen *pen);

#endif
