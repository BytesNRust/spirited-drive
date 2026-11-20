/* port (dep59 car editor, N1): a car's texture as the race shows it at night or by day, from plain buffers: the palette tinted as the race tints it, the lamp texels
 * (the brake, position, head and reversing lights of the mask: formats/lamp_emissive.h) at the fixed colours the race gives them, and the police car's lights
 * (formats/police_flash.h). The editor's painted preview gives it the texels the paint left (indices) and the mask; it keeps no state. What the race adds from the
 * place (the light at the car's node, fog, the glow on the road, halos) is not here. */
#ifndef TNFS_CAR_NIGHT_H
#define TNFS_CAR_NIGHT_H

#include "formats/palette_tint.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
    int tinted;              /* 1: the car's palette is tinted with `tint` (a track's INFO tint of its time of day), 0: as the file has it */
    PaletteTint tint;
    int lit;                 /* 1: the car's colours are scaled by `light` (the race's ratio of the light at the car: game/viewer/view_light.c car_light), after the tint, per channel, 6 bits */
    float light[3];          /* the ratio at the car's node: 1 is the colour as the file has it */
    int night;               /* 1: low light: the lamps can glow (lamp_emissive_low); 0 by day: only the brake and the reversing lights */
    int lights;              /* at night: the head and position lights (in the race they go together) */
    int brake;               /* the brake lights, in any light: by day the race writes the car's lamp entry with the lit colour (game/race/car_lights.c) */
    int reverse;             /* the reversing lights, in any light */
    int police;              /* the police lights (texels 0xfa..0xfd) flash */
    int32_t police_tick;     /* the tick of the flash: police_lights_phase */
} CarNightState;

/* The race's lamp state of `s` (formats/lamp_emissive.h LampState): by day LAMP_DAY; at night LAMP_BRAKING with the brake on, LAMP_LOW with the lights, LAMP_DAY with neither;
 * LAMP_REVERSING added with the reversing light on. */
int car_night_lamp_state(const CarNightState *s);

/* The 8-bit RGB palette (256 colours) the car's bank has in state `s`: `palette8` as the race takes it (6 bits: each byte >> 2), tinted when s->tinted, back to 8 bits as the screen has it. */
void car_night_palette(const uint8_t palette8[768], const CarNightState *s, uint8_t out8[768]);

/* The RGBA of a texture (`w` x `h`, 8-bit palette indices `index`) in state `s`. `mask` (w x h of CAR_MASK_* classes; NULL: none) says its lamp texels; they take the fixed
 * lamp colours when lit, else the palette's colour of their index (in the race an unlit brake texel shows the position light's colour while the lights are on). With
 * s->police the texels 0xfa..0xfd take the flash's entries of the palette (red and blue, untinted). Entry 0xff is transparent (alpha 0), the others opaque.
 * Returns 0 when a buffer is NULL or the size is 0. */
int car_night_texture(const uint8_t *index, const uint8_t *mask, int w, int h, const uint8_t palette8[768], const CarNightState *s, uint8_t *rgba);

#endif
