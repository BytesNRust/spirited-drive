/* port (track light file, a reversed circuit): a track light file is written in the track's own direction; a circuit driven the other way round runs its nodes mirrored (node j is the file's
 * (nodes - j) mod nodes, the two chains of a row swapped: tri_reverse.h, track_node_light_old_node). */
#ifndef TNFS_TRACK_LIGHT_MIRROR_H
#define TNFS_TRACK_LIGHT_MIRROR_H

#include "game/race/track_light_file.h"

#include <stdint.h>

/* Turns a read file into the direction the track runs when it is reversed: the tunnel marks, the rows and which of them the file sets, every node range and lamp of the retouches (brightness, tone runs,
 * zones, gradients and their steps, lamps, tunnel lamps) and the sides of a row (a strip's points, a brightness line's left and right). A range that began at node 0 ends at the last node (node 0 is its
 * own mirror, the range wraps there). Mirroring twice is the file again (but for a range that began at node 0). 1, or 0 when `nodes` does not match the file's rows or memory runs out. */
int track_light_file_mirror(TrackLightFile *file, uint32_t nodes);

#endif
