/* port (dep59 Lunar L2): Lunar Springs as an entry of the track list (notes/dep59-lunar-entry-l1l2l5-addendum.md). The menus' track number
 * never holds 10: Lunar is (Rusty, 3, with its rally, variant_r, and the Lunar pick), so the original's tables, config.dat and the network
 * see Rusty's rally. 10 (STOCK_TRACK_LUNAR, track_traits.h's TRACK_PICK_LUNAR) lives only in the track list (track_list.h) and the arrows'
 * step here. With the variant switches off the entries are Rusty, Rusty Mirror, Rusty Rally (the desert), Lunar, Lunar Mirror, Autumn: the
 * moon has no rally entry, it is one. */
#ifndef TNFS_TRACK_PICK_H
#define TNFS_TRACK_PICK_H

#include "game/frontend/race_settings.h"
#include "game/frontend/track_list.h"

/* the entry of (track, variant_r, lunar): STOCK_TRACK_LUNAR for Rusty's rally with the pick, else `track` */
int track_pick_of(int track, int variant_r, int lunar);
/* the entry `pick` back: STOCK_TRACK_LUNAR gives (Rusty, 1, 1); another its number, the rally as it was, no pick */
void track_pick_set(int pick, int *track, int *variant_r, int *lunar);
/* the arrows (`dir` 1 on, -1 back) from the entry in `s` and `*lunar` over `tracks` (NULL: the original's walk): with `switches` the
 * variant switches' step (race_settings_track_step), else the entries' (race_settings_track_forward / _backward), Lunar among them when the
 * list has it */
void track_pick_step(RaceSettings *s, int *lunar, const Unlocks *u, const TrackList *tracks, int dir, int switches);

/* the Lunar pick the next race runs (0/1): Control Central's, kept here for the race (track_viewer_cars.c's race_extras) */
int track_pick_lunar(void);
void track_pick_lunar_put(int lunar);

/* 1 when a Lunar pick holds (dep59 Lunar L8): Lunar is offered, the track is Rusty's rally, and this machine is not a guest in a link game. A guest
 * races what the host's rules say (session_rules.h: the host's entry decides) and takes no pick of its own into Control Central, where a pick
 * left from a solo session would draw the moon over a host on Rusty's desert. */
int track_pick_holds(int offered, int track, int variant_r, int link_guest);

#endif
