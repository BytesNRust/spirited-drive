/* See car_night.h. */
#include "formats/car_night.h"

#include "formats/car_brake_lamp.h"
#include "formats/lamp_emissive.h"
#include "formats/police_flash.h"

#include <stdlib.h>
#include <string.h>

enum { COLOURS = 256, TRANSPARENT_ENTRY = 0xff };

static uint8_t to_8_bits(uint8_t v)
{
    return (uint8_t)(v << 2 | v >> 4);   /* race_palette_write */
}

int car_night_lamp_state(const CarNightState *s)
{
    int state = LAMP_DAY;
    if (s->night) state = s->brake ? LAMP_BRAKING : s->lights ? LAMP_LOW : LAMP_DAY;
    if (s->reverse) state |= LAMP_REVERSING;
    return state;
}

void car_night_palette(const uint8_t palette8[768], const CarNightState *s, uint8_t out8[768])
{
    uint8_t six[768];
    for (int i = 0; i < 768; i++) six[i] = (uint8_t)(palette8[i] >> 2);
    if (s->tinted) {
        PaletteTint tint = s->tint;   /* the tint is written to as the exe does */
        palette_tint_colours(&tint, 0, COLOURS, six);
    }
    if (s->lit)   /* the light at the car: the race maps each colour to the screen palette entry nearest its scaled colour (light_tint_lit_table_floor); here the scaled colour itself */
        for (int i = 0; i < 768; i++) {
            const float v = (float)six[i] * s->light[i % 3] + 0.5f;
            six[i] = (uint8_t)(v < 0.0f ? 0 : v > 63.0f ? 63 : v);
        }
    car_brake_lamp_colour(s->lamp_row, s->brake, six + 3 * CAR_BRAKE_LAMP_ENTRY);   /* written after the tint and the light, as car_lights does */
    for (int i = 0; i < 768; i++) out8[i] = to_8_bits(six[i]);
}

/* the fixed colour (8 bits) a lit lamp of `kind` shows */
static void lamp_colour(int kind, uint8_t rgb[3])
{
    static const uint8_t SIX[4][3] = { { LAMP_BRAKE_R, LAMP_BRAKE_G, LAMP_BRAKE_B }, { LAMP_POSITION_R, LAMP_POSITION_G, LAMP_POSITION_B },
                                       { LAMP_HEADLIGHT_R, LAMP_HEADLIGHT_G, LAMP_HEADLIGHT_B }, { LAMP_REVERSE_R, LAMP_REVERSE_G, LAMP_REVERSE_B } };
    for (int k = 0; k < 3; k++) rgb[k] = to_8_bits(SIX[kind][k]);
}

/* Which texels glow, and as which kind: -1 for none. The race's own rule (lamp_texels_write) is run twice on a copy with two different sets of entries: a texel that
 * shows entry A.x in the one and B.x in the other is a lit lamp of kind x (a texel left at its day index cannot match both). */
static int lit_kinds(const uint8_t *index, const uint8_t *mask, size_t count, int state, int day_brake, int8_t *kind)
{
    memset(kind, -1, count);
    if (mask == NULL) return 1;
    LampTexels l = { NULL, 0, 0 };
    if (lamp_texels_collect(&l, 0, mask, count) < 0) return 0;
    uint8_t *one = malloc(count), *two = malloc(count);
    if (one == NULL || two == NULL) { free(one), free(two), lamp_texels_free(&l); return 0; }
    memcpy(one, index, count), memcpy(two, index, count);
    lamp_texels_read(&l, one, count);
    const LampEntries a = { 10, 11, 12, 13 }, b = { 20, 21, 22, 23 };
    lamp_texels_write(&l, one, count, (LampState)state, &a);
    lamp_texels_write(&l, two, count, (LampState)state, &b);
    for (int i = 0; i < l.count; i++) {
        const uint32_t at = l.texel[i].offset;
        if (at >= count) continue;
        const uint8_t va = one[at], vb = two[at];
        const uint8_t ea[4] = { 10, 11, 12, 13 }, eb[4] = { 20, 21, 22, 23 };   /* the brake (lit), position, headlight, reversing entries */
        for (int e = 0; e < 4; e++)
            if (va == ea[e] && vb == eb[e]) { kind[at] = (int8_t)(e == 0 ? LAMP_BRAKE : e == 1 ? LAMP_POSITION : e == 2 ? LAMP_HEADLIGHT : LAMP_REVERSE); break; }
        if (day_brake && l.texel[i].kind == LAMP_BRAKE) kind[at] = LAMP_BRAKE;   /* by day the race lights the brake through the car's lamp entry (car_lights.c) */
    }
    free(one), free(two), lamp_texels_free(&l);
    return 1;
}

int car_night_texture(const uint8_t *index, const uint8_t *mask, int w, int h, const uint8_t palette8[768], const CarNightState *s, uint8_t *rgba)
{
    if (index == NULL || palette8 == NULL || s == NULL || rgba == NULL || w <= 0 || h <= 0) return 0;
    const size_t count = (size_t)w * (size_t)h;
    uint8_t pal[768];
    car_night_palette(palette8, s, pal);
    int8_t *kind = malloc(count);
    if (kind == NULL) return 0;
    if (!lit_kinds(index, mask, count, car_night_lamp_state(s), !s->night && s->brake, kind)) { free(kind); return 0; }
    uint8_t flash[256];
    if (s->police) police_lights_table(NULL, palette8, s->police_tick, flash);
    for (size_t i = 0; i < count; i++) {
        uint8_t *px = rgba + 4 * i;
        const uint8_t e = index[i];
        if (kind[i] >= 0) {
            lamp_colour(kind[i], px);
            px[3] = 255;
        } else if (s->police && e >= POLICE_LIGHT_FIRST && e < POLICE_LIGHT_FIRST + POLICE_LIGHT_COUNT) {
            const uint8_t to = flash[e];   /* the flash is not tinted: the world's light does not reach it */
            px[0] = palette8[3 * to], px[1] = palette8[3 * to + 1], px[2] = palette8[3 * to + 2], px[3] = 255;
        } else {
            px[0] = pal[3 * e], px[1] = pal[3 * e + 1], px[2] = pal[3 * e + 2];
            px[3] = e == TRANSPARENT_ENTRY ? 0 : 255;
        }
    }
    free(kind);
    return 1;
}
