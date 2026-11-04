/* port: the walls of a reversed circuit. The exe draws a fence on a fixed chain point by the side of its bit: the left-flagged one on the right chain's
 * second point, the right-flagged one on the left chain's second point in some LODs and its third in others (TRACK_LODS' fence pairs). A reversed
 * circuit has its sides turned (tri_reverse.h), so a wall that was on the second point of the right chain is now a right-flagged one and, on the LODs
 * that take the third point, stood a point further out: a barrier floating past the verge. track_wall_lod returns, on a reversed circuit,
 * the LOD with its right-flagged pairs on the second point whichever it was; otherwise the exe's own table entry. */
#ifndef TNFS_TRACK_WALL_H
#define TNFS_TRACK_WALL_H

#include "game/race/track_tables.h"

#include <stdint.h>

/* The circuit now running is reversed (the viewer sets it with reverse_track_set), or is not. */
void track_wall_set_inner(int inner);
const TrackLod *track_wall_lod(int lod);

/* port (reverse_rows): the exe's reverse (tri_reverse) swaps the two chains of every row, and the LODs' polygon tables are not mirror images (the tunnel LODs
 * 23..28 join the roof to the far wall from the left chain's fourth point and the right chain's third; some road LODs have one more strip on a side), so a
 * reversed circuit draws a tunnel's roof as a slab and its wall as a hillside falling away. With the option on, a reversed circuit's rows are read turned
 * back (a point from where it stood before the swap), the strip a polygon is tested and textured by is the other side's, the fence bits go back to their
 * sides and the fences use the LOD's own pairs: the forward circuit's geometry, driven the other way. The swapped rows stay as they are for everything else. */
void track_wall_set_rows(int on);
/* the row point a vertex reads */
int track_wall_point(int point);
/* the strip (0-9) a polygon is tested and textured by */
int track_wall_strip(int strip);
/* the segment header's fence byte as the fence code is to read it */
uint8_t track_wall_fence(uint8_t fence);

#endif
