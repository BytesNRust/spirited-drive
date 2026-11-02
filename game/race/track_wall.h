/* port: the walls of a reversed circuit. The exe draws a fence on a fixed chain point by the side of its bit: the left-flagged one on the right chain's
 * second point, the right-flagged one on the left chain's second point in some LODs and its third in others (TRACK_LODS' fence pairs). A reversed
 * circuit has its sides turned (tri_reverse.h), so a wall that was on the second point of the right chain is now a right-flagged one and, on the LODs
 * that take the third point, stood a point further out: a barrier floating past the verge. track_wall_lod returns, on a reversed circuit,
 * the LOD with its right-flagged pairs on the second point whichever it was; otherwise the exe's own table entry. */
#ifndef TNFS_TRACK_WALL_H
#define TNFS_TRACK_WALL_H

#include "game/race/track_tables.h"

/* The circuit now running is reversed (the viewer sets it with reverse_track_set), or is not. */
void track_wall_set_inner(int inner);
const TrackLod *track_wall_lod(int lod);

#endif
