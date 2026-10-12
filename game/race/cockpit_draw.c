/* The cockpit's per-frame pieces (0042270c's calls). */
#include "game/race/cockpit.h"

#include "game/gfx/blit.h"
#include "game/race/car_struct.h"

#include <stddef.h>

enum {
    BIG_LIGHT = 2, FIRST_SMALL = 3, SMALL_LIGHTS = 5,
    GATE_ORIGIN = 1, GATE_STEPS = 4, SHAPE_W = 4, SHAPE_H = 6,
    TICKS_SHIFTED = 0xb, TICKS_DOWN = 0xc, TICKS_UP = 0xd, TICKS_FROM = 0xe
};

/* 00473764: opaque at (x, y); a NULL shape draws nothing (port guard). */
static void put(const QuadTarget *t, const uint8_t *shape, CockpitPoint at)
{
    if (shape != NULL) blit_shape(t, shape, at.x, at.y, BLIT_OPAQUE);
}

static void small(const QuadTarget *t, const Cockpit *c, const uint8_t *shape, int32_t i)
{
    if (FIRST_SMALL + i >= 0) put(t, shape, c->dash_points[FIRST_SMALL + i]);
}

void cockpit_lights(const QuadTarget *t, const Cockpit *c, int32_t count, int32_t big_lit, int32_t *shown)
{
    const int32_t lit = count > SMALL_LIGHTS ? SMALL_LIGHTS : count;
    if (count != 0) put(t, big_lit != 0 ? c->big_lit : c->big_dark, c->dash_points[BIG_LIGHT]);
    if (count != *shown) {
        if (count == 0) {
            put(t, c->big_dark, c->dash_points[BIG_LIGHT]);
            for (int32_t i = 0; i < SMALL_LIGHTS; i++) small(t, c, c->small_dark, i);
        } else {
            for (int32_t i = 0; i < lit; i++) small(t, c, c->small_lit, i);
            for (int32_t i = lit; i < SMALL_LIGHTS; i++) small(t, c, c->small_dark, i);
        }
    }
    *shown = count;
}

static CockpitPoint gate_point(const Cockpit *c, int32_t i)
{
    return i >= 0 && i < COCKPIT_GATE_POINTS ? c->gate_points[i] : (CockpitPoint){ 0, 0 };
}

static int32_t half(const uint8_t *shape, int at)
{
    return (int16_t)(shape[at] | shape[at + 1] << 8) / 2;
}

/* The light centred at (x, y) with 0xff skipped (00473a98). */
static void light(const QuadTarget *t, const uint8_t *shape, int32_t x, int32_t y)
{
    if (shape != NULL) blit_shape_at_transparent(t, shape, x - half(shape, SHAPE_W), y - half(shape, SHAPE_H));
}

/* The state 0041f5bc picks and its point. */
static int32_t gate_state(const Cockpit *c, const Car *car, CockpitPoint *at)
{
    const CockpitPoint a = gate_point(c, car->gear_before_last + 2), b = gate_point(c, car->gear + 2);
    const int32_t ticks = car->gear_change_ticks, origin_y = c->gate_points[GATE_ORIGIN].y;
    if (ticks < TICKS_SHIFTED && car->shift_timer < 1) return *at = b, 1;
    if (ticks >= TICKS_FROM) return *at = a, 2;
    if (ticks == TICKS_UP) return *at = (CockpitPoint){ a.x, origin_y }, 3;
    if (ticks == TICKS_DOWN) return *at = (CockpitPoint){ b.x, origin_y }, 4;
    if (ticks == TICKS_SHIFTED && car->shift_timer < 1) return *at = b, 5;
    return *at = (CockpitPoint){ 0, 0 }, 0;
}

void cockpit_gate(const CockpitSelectOps *ops, HudState *hud, const Cockpit *c, const Car *car,
                  const CockpitGateInputs *in, int32_t *state)
{
    QuadTarget *t = ops->select(ops->ctx, in->back_buffer != 0 ? HUD_SURFACE_SCREEN : HUD_SURFACE_DEFAULT);
    if (in->use_parts != 0) return;
    hud_view_clip(t, hud, &in->view_clip);
    if (in->gate_always == 0 && car->gear_change_ticks < 1) {
        if (*state != 0 && c->gate_back != NULL) blit_shape_placed(t, c->gate_back);
        *state = 0;
        return;
    }
    CockpitPoint at;
    const int32_t now = gate_state(c, car, &at);
    if (now != *state) {
        if (c->gate != NULL) blit_shape_placed(t, c->gate);
        if (c->light_count == COCKPIT_LIGHTS) {
            const CockpitPoint origin = c->gate_points[GATE_ORIGIN];
            const int32_t dx = (int32_t)((uint32_t)at.x - (uint32_t)origin.x) / GATE_STEPS;
            const int32_t dy = (int32_t)((uint32_t)at.y - (uint32_t)origin.y) / GATE_STEPS;
            for (int32_t k = COCKPIT_LIGHTS - 1, i = 0; k >= 0; k--, i++)
                light(t, c->lights[k], (int32_t)((uint32_t)origin.x + (uint32_t)(i * dx)),
                      (int32_t)((uint32_t)origin.y + (uint32_t)(i * dy)));
        } else {
            light(t, c->lights[0], at.x, at.y);
        }
    }
    *state = now;
}
