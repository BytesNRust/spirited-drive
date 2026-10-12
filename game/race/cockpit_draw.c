/* The cockpit's per-frame pieces (0042270c's calls). */
#include "game/race/cockpit.h"

#include "game/gfx/blit.h"

#include <stddef.h>

enum { BIG_LIGHT = 2, FIRST_SMALL = 3, SMALL_LIGHTS = 5 };

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
