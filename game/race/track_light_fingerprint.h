/* port: a hash of the light a track has at a time of day, once the game has made it (the rows, the tunnel and hall flags, the lamps and what they lift, the tunnels' look): the same
 * hash before and after the light's file format changes proves the change changed no light. tests/track_light_fingerprint.sh runs the game with TNFS_LIGHT_FINGERPRINT=<file> and compares. */
#ifndef TNFS_TRACK_LIGHT_FINGERPRINT_H
#define TNFS_TRACK_LIGHT_FINGERPRINT_H

#include "game/race/track_node_light.h"

/* `light`'s hash as 64 lower-case hex digits and a NUL. What the file formats only name differently (canyon, car_dim, car_tone, the forest's mark, the tone slots) is not in it. */
void track_light_fingerprint(const TrackNodeLight *light, char out[65]);

#endif
