/* port: what the port makes of a light script (psx_lgs.h) and of the light model's bytes: the ambient and the night's tint, the exe's curve from a byte to a light ratio, the colour tables a ratio makes, the
 * tinted palettes (the world's and the dash's), and the masks that keep the HUD out of the tint. Our own effect; the reading of the files stays in psx_lgs.h. */
#ifndef TNFS_LIGHT_TINT_H
#define TNFS_LIGHT_TINT_H

#include "game/race/psx_lgs.h"

#include <stddef.h>
#include <stdint.h>

/* The night's light over a time of day's: each channel of the dusk's ambient times this (a dark blue); the one place to change the night's darkness
 * and colour (the race's light, drive_light.c, and the track screen's night art, track_select.c). */
extern const float light_tint_night[3];

/* The ambient colours (the start's and the end's, and each context's two) multiplied per channel by `mul`, rounded and kept in 0..255: the night's
 * light over a time of day's script; each context's two (a tunnel's own lights) too when `contexts`, else as the script has them. */
void light_tint_scale(PsxLgs *lgs, const float mul[3], int contexts);

/* port (the night's ceiling): the factor (at most 1) that brings the mean of `tint`'s channels down to `ceiling`; 1 when it is under it already. The the console version's nights
 * differ widely (TR2's orange dusk is as bright as the open roads' day), the port's all go dark (the owner's rule), the hue kept. */
float light_tint_night_ceiling_scale(const float tint[3], float ceiling);

/* The ambient colour as a multiplier of each channel, 1.0 for 128; the mean of the start and end colours. */
void light_tint_script_tint(const PsxLgs *lgs, float tint[3]);

/* The exe's light of a track point (0x8003DAD0 fills the colour, 0x8003D380 the table T): the light script's contexts are by BLOCK (a block is 4 nodes,
 * the segment) with no blend at their edges. In a block with a context of mode PSX_LGS_FIXED or PSX_LGS_RAMP (0x40: its colour is added) the point's
 * input is the context's colour alone (the LGT is read only by a mode with 0x80, the default of a block with no context); a RAMP context (0x20)
 * takes its second colour on odd nodes and its first on even ones, the lamps' alternation. Per channel k = colour + LGT, C = T[k] and the light is
 * C / LIGHT_TINT_CURVE_FULL: the lit road (LGT 255, no context) is 1, LGT 0 is 0.2 (T's floor of 32), a context's colour 128 is 0.6. T[k] = min(255, 32 + 128 k / 255).
 * `lgt` is the LGT byte of the point (the file's), `lgs` may be NULL (no contexts). Not capped above (a colour of 255 is 1.59). The exe takes C / 128 as
 * its modulate factor, which makes the lit road 1.25: the port keeps the lit road at 1 (LIGHT_TINT_CURVE_FULL = T[255] = 160), the one constant to change for that. */
enum { LIGHT_TINT_CURVE_FULL = 160 };

/* `node` on a ring of `ring_nodes`: the node the road's data (the light, the script's blocks) is of. The rings are not powers of two (CY1 has 1920), so
 * this is a modulo, never a mask. */
uint32_t light_tint_ring_node(int32_t node, uint32_t ring_nodes);
int light_tint_curve(int k);
void light_tint_script_light(const PsxLgs *lgs, int node, int lgt, float out[3]);

/* The factor a car's (and the dash's) colours are scaled by at `node` of a ring of `ring_nodes`: the exe's light of the road's centre point there
 * (`lgt` its LGT byte), rounded to 1/`step` and at most 2. The node is the car's own (the exe gives every car the viewer's). Exactly 1 where the light is full. */
void light_tint_car_ratio(const PsxLgs *lgs, uint32_t ring_nodes, int32_t node, int lgt, int step, float out[3]);

/* The ambient at `segment` over the world's `tint`, per channel, rounded to 1/`step` and at most 2 (a tint under 0.01 counts as 0.01): what the
 * cars' and the dash's colours are scaled by. Exactly 1 where the ambient is what the world already has. */
void light_tint_ratio(const PsxLgs *lgs, int segment, int segment_count, const float tint[3], int step, float out[3]);

/* A car's colour table under an ambient colour that differs from the world's: index c maps to the palette entry nearest the colour
 * of c times `ratio` (per channel, ambient / the world's tint); the shadow and see-through indices (0xfe, 0xff) stay. Ratios of 1 give the
 * identity. */
void light_tint_lit_table(const uint8_t palette[768], const float ratio[3], uint8_t out[256]);

/* port (car_tunnel_match): the same with `any_family` 1 taking the nearest usable colour of any family (the family keeps a red in the reds, but above full light a palette with no lighter red
 * leaves the car at its brightest red while the world's tables, which have no family, go on lighter) */
void light_tint_lit_table_ex(const uint8_t palette[768], const float ratio[3], int any_family, uint8_t out[256]);

/* port (car_night_lift): the same with a floor of light `add` (8 bits a channel, NULL none) added to the wanted colour: a near-black paint (a dark car at night) is lifted out of black by it, which a
 * ratio cannot do. With a floor the table is made even at ratio 1. */
void light_tint_lit_table_floor(const uint8_t palette[768], const float ratio[3], int any_family, const uint8_t add[3], uint8_t out[256]);

/* port (track light file, tono_coche): `base` (a table as light_tint_lit_table makes, NULL for none) with the car's colours taken toward the tone `rgb` (0..255 each) by `percent` 0..100: each colour keeps its brightness and
 * its hue goes toward the tone's (a red car toward orange, a white one to it, a black one to a dark orange); the result is the nearest colour the palette has, of any hue (the palette is all a car can take). */
void light_tint_car_tone_table(const uint8_t palette[768], const uint8_t *base, const uint8_t rgb[3], int percent, uint8_t out[256]);

/* `out` (768 bytes, 8 bits a channel) is `palette` with every entry multiplied by `tint` (per channel, clamped to 255,
 * rounded). A tint of the palette keeps each index its own colour, so a texture's variation survives, where moving each
 * polygon's colours to the nearest palette entry gathers them into the few dark ones the palette has. It is used for the 3D's
 * pixels only (light_tint_world_mask): the HUD and the dash index the same entries. */
void light_tint_palette(const uint8_t palette[768], const float tint[3], uint8_t out[768]);
/* The dash's art: `palette` moved the share num / den of the way to its tint (half at dawn and dusk, the whole at night). */
void light_tint_dash_palette(const uint8_t palette[768], const float tint[3], int num, int den, uint8_t out[768]);

/* Which pixels of a finished frame are the 3D world's: `world` is the frame as it was when the 3D was done and the 2D not yet
 * drawn, `shown` the frame now; a pixel the 2D layer changed is not the world's (mask 0), the others are (1). */
void light_tint_world_mask(const uint8_t *shown, const uint8_t *world, size_t count, uint8_t *mask);

/* The rectangle (x, y, w, h) of a mask `width` x `height` set to 1, clipped to it: the rear-view mirror's picture, which the 2D layer
 * carries but shows the 3D. */
void light_tint_mask_rect(uint8_t *mask, int width, int height, int x, int y, int w, int h);

#endif
