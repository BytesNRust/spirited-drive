#ifndef TNFS_WET_ZONES_H
#define TNFS_WET_ZONES_H

#include <stdint.h>
#include <stdlib.h>

/* port (tunnel_dry): how much of the rain reaches the road at each node, 0..16, from the track's own data alone (the road's property bytes: its tunnels and Coastal's galleries), so the
 * sim can use it: a pure function of (node, zones), the same for the player and for every rival, on any machine. The PS1 disc's light and the light files are not read. Closed nodes are 0,
 * fading to 16 over WEATHER_MOUTH_NODES nodes outside the mouths (weather_open_for_distance). Returns a malloc'd array of `count` bytes, NULL when there is no data. */
uint8_t *wet_zones_make(const uint8_t *property, uint32_t count, int ring);

#endif
