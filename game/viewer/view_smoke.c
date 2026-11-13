/* port (the car's smoke and the followed car's dust, and the world-to-screen projection), taken out of ported_view.c.
 * ported_view.c calls these; what this file takes of the view is in view_shared.h. */

#include "game/race/car_wheels.h"
#include "game/race/tyre_smoke.h"
#include "game/viewer/screen_layout.h"
#include "game/viewer/weather_hooks.h"
#include "viewer/sky_art_draw.h"
#include "viewer/view_spray.h"
#include "viewer/view_models.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* 00513e88 / 00514298, *004c795c: NULL until the race has its smoke. */
const PuffArt *puff_art;
const PuffBlend *puff_blend;
int32_t (*puff_rand)(void *ctx, int32_t n);
static void *puff_rand_ctx;
CrashTrail *puff_trail;   /* 005146ac */

void ported_view_set_puffs(const PuffArt *art, const PuffBlend *blend, int32_t (*rand_below)(void *ctx, int32_t n),
                           void *ctx, CrashTrail *trail)
{
    puff_art = art, puff_blend = blend, puff_rand = rand_below, puff_rand_ctx = ctx, puff_trail = trail;
}

PuffView puff_view(void)
{
    PuffView v = { object_pixels.target, puff_blend->rows, { 0 }, { 0 }, SKID_HIGH_RES, puff_rand_ctx, puff_rand };
    for (int k = 0; k < PUFF_SHAPES; k++) v.smoke[k] = puff_art->smoke[k], v.dirt[k] = puff_art->dirt[k];
    return v;
}

static void puff_1(void *ctx, int32_t x, int32_t y, int32_t size)
{
    (void)ctx;
    const PuffView v = puff_view();
    puff_smoke(&v, x, y, size);
}

static void puff_other(void *ctx, int32_t x, int32_t y, int32_t size)
{
    (void)ctx;
    const PuffView v = puff_view();
    puff_dirt(&v, x, y, size);
}

/* 004588ac */
static void smoke_puffs(void *ctx, int32_t corner, int32_t size)
{
    (void)ctx;
    const SkidPuffOps ops = { NULL, puff_1, puff_other };
    skid_puffs(&ops, &skid_marks, shadow_race->tick, corner, size);
}

/* 004587ac */
static int32_t smoke_view_angle(void *ctx, const Car *car)
{
    (void)ctx;
    return car_view_angle(car, shadow_race);
}

/* port (rally_gravel.h): the dust of the followed car, a trail (dust_trail.h): puffs born at the rear wheels that stay where they were dropped, low,
 * rising and spreading a little, fading behind it. Each is a smoke puff over a few of the exe's own gravel puffs (puff_dirt, dir1..4), so
 * it reads as grit in a haze; the screen's place is the car's own world-to-screen (render_point). */

int world_screen(const int32_t place[3], int32_t camera[3], int32_t screen[2])
{
    TrackCamera cam = { { 0 }, { 0 }, 0 };
    memcpy(cam.position, model_view.camera_position, sizeof cam.position);
    memcpy(cam.matrix, model_view.camera_matrix, sizeof cam.matrix);
    DrawContext dc = { 0 };
    dc.view = model_view.view, dc.world_shift = WORLD_SHIFT;
    memcpy(dc.near_dir, last_near_dir, sizeof dc.near_dir);
    dc.near = last_near;
    track_point_screen(&cam, &dc, place, camera, screen);
    return camera[2] > 0;
}


/* 00458988 */
void smoke_draw(void *ctx, int in_car)
{
    const TyreSmokeOps ops = { puff_rand_ctx, smoke_view_angle, puff_rand, smoke_puffs };
    tyre_smoke(&ops, (const Car *)ctx, in_car, shadow_race->camera_roll_add);
    const Car *const car = ctx;
    int slot = -1;
    for (int32_t i = 0; i < RACE_CARS && slot < 0; i++)
        if (shadow_race->cars[i] == car) slot = (int)i;
    if (slot >= 0 && (car == followed_car() || (shadow_race->port_rally_road != 0 && port_rally_spray()))) view_spray_dust_draw(slot);   /* port (rally_spray): every car's dust, not only the followed one's */
}
