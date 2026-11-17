/* port (dep59 step 4): the tracks the menus' arrows step through (notes/dep59-step4-editions-plan.md, section 2, Task 2), by their stock
 * number (stock_content.h): the standard edition's 0..7, and Lost Vegas (8) when it is unlocked, as the original wraps; an edition's
 * tracks= in its own order, Lost Vegas by being listed there. An id there that names no stock track is a registered added track's (track_registry.h, 16..79, T7b) or,
 * when none is registered by it, left out of the list; the edition's file keeps it. The hidden track (9, the cut moon track) is never listed: the CD has no geometry for it
 * (no MISC/TR8.TRI, and the exe's stem table stops at 8), so an edition that names it has it logged and skipped like a missing piece.
 * dep59 Lunar L1: Lunar Springs (10, stock_content.h) comes right after Rusty in the standard edition when its switch is on (level 2), and
 * in an edition where its tracks= puts it, whatever the switch; a tournament never lists it. */
#ifndef TNFS_TRACK_LIST_H
#define TNFS_TRACK_LIST_H

#include "formats/edition_ini.h"

#include <stdint.h>

enum { TRACK_LIST_MAX = 11 + EDITION_TRACKS_MAX };

/* track_list_build's `lunar`: the standard edition lists Lunar Springs (ON) or not (OFF), an edition by its tracks= either way; NEVER
 * leaves it out of both (a tournament) */
enum { TRACK_LIST_LUNAR_NEVER = -1, TRACK_LIST_LUNAR_OFF = 0, TRACK_LIST_LUNAR_ON = 1 };

typedef struct {
    int16_t track[TRACK_LIST_MAX];
    int count;
} TrackList;

/* the stock tracks in the edition's order, 9 left out (e NULL: the standard, 0..7 with 10 after 3 when `lunar` is ON, and 8 when
 * lost_vegas) */
void track_list_build(TrackList *out, const Edition *e, int lost_vegas, int lunar);
/* the track `step` (1 on, -1 back) from `track` round the list; a track not in it gives the list's first (0 for an empty list) */
int16_t track_list_step(const TrackList *l, int16_t track, int step);
/* 1 when `track` is in the list */
int track_list_has(const TrackList *l, int16_t track);

#endif
