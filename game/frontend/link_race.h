/* A link game's race (glue, no oracle): the drive's link (track_viewer.h)
 * from the link game, and the race's messages routed to the drive while it
 * runs. */
#ifndef TNFS_LINK_RACE_H
#define TNFS_LINK_RACE_H

#include <stdint.h>

/* Before the drive, in a link game: this machine's player name (00524df0, its record's for 004273d0) and the chat
 * screen (0040a5d8, the frontend's) the in-race menu's chat item opens */
void link_race_begin(const char *own_name, void (*chat)(void *ctx));

/* After the drive: the race's messages no longer go to it */
void link_race_end(void);

#endif
