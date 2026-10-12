/* The dashboard's colours at the race start (0041f818). */
#include "game/race/cockpit.h"

enum { MODEL_ROWS = 12, LINE_SHIFT = 16 };

/* 004b91d0: per car model red, green, blue (then two words 0x20000 not read here) */
static const int32_t model_colours[MODEL_ROWS][3] = {
    { 255, 0, 0 }, { 255, 0, 0 }, { 255, 0, 0 }, { 255, 64, 0 }, { 255, 0, 0 }, { 255, 0, 0 },
    { 255, 0, 0 }, { 255, 0, 0 }, { 255, 0, 0 }, { 255, 0, 0 }, { 255, 0, 0 }, { 255, 0, 0 }
};

void cockpit_colours(RacePalette *p, const RaceState *race, const Cockpit *c, const CockpitWorld *w, HudState *hud,
                     CockpitColours *out)
{
    const Car *car = race->cars[race_slot(race->player)];
    const int32_t model = car != NULL ? car->model : 0;
    const int32_t *rgb = model_colours[model >= 0 && model < MODEL_ROWS ? model : 0];
    race_palette_read(p, 0, RACE_PALETTE_COLOURS, p->copy);
    race_palette_enable(p, race);
    race_palette_inverse(p);
    race_palette_fade(p, rgb[0], rgb[1], rgb[2], p->copy, out->fade);
    out->line_left = 0, out->line_right = (int32_t)((uint32_t)w->width << LINE_SHIFT);
    out->line_top = 0, out->line_bottom = (int32_t)((uint32_t)w->height << LINE_SHIFT);
    if (c->bank != NULL && c->palette != NULL) race_palette_remap_track_bank(p, c->bank, c->palette);
    hud_reset(hud);
    hud->unknown_4f9a20[1] = hud->unknown_4f9a20[2] = 0;
    if (c->palette != NULL) race_palette_tint(p, 0, RACE_PALETTE_COLOURS, c->palette);
    race_palette_read(p, 0, RACE_PALETTE_COLOURS, out->saved);
}
