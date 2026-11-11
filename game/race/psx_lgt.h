/* port: the reader of the user's own baked light files, Z<TRACK><A|B|C>.LGT of the console CD: u32 node count, u32 scale, 11 bytes of intensity a node, a 12-byte CRCF
 * trailer. It fills the light model (track_node_light.h); nothing of the files is in the repo. */
#ifndef TNFS_PSX_LGT_H
#define TNFS_PSX_LGT_H

#include "game/race/track_node_light.h"

#include <stddef.h>
#include <stdint.h>

/* The file's bytes as a TrackNodeLight: its node count must be `expected_nodes` (the track's ring nodes) and the file at least as
 * long as the rows. 1 on success; `out` is untouched otherwise. */
int psx_lgt_parse(const uint8_t *file, size_t size, uint32_t expected_nodes, TrackNodeLight *out);

#endif
