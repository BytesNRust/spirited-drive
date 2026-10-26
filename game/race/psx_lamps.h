/* port (the PS1 look's light, psx_light.h): the tunnels' lamps. The baked light darkens a tunnel's walls and ceiling, and with them the
 * texture of each ceiling lamp; the PS1 keeps the lamp itself bright. A lamp is a texture of the track (a slot of the track's TRKD
 * texture bytes, id * 3 + 0..2) and, inside it, a few palette entries (the fitting's colours): in that texture only, those entries
 * are not darkened. The palette entries are the PC's and are shared with the sky and the windows, so the texture is the key, never the
 * entry alone. Found by tnfs-decomp's helper from the PC tracks' textures; no PS1 data is used. */
#ifndef TNFS_PSX_LAMPS_H
#define TNFS_PSX_LAMPS_H

#include "game/race/psx_lgs.h"

#include <stdint.h>

enum { PSX_LAMP_COLOURS = 8 };

typedef struct {
    int slot;                        /* the track's texture byte: slot of TrackArt, in every variant it has */
    int count;                       /* the colours below */
    uint8_t idx[PSX_LAMP_COLOURS];   /* palette entries of the lamp's fitting */
    int rect[4];                     /* x0, y0, x1, y1 (inclusive, texels): where the fitting and its glow are; the colours seen only there stay bright too */
} PsxLamp;

/* The lamps of the track (CY1, CY2, CY3, TR4, in any case; the others have none): how many, and `*out` (when not NULL) the table. */
int psx_lamps_for(const char *track, const PsxLamp **out);

/* A tunnel's lamps hang one to a segment (4 nodes: 7 m, the texture once along it) and the PS1 car's light switches under each: lit while
 * its position `node_position` (a node and the fraction past it, a ring's node 0 included) is within PSX_LAMP_DUTY of a segment of the
 * lamp's place `v` (0..1 along the segment), else not; hard steps, no ramp (measured on the PS1: ~55% lit, ~6-7 Hz at 200 km/h). */
enum { PSX_LAMP_NODES = 4 };
#define PSX_LAMP_DUTY 0.55
int psx_lamp_lit(double node_position, double v);

/* Where the lamp hangs along its segment (0..1 from the segment's first node, the way the nodes run): the lamp's rectangle centre across the texture's
 * u, which runs along the road and against the nodes (the track's polygons map u = 0 at the far row, 1 at the near one), so 1 - u. `width` the texture's;
 * the rectangle's far edge may be ALL. A lamp that fills the width hangs mid-segment. */
double psx_lamp_place(const PsxLamp *lamp, int width);

/* `node` on a ring of `ring_nodes`: the node the road's data (the light, the script's segments) is of. The rings are not powers of two (CY1 has 1920), so
 * this is a modulo, never a mask. */
uint32_t psx_ring_node(int32_t node, uint32_t ring_nodes);

/* What the cars' and the dash's colours are scaled by at a place: the ratio of the LGS's ambient at the segment of `node` to the world's tint
 * (psx_lgs_ratio), or, in a tunnel of a track with lamps (`lamp_v` >= 0; inside a mode-96 context), the two hard steps by the lamp: lit (PSX_TUNNEL_LIT) while `position`
 * (the node and the fraction past it, a ring's node 0 included) is within PSX_LAMP_DUTY of the lamp's place, else PSX_TUNNEL_UNLIT. One function for the
 * cars and the dash; what the colour means (the formula of the exe) changes in here. `step` is psx_lgs_ratio's. */
void psx_light_car_ratio(const PsxLgs *lgs, uint32_t ring_nodes, int32_t node, double position, double lamp_v, const float tint[3], int step, float out[3]);
extern const float PSX_TUNNEL_LIT[3], PSX_TUNNEL_UNLIT[3];

/* The palette entries of a lamp that stay bright: its listed colours, and the colours its texture (w x h palette indices) shows inside its
 * rectangle and nowhere outside it (the glow's greys, which the dark wall of the same texture does not use). `exempt`: 256 flags. */
void psx_lamp_exempt(const PsxLamp *lamp, const uint8_t *pixels, int w, int h, uint8_t exempt[256]);

/* `out`: `levels` darkening tables like `lit` (256 bytes each, level 0 the world's lightest) with the `exempt` colours mapped to themselves. */
void psx_lamp_tables(const uint8_t exempt[256], const uint8_t (*lit)[256], int levels, uint8_t (*out)[256]);

#endif
