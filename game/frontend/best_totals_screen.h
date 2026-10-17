/* The ten best totals' screen (0045c45c), shown after a track's last segment or a circuit's race. */
#ifndef TNFS_BEST_TOTALS_SCREEN_H
#define TNFS_BEST_TOTALS_SCREEN_H

#include "game/frontend/results_screen.h"

#include <stdint.h>

/* 0045c45c (ax: the row the player's total went in, 10.. none; dx: the
 * top speed record beaten): stats/standbst.qfs for the track: the ten best
 * totals (a circuit's for the difficulty, 00524ea3), each name, total, car
 * and gearbox mark, the player's row lit, then the track's top speed
 * record (lit when beaten). A row in the table with the track music on
 * (00524e6b) plays a fanfare, another one after a segment record's
 * (record_music, 0051dd14). "Done" as on 0045cea0's screen, but any arrow
 * moves the pointer on and off it and "dond" is drawn masked (00473ab4).
 * Of in, only image, german, unknown_50a5b6, screen, screen_shape and
 * car_names are read; st->record_music is read, st->inverse written; the
 * records op is not called. */
void best_totals_screen(int32_t row, int32_t top_beaten, const ResultsInputs *in, ResultsState *st, DialogPointer *p,
                        const ResultsOps *ops);

#endif
