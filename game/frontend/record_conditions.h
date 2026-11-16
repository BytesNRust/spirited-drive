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

/* port (owner, 2026-11-16): a Restomod record's cheats as words in the same small style, where the original drew their letters: NITRO,
 * EXPLODE, TURBO and LUNAR, in that order (the Warrior never: the car's column says it). Two words to a line, so up to two lines
 * ("NITRO TURBO" / "LUNAR"). How many words; "" and "" for none */
int record_cheat_lines(int cheats, char *first, char *second, int size);
/* the words centred on `centre` (both lines on each other) and on the row's letters as record_conditions_draw's, at the largest of
 * 1/2, 2/5 and 1/3 of the font's size that fits `room` (the smallest when none does); nothing for no words */
void record_cheat_draw(int cheats, int32_t centre, int32_t room, int32_t row_y, const ConditionsPen *pen);
/* the words at num/den, the block (lines centred on each other) ending on `right`, on the row's letters; its left edge (`right` for no
 * words) */
int32_t record_cheat_draw_before(int cheats, int32_t right, int32_t row_y, int32_t num, int32_t den, const ConditionsPen *pen);

#endif
