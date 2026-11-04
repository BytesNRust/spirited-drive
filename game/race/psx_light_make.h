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

#include <stddef.h>
#include <stdint.h>

typedef struct {
    float x, y, z;   /* a terrain point in the track's units, y up */
} LightPos;

/* 0..1 (anything outside is clipped) as a byte 0..255 (the CD's own bytes are not banded: its mean follows 255 v at every band). */
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

/* What the light of a point is made from, for a tool that fits it to the CD's (tools/psx_light_fit.c): the share of the dome it sees, the tangent of the horizon
 * toward the default sun, its surface's upward normal, how far it is over the node's centre point and across from it (units). */
typedef struct {
    float sky, sun_tan, nx, ny, nz, rise, lateral;
} PsxLightFeatures;
void psx_light_features(const LightPos *pts, uint32_t count, int ring, uint32_t node, int point, PsxLightFeatures *out);

/* The day's Lambert light (0..1, unclipped) of a surface normal at a time (PSX_SYNTH_DAWN .. PSX_SYNTH_DUSK), the model before smoothing and the 16 levels. */
float psx_light_lambert(int time, float nx, float ny);

/* As psx_light_make_rows with the sun given (NULL: the time's default). */
int psx_light_make_rows_sun(const LightPos *pts, uint32_t count, int ring, int time, const PsxLightSun *sun, uint8_t *data);

/* The rows for `count` nodes of 11 points (`pts`: TriRow order, the centre, a chain of 5 each side), at `time` (PSX_SYNTH_DAWN .. PSX_SYNTH_DUSK; the night is
 * made apart), into `data` (count * PSX_LIGHT_POINTS bytes). 1 on success; 0 for a NULL, fewer than 2 nodes or an unknown time. `ring` joins the last node to the first. */
int psx_light_make_rows(const LightPos *pts, uint32_t count, int ring, int time, uint8_t *data);

/* The night's lamps: the terrain's points within PSX_LIGHT_LAMP_REACH nodes of each of the `n` lamp nodes (a ring wraps; on a stage a node past the end is dropped) are
 * lifted by a pool that fades with the distance, still in the 16 levels. The road's points are left as they are. */
enum { PSX_LIGHT_LAMP_REACH = 6 };
void psx_light_add_lamps(uint8_t *data, uint32_t count, int ring, const uint32_t *lamp_nodes, int n);

/* port (lamp_pool_tint): how strong the lamps' pool is at each of `count` nodes, 0 (none) .. 4 (under a lamp), into `out`: the pool's own weight (the one psx_light_add_lamps lifts the
 * terrain by) in four steps, the strongest of the lamps' near. */
void psx_light_lamp_classes(uint8_t *out, uint32_t count, int ring, const uint32_t *lamp_nodes, int n);

/* port (lamp_pools): the night's lamps laid over a light that was not made here (the CD's own LGT): psx_light_add_lamps on its rows, and with `classes` its pool_class (psx_light_lamp_classes)
 * made too. 1 when pools were added; 0 for no light, no lamps, or one already pooled (`light->pooled`), so a second call changes nothing. */
int psx_light_add_pools(PsxLight *light, int ring, const uint32_t *lamp_nodes, int n, int classes);

/* An override: nodes node_from..node_to and points point_from..point_to (0..10) get `value` (0..255, put in the 16 levels). The per-track text file, one rule a line:
 * `node_from node_to point_from point_to value`, `#` starts a comment, a bad line is skipped. A rule's nodes past the track are ignored when applied. */
typedef struct {
    int node_from, node_to, point_from, point_to, value;
} PsxLightRule;

/* The rules of `text` (`size` bytes, not NUL-terminated needed): up to `max`, their count. NULL or empty text: 0. */
int psx_light_rules_parse(const char *text, size_t size, PsxLightRule *out, int max);

/* Applies `n` rules to `count` nodes of rows, in order, the last word the later one's. */
void psx_light_apply_rules(uint8_t *data, uint32_t count, const PsxLightRule *rules, int n);

/* port (forest_dark): a forest, nodes `from`..`to` of the rows, is sombre: the whole row alternates between two dark tones, `period` nodes each (the first band the darker), the way a
 * hall of lamps does, eased in over the first and the last PSX_LIGHT_FOREST_RAMP nodes. A multiple of what the rows hold, so what the terrain shaded stays shaded. Nothing for bad arguments. */
enum { PSX_LIGHT_FOREST_RAMP = 3, PSX_LIGHT_FOREST_BAND = 8 };   /* nodes to a band */
void psx_light_forest(uint8_t *data, uint32_t count, uint32_t from, uint32_t to, int period);

/* the nodes of a track's forest (Vertigo Ridge's, TR3, the trees before the finish), 1 and `from`, `to` when the track has one */
int psx_light_forest_range(const char *track, uint32_t *from, uint32_t *to);

/* The forest of `track` laid over `light`'s rows (made, or the CD's: the file's node order), the flag's to ask. 1 when the track has a forest and the rows reach it. */
int psx_light_forest_apply(PsxLight *light, const char *track);

/* The terrain's points of a track, node_count * PSX_LIGHT_POINTS of them, into `out` (the track's units: 16.16 fixed point scaled to 1 = a unit). 1 on success. */
int psx_light_tri_points(const Tri *tri, LightPos *out);

/* A PsxLight of the track's terrain (the data owned: psx_light_free). 1 on success, else 0 and `out` is untouched. */
int psx_light_make(const Tri *tri, int time, PsxLight *out);

/* As psx_light_make, with the night's lamps at `lamp_nodes` (the scenery placements' nodes of the street lamp boards): ignored by day. */
int psx_light_make_lit(const Tri *tri, int time, const uint32_t *lamp_nodes, int lamps, PsxLight *out);

#endif
