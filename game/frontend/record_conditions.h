/* port (dep55): a Restomod record's conditions as one small two-line block, the same on the results and on Best Times: the weather and the
 * time of day ("STRM DUSK"), and under them, centred, the difficulty's word ("ORIGINAL"). An imported row (conditions not known) shows "-". */
#ifndef TNFS_RECORD_CONDITIONS_H
#define TNFS_RECORD_CONDITIONS_H

#include "game/sys/race_records.h"

#include <stdint.h>

/* how a screen writes the block: its words at num/den of the font's size from (x, y), their width, and where their letters start and end
 * below that y (text_ink_scaled) */
typedef struct ConditionsPen {
    void *ctx;
    void (*text)(void *ctx, const char *text, int32_t x, int32_t y, int32_t num, int32_t den);
    int32_t (*width)(void *ctx, const char *text, int32_t num, int32_t den);
    void (*ink)(void *ctx, const char *text, int32_t num, int32_t den, int32_t *top, int32_t *bottom);
} ConditionsPen;

/* the rows between the first line's letters and the second's, the same at every size (the owner's: the time's block and the speed's
 * smaller one spaced alike) */
enum { RECORD_CONDITIONS_GAP = 2 };

/* the block's two lines: "CLR DAY" / "ORIGINAL" (EASY, HARD, MIXED when rivals, police and traffic differ); "-" / "" when not known */
void record_conditions_lines(RecordMarks m, char *first, char *second, int size);
/* the block at num/den of the font's size: its right edge at `right`, the two lines centred on each other and RECORD_CONDITIONS_GAP rows
 * apart, the block's letters centred on those of the row of full-size text drawn at `row_y` */
void record_conditions_draw(RecordMarks m, int32_t right, int32_t row_y, int32_t num, int32_t den, const ConditionsPen *pen);

#endif
