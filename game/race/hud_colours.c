/* The HUD's colours (hud_colours.h), as 00446bac sets them. */
#include "game/race/hud_colours.h"

#include "game/sys/palette.h"

#include <string.h>

static const uint8_t COLOURS[HUD_COLOURS][3] = {
    { 0x3f, 0, 0 }, { 0x20, 0, 0 }, { 0, 0x3f, 0 }, { 0, 0x20, 0 }, { 0, 0, 0x3f }, { 0, 0, 0x20 },
    { 0x3f, 0x3f, 0 }, { 0x20, 0x20, 0 }, { 0x3f, 0x3f, 0x3f }, { 0, 0, 0 }, { 6, 6, 6 }, { 0x10, 0x10, 0x10 },
    { 0x20, 0x20, 0x20 }, { 0x30, 0x30, 0x30 }, { 0x3f, 0x14, 0 }, { 0x3f, 0, 0x3f }, { 0, 0x3f, 0x3f }
};
static const uint8_t UNKNOWN_4F9B54[3] = { 0xa, 0xe, 0x12 }, FRAME[3] = { 0x1a, 0x1a, 0x1a };
/* 0050b7d4..: the map's cars; slot 4 is the white of 0050b7b0 */
static const uint8_t MAP_CARS[HUD_MAP_CAR_COLOURS] = { 0xf5, 0xf8, 0xf0, 0xfa, 0, 0xf9, 0xfc, 0xfb };
enum { MAP_CAR_WHITE = 4 };

static int32_t nearest(RacePalette *p, const uint8_t rgb[3])
{
    return race_palette_nearest(p, rgb[0], rgb[1], rgb[2]);
}

void hud_colours(RacePalette *p, HudColours *c)
{
    for (int i = 0; i < HUD_COLOURS; i++) c->colours[i] = nearest(p, COLOURS[i]);
    c->unknown_4f9b54 = nearest(p, UNKNOWN_4F9B54);
    c->frame_colour = nearest(p, FRAME);
    race_palette_read(p, 0, RACE_PALETTE_COLOURS, p->copy);
    palette_shade_table(p->copy, p->enabled, HUD_SHADE_SCALE, c->shade);
    memcpy(c->remap, c->shade, sizeof c->remap);
    memcpy(c->map_cars, MAP_CARS, sizeof c->map_cars);
    c->map_cars[MAP_CAR_WHITE] = (uint8_t)c->colours[HUD_COLOUR_WHITE];
}
