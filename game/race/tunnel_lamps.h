/* port (tunnel_lamps): where the lamps of a track's tunnels and caverns hang. A lamp is a polygon of the track whose texture is one of
 * tunnel_lamp_textures.h's lamp textures (the table of CY1, CY2, CY3, TR3, TR4); its place is the middle of the polygon on the ground plane. The
 * light (track_node_light.h's `lamp` list) puts a pool under each. No console data is used. */
#ifndef TNFS_TUNNEL_LAMPS_H
#define TNFS_TUNNEL_LAMPS_H

#include "formats/tri.h"
#include "game/race/tunnel_lamp_textures.h"
#include "game/race/track_geometry.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t node;   /* the road node the polygon's near row is */
    float x, z;      /* metres: the middle of the lamp polygons of that side of that node */
    float spacing;   /* metres to the next node: the pool's reach is a few of them */
    float along[2];  /* the road's direction there (x, z; a unit vector): the pool is shorter along it than across */
} TunnelLamp;

enum { TUNNEL_LAMP_EVERY = 2 };   /* one lamp a TUNNEL_LAMP_EVERY nodes (the even ones, where the lamp textures hang and the car's lamp alternation puts its light) */

/* The lamps of one segment (its 4 nodes, `segment` * 4 ..): `next_row` the first row of the segment after it (the last band's far row). For each of the segment's even nodes, when a strip's texture byte is one of `slots` (`slot_count`), one lamp for each side of the road that has such strips, at the mean of that side's strips' middles (the lamp textures hang in the strips at the walls: a lamp each side). Up to `max` in `out`; returns how many. */
size_t tunnel_lamps_in_segment(const TrackSegment *segment, const TrackPoint *next_row, uint32_t segment_index, const uint8_t *slots, int slot_count, TunnelLamp *out, size_t max);

/* All the lamps of `tri`'s track `track` (tunnel_lamp_textures_for): in node order, up to `max`; 0 for a track with no lamp textures. */
size_t tunnel_lamps_find(const Tri *tri, const char *track, TunnelLamp *out, size_t max);

#endif
