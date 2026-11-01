/* port: the PS1 look's light for the two tracks the PS1 CD holds none for (Burnt Sienna, tr6, whose ZTR6 has other nodes; Transtropolis, tr7, with no Z file),
 * made at the race's start from the track's own road: nothing of the CD is read, nothing is written. The TRI marks a tunnel's nodes (the road property byte,
 * bit 4: CY1's run is nodes 1064..1171, the rows the CD's LGT darkens at 1063..1174); a run of PSX_SYNTH_MIN_RUN nodes or more is a tunnel, a shorter one
 * (4 or 8 nodes: an overpass or a bridge's shade, CY1 500 / 508) is not. A tunnel's rows are the CD's, as measured on CY1 (A, B and C alike where not said): the
 * points beyond the wall foot 0, the road and the wall foot the time's values, and at each door the rows step from the open road's 255 to those along the
 * measured doors' shapes: the points beyond over a few nodes, steep in the middle at the entry and slow at the start at the exit, the foot and the road over
 * PSX_SYNTH_FOOT_RAMP nodes in a smooth S. */
#ifndef TNFS_PSX_LIGHT_SYNTH_H
#define TNFS_PSX_LIGHT_SYNTH_H

#include "formats/tri.h"
#include "game/race/psx_light.h"

#include <stdint.h>

enum {
    PSX_SYNTH_PROPERTY_TUNNEL = 4,   /* TriNode.property bit: a tunnel (CY1 1064..1171, TR6 199..546) */
    PSX_SYNTH_PROPERTY_OPEN = 1,     /* with the tunnel bit: not a tunnel (TR7 778..818, an open truss bridge: the sky shows through its sides) */
    PSX_SYNTH_MIN_RUN = 12,          /* nodes: the shortest run that is a tunnel */
    PSX_SYNTH_FOOT_RAMP = 9,         /* nodes either side of a tunnel over which the wall foot and the road go to their tunnel values */
    PSX_SYNTH_BEYOND_ENTRY = 7,      /* nodes before a tunnel over which the points beyond the foot go dark (CY1: 220 at 12, 192 at 6, 99 at 4, 11 at 3, 0 from 1) */
    PSX_SYNTH_BEYOND_EXIT = 10       /* and after it over which they come back (CY1: 0 to 2, 12, 30, 125, 162, 190, 201, 206, 215) */
};

/* the time of day the rows are of: the PS1 CD's files A, B and C */
enum { PSX_SYNTH_DAWN = 0, PSX_SYNTH_DAY = 1, PSX_SYNTH_DUSK = 2 };

/* 1 on node `i` of `property` (`count` bytes, a ring when `ring`) when it is in a run of PSX_SYNTH_MIN_RUN or more nodes with the tunnel bit; `flags` gets
 * 0 or 1 per node. A ring joins a run across node 0. */
void psx_light_synth_tunnels(const uint8_t *property, uint32_t count, int ring, uint8_t *flags);

/* The rows of a light for the nodes with `flags` (count of them) at `time` (PSX_SYNTH_*): a tunnel's rows, the doors', the open road's 255. `data` is
 * count * PSX_LIGHT_POINTS bytes. CY1's tunnel inside: road 255 by day, 215 at dawn, 214 at dusk; wall foot 130, 196, 106 (dusk's two sides, 14 and 198, averaged). */
void psx_light_synth_rows(const uint8_t *flags, uint32_t count, int ring, int time, uint8_t *data);

/* A PsxLight of the track's tunnels (its nodes node_count, the data owned: psx_light_free). 1 when the track has a tunnel, else 0 and `out` is untouched. */
int psx_light_synth(const Tri *tri, int time, PsxLight *out);

/* 1 for the tracks it is for: "TR6" and "TR7" (any case). */
int psx_light_synth_track(const char *track);

/* The light a car and the dash have at `node` of a synthesised light, as a multiplier of each channel: 1 on the open road, PSX_SYNTH_CAR_PERCENT in a tunnel, the
 * way the points beyond the foot go dark at the doors between. The CD's lamp contexts average R 0.51, G 0.44, B 0.40 in tunnels (re/notes/psx-light.md). */
enum { PSX_SYNTH_CAR_PERCENT = 50 };
float psx_light_synth_car(const PsxLight *light, uint32_t node);

#endif
