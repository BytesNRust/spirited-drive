#ifndef TNFS_WET_ZONES_H
#define TNFS_WET_ZONES_H

#include "formats/tri.h"
#include "game/race/track_node_light.h"

#include <stdint.h>
#include <stdlib.h>

/* port (tunnel_dry): how much of the rain reaches the road at each node, 0..16, from the track's data alone, so the sim can use it: a pure function of (node, zones), the same for the player and
 * for every rival, and on every machine given the same data files. The closed zones are the ones the weather's looks use (track_node_light_weather_closed): the places file's tunnels (halls
 * included, a canyon is open air; Coastal's galleries are `tunnel` lines of CL2's). The places file is the only source: with none, the road's own roofed bits (tunnels, galleries) stand in. The console
 * disc's light and the light files are not read. Closed nodes are 0, fading to 16 over WEATHER_MOUTH_NODES nodes outside the mouths (weather_open_for_distance).
 * `places` holds the places' tunnel / canyon arrays and `nodes` / `reversed` (NULL: no places file); `property` the road's property byte of each of the `count` ring nodes. Returns a
 * malloc'd array of `count` bytes, NULL when there is no data. */
uint8_t *wet_zones_make(const TrackNodeLight *places, const uint8_t *property, uint32_t count, int ring);

/* The same for `track` (wet_zones_load.c, with the platform) (`name`, e.g. "CY1"): finds and reads its places file (a mod's first, then data/track/<name>.txt) and the road's property bytes from `tri`, whose ring is `ring_nodes`
 * long; `reversed` is the track's direction. NULL when the road has no data. */
uint8_t *wet_zones_for_track(const Tri *tri, const char *name, int reversed);

/* port (snow_weather): 1 when the track's places file (the same one, found the same way) says `snow 1`: its rain is snow (weather_snow.h) and its rally road is covered in it (rally_gravel.h). 0 with no file
 * or no such line; `snow_map` (NULL: not wanted) is the file's `snow_map` lines, as TrackNodeLight has them (zeros when none); a mod's places file for the track that leaves it out makes the track a plain one. */
int track_snow_for(const Tri *tri, const char *name, uint8_t snow_map[TRACK_NODE_LIGHT_SNOW_SLOTS]);

#endif
