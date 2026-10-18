/* A link game's race (glue, no oracle): the drive's link (track_viewer.h)
 * from the link game, and the race's messages routed to the drive while it
 * runs. */
#ifndef TNFS_LINK_RACE_H
#define TNFS_LINK_RACE_H

#include <stdint.h>

/* Before the drive, in a link game: each slot's car pick (the menus' 00524e8d by slot, this machine's 00524e8b) */
void link_race_begin(int16_t own_car, const int16_t *cars_by_slot);

/* After the drive: the race's messages no longer go to it */
void link_race_end(void);

#endif
