/* A stage's results screen after its last segment (0045de08). */
#ifndef TNFS_TRACK_RESULTS_H
#define TNFS_TRACK_RESULTS_H

#include "game/frontend/results_screen.h"

#include <stdint.h>

/* 0045de08 (ax: check the records, then show the ten best after): with
 * ax, 0045be50 (the row the total went in) and 0045c2d0 (the top speed
 * record) first; stats/standtrk.qfs for the track: the place (as 0045cea0's
 * on the totals: "%d / 8", "1 / 2" / "2 / 2", DNF), the player's name and
 * car against the rival's (the best total's on a time trial), the total
 * (0 when flagged), average on the track's distance (+0x69) and top speed,
 * then the best total and the top speed record (lit when beaten). Its
 * fanfare: with the track music on and a record (the top speed, or the
 * first row), one of three by which; a tournament's or a cup's winner of
 * the race (place 0, race type not 0) hears others, the champion's when all
 * eight tracks are won (nine with Lost Vegas) and 0045b73c says the player
 * leads; there, without a record, ax still plays one (the music setting
 * aside). 0051dd14 (st->record_music) picks the second of each pair and is
 * set. "Done" as 0045c45c's; at the end ax == 1 shows 0045c45c. */
void track_results_screen(int32_t ask, const ResultsInputs *in, ResultsState *st, DialogPointer *p,
                          const ResultsOps *ops);

#endif
