/* port (dev only): TNFS_SCENERY_DUMP=file.ppm writes every scenery board of the track just loaded in a sheet (cells 96 x 144, ten to a row, the transparent
 * texels purple) and logs one line per board: its id, size, how many placements use it (descriptor resource / 4) and its type. For finding what a track's lamp
 * posts and windows are made of. */
#ifndef TNFS_SCENERY_DUMP_H
#define TNFS_SCENERY_DUMP_H

#include "game/viewer/track_load.h"

void scenery_dump(const Track *t, const char *path);

#endif
