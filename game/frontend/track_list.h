/* port (dep59 step 4): the tracks the menus' arrows step through (notes/dep59-step4-editions-plan.md, section 2, Task 2), by their stock
 * number (stock_content.h): the standard edition's 0..7, and Lost Vegas (8) when it is unlocked, as the original wraps; an edition's
 * tracks= in its own order, Lost Vegas by being listed there. An id there that names no stock track (a track piece, a later step's) is left
 * out of the list; the edition's file keeps it. The hidden track (9, the cut moon track) is never listed: the CD has no geometry for it
 * (no MISC/TR8.TRI, and the exe's stem table stops at 8), so an edition that names it has it logged and skipped like a missing piece. */
#ifndef TNFS_TRACK_LIST_H
#define TNFS_TRACK_LIST_H

#include "formats/edition_ini.h"

#include <stdint.h>

enum { TRACK_LIST_MAX = 10 + EDITION_TRACKS_MAX };

typedef struct {
    int16_t track[TRACK_LIST_MAX];
    int count;
} TrackList;

/* the stock tracks in the edition's order, 9 left out (e NULL: the standard, 0..7, and 8 when lost_vegas) */
void track_list_build(TrackList *out, const Edition *e, int lost_vegas);
/* the track `step` (1 on, -1 back) from `track` round the list; a track not in it gives the list's first (0 for an empty list) */
int16_t track_list_step(const TrackList *l, int16_t track, int step);

#endif
