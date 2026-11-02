/* port: our own light for a track, made from its terrain (re/notes: the M5 design). The PS1 CD's Z<TRACK><A|B|C>.LGT is the light the PS1 look reads (psx_light.h);
 * a release does not carry the CD, so with none the light is made here at the race's start from the road's own geometry, in memory, deterministic, nothing read
 * from or written to disk. It is the same PsxLight the renderer already reads (11 bytes a node, TriRow order), so nothing downstream changes.
 *
 * The light of a point is how much sky it sees (a hemisphere of rays against the terrain), how much sun it gets (the surface against the time's sun, and a shadow
 * ray), smoothed along and across the road, in 16 levels (the PS1's banding): a flat open point is 255. The road's own points never darken; a tunnel's nodes
 * are the synth's (psx_light_synth.h) rows. */
#ifndef TNFS_PSX_LIGHT_MAKE_H
#define TNFS_PSX_LIGHT_MAKE_H

#include "formats/tri.h"
#include "game/race/psx_light.h"
#include "game/race/psx_light_synth.h"

#include <stdint.h>

typedef struct {
    float x, y, z;   /* a terrain point in the track's units, y up */
} LightPos;

/* 0..1 (anything outside is clipped) as one of 16 levels: 0, 17 .. 255 (level * 17, the PS1's banded look). */
uint8_t psx_light_level(float v);

/* Smooths a light of `count` nodes of PSX_LIGHT_POINTS values (0..1) in place: 1-2-1 along the nodes and along each chain of terrain points; the road's points
 * (0, 1 and 6) keep their value and do not enter the terrain's. `ring` joins the last node to the first. */
void psx_light_smooth(float *light, uint32_t count, int ring);

/* The sun of a time of day: the direction in the ground's plane (radians, 0 toward +x, pi/2 toward +z) and its elevation over the horizon (radians). */
typedef struct {
    float azimuth, elevation;
} PsxLightSun;

/* The time's own sun (PSX_SYNTH_DAWN .. PSX_SYNTH_DUSK): the day's high, the dawn's and the dusk's low; the azimuth the track's own (the default is 0). */
void psx_light_sun_default(int time, PsxLightSun *out);

/* As psx_light_make_rows with the sun given (NULL: the time's default). */
int psx_light_make_rows_sun(const LightPos *pts, uint32_t count, int ring, int time, const PsxLightSun *sun, uint8_t *data);

/* The rows for `count` nodes of 11 points (`pts`: TriRow order, the centre, a chain of 5 each side), at `time` (PSX_SYNTH_DAWN .. PSX_SYNTH_DUSK; the night is
 * made apart), into `data` (count * PSX_LIGHT_POINTS bytes). 1 on success; 0 for a NULL, fewer than 2 nodes or an unknown time. `ring` joins the last node to the first. */
int psx_light_make_rows(const LightPos *pts, uint32_t count, int ring, int time, uint8_t *data);

/* The night's lamps: the terrain's points within PSX_LIGHT_LAMP_REACH nodes of each of the `n` lamp nodes (a ring wraps; on a stage a node past the end is dropped) are
 * lifted by a pool that fades with the distance, still in the 16 levels. The road's points are left as they are. */
enum { PSX_LIGHT_LAMP_REACH = 6 };
void psx_light_add_lamps(uint8_t *data, uint32_t count, int ring, const uint32_t *lamp_nodes, int n);

/* A PsxLight of the track's terrain (the data owned: psx_light_free). 1 on success, else 0 and `out` is untouched. */
int psx_light_make(const Tri *tri, int time, PsxLight *out);

/* As psx_light_make, with the night's lamps at `lamp_nodes` (the scenery placements' nodes of the street lamp boards): ignored by day. */
int psx_light_make_lit(const Tri *tri, int time, const uint32_t *lamp_nodes, int lamps, PsxLight *out);

#endif
