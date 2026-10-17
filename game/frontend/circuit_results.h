/* A circuit's results screen (0045eff4), shown after a circuit race. */
#ifndef TNFS_CIRCUIT_RESULTS_H
#define TNFS_CIRCUIT_RESULTS_H

#include "game/frontend/results_screen.h"

#include <stdint.h>

/* 0045eff4 (ax: check the records, then show the ten best after): with
 * ax, 0045be50 (the row the total went in), 0045c2d0 (the top speed
 * record) and 0045b908 (the lap record) first; 0051dd14 is cleared.
 * stats/standlo1.qfs for the track (3..8): the place ("%d / 8" from
 * 0051dcc4 / 0051dcbe after 0045b6c8 on a tournament or cup, "1 / 2" /
 * "2 / 2" against the other player or the best total, DNF), the laps
 * ("%d Lap race"), the player's name and car against the rival's (the best
 * total's on a time trial), the total (0 when flagged), average on +0x69
 * and top speed against the rival's (the other player's, or the finisher
 * after or before the player in 0051dccc: 0 when the player did not
 * finish), then the best total (of the difficulty) and the top speed
 * record, lit when beaten. A race won (or a cup) plays a winner's fanfare,
 * the champion's when all eight tracks are won (nine with Lost Vegas) and
 * 0045b73c says the player leads; else a record with the track music on
 * plays one; 0051dd14 is set. The pointer picks "Done" (0, 0x82), the lap
 * times (400, 0x280) or neither; the arrows move between them. The lap
 * times (0046067c with the lap record and a count of the times shown)
 * end the screen when they return non-zero, else it is loaded again.
 * Enter, space or the left button on "Done" ends it, as do Escape, the
 * timer, the right button and 0050a5b6; at the end ax == 1 shows
 * 0045c45c. */
void circuit_results_screen(int32_t ask, const ResultsInputs *in, ResultsState *st, DialogPointer *p,
                            const ResultsOps *ops);

#endif
