/* port: the PS1 look's light script (re/notes/psx-light.md). Z<TRACK><A|B|C>.LGS is text: C comments, then the depth cue's
 * four points (X,Y percent), the default ambient colour at the start and at the end (128 is neutral, per channel) and the
 * light contexts. Only the ambient colour is read here; the contexts (the cars' light by segment range) are not. Numbers are
 * separated by anything that is not a digit, so commas, spaces and CR / LF all do, and the file's trailing binary after the
 * last line ("CRCF" and padding) ends the read. */
#ifndef TNFS_PSX_LGS_H
#define TNFS_PSX_LGS_H

#include <stddef.h>
#include <stdint.h>

enum { PSX_LGS_CONTEXTS = 32, PSX_LGS_FIXED = 64, PSX_LGS_RAMP = 96, PSX_LGS_BLEND = 2 };

typedef struct {
    int seg_ini, seg_fin;   /* the segments the context covers, both ends included */
    int mode;               /* PSX_LGS_FIXED: c1 throughout; PSX_LGS_RAMP: c1 at seg_ini to c2 at seg_fin */
    int c1[3], c2[3];
} PsxLgsContext;

typedef struct {
    int depth[8];       /* X0,Y0 .. X3,Y3 */
    int start[3];       /* the ambient colour at the track's start: r, g, b */
    int end[3];         /* and at its end */
    int context_count;  /* the contexts read (those after PSX_LGS_CONTEXTS are dropped; none when the file stops early) */
    PsxLgsContext contexts[PSX_LGS_CONTEXTS];
} PsxLgs;

/* 1 and `out` set when the text has the depth cue and both ambient colours (each channel 0..255); 0 otherwise. */
int psx_lgs_parse(const uint8_t *file, size_t size, PsxLgs *out);

/* The night's light over a time of day's: each channel of the dusk's ambient times this (a dark blue); the one place to change the night's darkness
 * and colour (the race's light, drive_light.c, and the track screen's night art, track_select.c). */
extern const float psx_night_light[3];

/* The ambient colours (the start's and the end's, and each context's two) multiplied per channel by `mul`, rounded and kept in 0..255: the night's
 * light over a time of day's script; each context's two (a tunnel's own lights) too when `contexts`, else as the script has them. */
void psx_lgs_scale(PsxLgs *lgs, const float mul[3], int contexts);

/* The ambient colour as a multiplier of each channel, 1.0 for 128; the mean of the start and end colours. */
void psx_lgs_tint(const PsxLgs *lgs, float tint[3]);

/* The ambient colour the cars and the dash have at `segment` of a track of `segment_count`, as a multiplier of each channel (1.0
 * for 128): inside a context its colour (fixed, or ramped across its range), else the start colour ramped to the end across the
 * track; PSX_LGS_BLEND segments outside a context its colour fades in and out into the default one. */
void psx_lgs_ambient(const PsxLgs *lgs, int segment, int segment_count, float out[3]);

/* 1 when `segment` is inside a PSX_LGS_RAMP context (kUseAC + kSwitchAC, 96): the tunnels, where the ceiling lamps switch the cars' light;
 * the edges are exact, no blend. 0 for NULL. */
int psx_lgs_switched(const PsxLgs *lgs, int segment);

/* The exe's light of a track point (0x8003DAD0 fills the colour, 0x8003D380 the table T): the light script's contexts are by BLOCK (a block is 4 nodes,
 * the segment) with no blend at their edges. In a block with a context of mode PSX_LGS_FIXED or PSX_LGS_RAMP (0x40: its colour is added) the point's
 * input is the context's colour alone (the LGT is read only by a mode with 0x80, the default of a block with no context); a RAMP context (0x20)
 * takes its second colour on odd nodes and its first on even ones, the lamps' alternation. Per channel k = colour + LGT, C = T[k] and the light is
 * C / PSX_EXE_FULL: the lit road (LGT 255, no context) is 1, LGT 0 is 0.2 (T's floor of 32), a context's colour 128 is 0.6. T[k] = min(255, 32 + 128 k / 255).
 * `lgt` is the LGT byte of the point (the file's), `lgs` may be NULL (no contexts). Not capped above (a colour of 255 is 1.59). The exe takes C / 128 as
 * its modulate factor, which makes the lit road 1.25: the port keeps the lit road at 1 (PSX_EXE_FULL = T[255] = 160), the one constant to change for that. */
enum { PSX_EXE_FULL = 160 };

/* `node` on a ring of `ring_nodes`: the node the road's data (the light, the script's blocks) is of. The rings are not powers of two (CY1 has 1920), so
 * this is a modulo, never a mask. */
uint32_t psx_ring_node(int32_t node, uint32_t ring_nodes);
int psx_exe_t(int k);
void psx_lgs_light(const PsxLgs *lgs, int node, int lgt, float out[3]);

/* The factor a car's (and the dash's) colours are scaled by at `node` of a ring of `ring_nodes`: the exe's light of the road's centre point there
 * (`lgt` its LGT byte), rounded to 1/`step` and at most 2. The node is the car's own (the exe gives every car the viewer's). Exactly 1 where the light is full. */
void psx_lgs_car_ratio(const PsxLgs *lgs, uint32_t ring_nodes, int32_t node, int lgt, int step, float out[3]);

/* The ambient at `segment` over the world's `tint`, per channel, rounded to 1/`step` and at most 2 (a tint under 0.01 counts as 0.01): what the
 * cars' and the dash's colours are scaled by. Exactly 1 where the ambient is what the world already has. */
void psx_lgs_ratio(const PsxLgs *lgs, int segment, int segment_count, const float tint[3], int step, float out[3]);

/* A car's colour table under an ambient colour that differs from the world's: index c maps to the palette entry nearest the colour
 * of c times `ratio` (per channel, ambient / the world's tint); the shadow and see-through indices (0xfe, 0xff) stay. Ratios of 1 give the
 * identity. */
void psx_lit_table(const uint8_t palette[768], const float ratio[3], uint8_t out[256]);

/* `out` (768 bytes, 8 bits a channel) is `palette` with every entry multiplied by `tint` (per channel, clamped to 255,
 * rounded). A tint of the palette keeps each index its own colour, so a texture's variation survives, where moving each
 * polygon's colours to the nearest palette entry gathers them into the few dark ones the palette has. It is used for the 3D's
 * pixels only (psx_world_mask): the HUD and the dash index the same entries. */
void psx_tint_palette(const uint8_t palette[768], const float tint[3], uint8_t out[768]);

/* Which pixels of a finished frame are the 3D world's: `world` is the frame as it was when the 3D was done and the 2D not yet
 * drawn, `shown` the frame now; a pixel the 2D layer changed is not the world's (mask 0), the others are (1). */
void psx_world_mask(const uint8_t *shown, const uint8_t *world, size_t count, uint8_t *mask);

/* The rectangle (x, y, w, h) of a mask `width` x `height` set to 1, clipped to it: the rear-view mirror's picture, which the 2D layer
 * carries but shows the 3D. */
void psx_mask_rect(uint8_t *mask, int width, int height, int x, int y, int w, int h);

#endif
