/* port: the PS1 look's light for the two tracks the PS1 CD holds none for (Burnt Sienna, tr6, whose ZTR6 has other nodes; Transtropolis, tr7, with no Z file),
 * made at the race's start from the track's own road: nothing of the CD is read, nothing is written. The TRI marks a tunnel's nodes (the road property byte,
 * bit 4: CY1's run is nodes 1064..1171, the rows the CD's LGT darkens at 1063..1174); a run of PSX_SYNTH_MIN_RUN nodes or more is a tunnel, a shorter one
 * (4 or 8 nodes: an overpass or a bridge's shade, CY1 500 / 508) is not. A tunnel's rows are a CD tunnel's rows (psx_light.h: road 255, wall foot 255, every
 * point beyond 0, which psx_light_at reads as a dim road, a half-light wall), and PSX_SYNTH_RAMP nodes either side of it step from the open road's light to the
 * tunnel's (the road, foot and wall values in between are a straight line, so the mouth does not step). */
#ifndef TNFS_PSX_LIGHT_SYNTH_H
#define TNFS_PSX_LIGHT_SYNTH_H

#include "formats/tri.h"
#include "game/race/psx_light.h"

#include <stdint.h>

enum {
    PSX_SYNTH_PROPERTY_TUNNEL = 4,   /* TriNode.property bit: a tunnel (CY1 1064..1171, TR6 199..546) */
    PSX_SYNTH_MIN_RUN = 12,          /* nodes: the shortest run that is a tunnel */
    PSX_SYNTH_RAMP = 4               /* nodes either side of a tunnel that ramp */
};

/* 1 on node `i` of `property` (`count` bytes, a ring when `ring`) when it is in a run of PSX_SYNTH_MIN_RUN or more nodes with the tunnel bit; `flags` gets
 * 0 or 1 per node. A ring joins a run across node 0. */
void psx_light_synth_tunnels(const uint8_t *property, uint32_t count, int ring, uint8_t *flags);

/* The rows of a light for the nodes with `flags` (count of them): a tunnel's rows, the ramp's, the open road's 255. `data` is count * PSX_LIGHT_POINTS bytes. */
void psx_light_synth_rows(const uint8_t *flags, uint32_t count, int ring, uint8_t *data);

/* A PsxLight of the track's tunnels (its nodes node_count, the data owned: psx_light_free). 1 when the track has a tunnel, else 0 and `out` is untouched. */
int psx_light_synth(const Tri *tri, PsxLight *out);

/* 1 for the tracks it is for: "TR6" and "TR7" (any case). */
int psx_light_synth_track(const char *track);

/* The light a car and the dash have at `node` of a synthesised light, as a multiplier of each channel: 1 on the open road, PSX_SYNTH_CAR_TUNNEL in a tunnel, the
 * ramp's straight line between. The CD's tunnel colours (re/notes/psx-light.md: lit under a lamp 9 / 7 / 6 of 16, between lamps 7 / 7 / 7) average about a half. */
enum { PSX_SYNTH_CAR_PERCENT = 50 };
float psx_light_synth_car(const PsxLight *light, uint32_t node);

#endif
