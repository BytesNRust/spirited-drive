/* port (spray, the dust): the particles the cars throw up and how they are drawn, taken out of ported_view.c: spray v4's glue (its water, gravel, earth and snow) and the rally's dust
 * trail with spray v4 off. What they share with the view (the frame's target and palette, the race, the camera) comes through view_spray.h. */
#include "game/gfx/sprite_scaled.h"
#include "game/viewer/screen_layout.h"
#include "game/race/dust_trail.h"
#include "game/race/rally_gravel.h"
#include "viewer/drive_light.h"
#include "game/race/rain_spray.h"
#include "game/race/spray_pace.h"
#include "game/race/race_sim.h"
#include "game/sys/port_log.h"
#include "viewer/spray_v4_draw.h"
#include "game/viewer/spray_v4_switch.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "viewer/view_spray.h"

static DustTrail dust_trails[RACE_CARS];   /* port (rally_spray): one a car, so a rival's dust is its own; the followed car's is the car's slot too */

enum { DUST_SIZE = 14, DUST_DIRT_SHARE = 1 };   /* a puff's size units per 0x100 of the trail's, and the share (of 4) that gets gravel over its smoke */

/* port (spray v4, dev prototype: TNFS_SPRAY_V4=1): the spray of race/spray_v4.h in place of rain_fx's. Its emitter runs after every simulation tick (race_sim_set_tick_hook), so each wheel's drop has that tick's own place
 * and height whatever the frames drawn; the frame composites the particles by road row (viewer/spray_v4_draw.h). Off: nothing here runs. */
static int spray_ground_colour(const int32_t ground[3], uint8_t rgb[3]);

/* port (spray v4, the dust): the pace a car's spray goes by, its kept one capped by how far it really goes (game/race/spray_pace.h): a traffic car that waits keeps its pace and threw spray from the one spot */
static SprayPace spray_paces[RACE_CARS];

static int32_t spray_car_pace(const RaceState *race, const Car *car, int slot, int32_t tick)
{
    const int32_t kept = rain_spray_speed(car->speed, car->velocity_x, car->velocity_z);
    const Car *const followed = race->cars[race_slot(race->player)];   /* followed_car()'s, from the race given: spray v4's tick runs before the view has a race */
    return car == followed ? kept : spray_pace(&spray_paces[slot], tick, car->x, car->z, kept);   /* the followed car is driven by the player: its pace is real */
}

static int spray_v4_on(void) { return spray_v4_switch_on(); }   /* the mode's spray_v4 (TNFS_SPRAY_V4 over it) as the settings are now: a change with no restart is followed */

static void spray_v4_tick(const RaceState *race)
{
    const int rain = weather_wet(view_weather()) && port_rain_fx();
    const int32_t n = race->car_count < RACE_CARS ? race->car_count : RACE_CARS;
    for (int32_t i = 0; i < n; i++) {
        const Car *car = race->cars[i];
        if (car == NULL) continue;
        int32_t forward[2], wheel[2][3], right[2];
        const int32_t at[3] = { car->x, car->y, car->z };
        rain_spray_heading(car->use_matrix, car->yaw, car->matrix[6], car->matrix[8], forward);   /* as the old emitter: a rival is drawn with its matrix and its yaw stays 0 */
        rain_spray_wheels(at, forward, wheel, right);
        SprayV4Surface surface = SPRAY_V4_NONE;   /* what the wheels run on: the rally's ground, the shoulder, or a wet road */
        switch (race->port_rally_gravel) {
        case RALLY_GRAVEL_DUST:
        case RALLY_GRAVEL_REGOLITH: surface = SPRAY_V4_ON_EARTH; break;
        case RALLY_GRAVEL_SNOW:     surface = SPRAY_V4_ON_SNOW; break;
        case RALLY_GRAVEL_COMPACT:  surface = SPRAY_V4_ON_GRAVEL; break;
        default:                    surface = car->off_road != 0 ? SPRAY_V4_ON_GRAVEL : rain && rain_open_at(car->node) > 0 ? SPRAY_V4_WATER : SPRAY_V4_NONE; break;
        }
        SprayV4Emit e = { (int)i, (uint32_t)race->tick, car->node, (float)spray_car_pace(race, car, (int)i, (int32_t)race->tick) / 65536.0f,
                          car->airborne_ticks == 0, surface, { { 0 } }, { 0 }, { 0 } };
        for (int w = 0; w < 2; w++)
            for (int k = 0; k < 3; k++) e.wheel[w][k] = (float)wheel[w][k] / 65536.0f;
        e.right[0] = (float)right[0] / 65536.0f, e.right[1] = (float)right[1] / 65536.0f;
        e.forward[0] = -e.right[1], e.forward[1] = e.right[0];   /* the heading (right is (fz, -fx)) */
        spray_v4_emit(spray_v4_state(), &e);
    }
}

/* the frame's colours: the water is the scene's fog lifted toward a pale blue-white by day (a wet road's mist is whitish) and a light grey of the fog at night (never lit white); the stones are a dark shade of the fog and
 * the gravel's dust a light dusty one */
static void spray_v4_frame(const TrackArt *art)
{
    static const int PALE[3] = { 236, 240, 246 }, DUSTY[3] = { 205, 198, 184 }, SNOWY[3] = { 250, 251, 255 }, LIFTED[3] = { 250, 232, 205 };
    static uint8_t ground[3] = { 176, 124, 84 };   /* the desert's: sampled from the road under the followed car when it is on the screen, the last one else */
    uint8_t fog[3], rgb[SPRAY_V4_KINDS][3];
    modern_look_fog_colour(art, fog);
    const int night = drive_light_night();
    const Car *const car = shadow_race != NULL ? followed_car() : NULL;
    if (car != NULL && shadow_race->port_rally_gravel != RALLY_GRAVEL_OFF) {
        const int32_t at[3] = { car->x, car->y, car->z };
        uint8_t sampled[3];
        if (spray_ground_colour(at, sampled)) memcpy(ground, sampled, 3);
    }
    for (int k = 0; k < 3; k++) {
        const int water = night ? fog[k] * 5 / 10 + 78 : (fog[k] * 30 + PALE[k] * 70) / 100, dust = night ? fog[k] * 5 / 10 + 62 : (fog[k] * 35 + DUSTY[k] * 65) / 100;
        const int earth = night ? ground[k] * 6 / 10 + fog[k] * 2 / 10 : ((ground[k] * 40 + LIFTED[k] * 60) / 100) * 85 / 100 + fog[k] * 15 / 100;   /* the road's own colour lifted toward a pale dust and hazed: lighter than the ground, so the cloud shows against it */
        const int snow = night ? fog[k] * 5 / 10 + 85 : (fog[k] * 25 + SNOWY[k] * 75) / 100;
        rgb[SPRAY_V4_MIST][k] = rgb[SPRAY_V4_DROP][k] = (uint8_t)water;
        rgb[SPRAY_V4_DUST][k] = (uint8_t)dust;
        rgb[SPRAY_V4_GRAVEL][k] = (uint8_t)((night ? fog[k] * 6 / 10 : fog[k]) * 30 / 100);
        rgb[SPRAY_V4_EARTH][k] = (uint8_t)(earth > 255 ? 255 : earth);
        rgb[SPRAY_V4_SNOW][k] = rgb[SPRAY_V4_CLUMP][k] = (uint8_t)snow;
    }
    const SprayV4Host host = { object_pixels.target, object_pixels.palette, &model_view, shadow_race, world_screen, rain_open_at };
    spray_v4_draw_frame(&host, rgb);
}

void ported_view_trail_stats(int *dust)
{
    *dust = 0;
    if (shadow_race == NULL) return;
    const int slot = race_slot(shadow_race->player);
    uint64_t dust_ticks = 0;
    if (slot >= 0 && slot < RACE_CARS)
        for (int i = 0; i < DUST_TRAIL_PUFFS; i++) {
            const DustPuff *d = &dust_trails[slot].puff[i];
            int32_t place[3], size;
            if (dust_puff_at(d, shadow_race->tick, shadow_race->port_rally_gravel, shadow_race->port_lunar_gravity != 0, place, &size) && shadow_race->tick - d->born < 64)
                dust_ticks |= (uint64_t)1 << (shadow_race->tick - d->born);
        }
    for (int b = 0; b < 64; b++) *dust += (int)(dust_ticks >> b & 1);
}

static int spray_ground_colour(const int32_t ground[3], uint8_t rgb[3])
{
    int32_t camera[3], screen[2];
    if (!world_screen(ground, camera, screen) || camera[2] < 64) return 0;
    const QuadTarget *t = object_pixels.target;
    int sum[3] = { 0, 0, 0 }, n = 0;
    for (int dy = 0; dy <= 4; dy += 4)
        for (int dx = -8; dx <= 8; dx += 8) {
            const int x = screen[0] + dx, y = screen[1] + dy;
            if (x < t->clip_left || x >= t->clip_right || y < t->clip_top || y >= t->clip_bottom) continue;
            const uint8_t i = t->pixels[(size_t)y * (size_t)t->pitch + (size_t)x];
            if (i == 0xFF) continue;
            for (int k = 0; k < 3; k++) sum[k] += object_pixels.palette[i * 3 + k];
            n++;
        }
    if (n == 0) return 0;
    for (int k = 0; k < 3; k++) rgb[k] = (uint8_t)(sum[k] / n);
    return 1;
}

static void dust_puffs(int slot);

/* port (cam_trail): a car's dust trail is stepped every frame for every car that throws dust, drawn or not (the body's smoke stepped it, so a camera that does not draw the car, or a car off the screen,
 * froze its trail and the next view laid the missed ticks again along the heading it has now) */
static void dust_step(const Car *car, int slot)
{
    const int kind = shadow_race->port_rally_gravel;
    DustTrail *const trail = &dust_trails[slot];
    int32_t yaw = car->yaw, speed = car->speed;
    if (car != followed_car()) {   /* port (rally_spray): a rival's yaw stays 0 (it is drawn with its matrix) and the traffic's speed too: heading from the matrix, pace from the speed or the velocity */
        int32_t forward[2];
        rain_spray_heading(car->use_matrix, car->yaw, car->matrix[6], car->matrix[8], forward);
        yaw = (int32_t)((uint32_t)((int32_t)lrint(atan2((double)forward[0], (double)forward[1]) / (2.0 * M_PI) * 65536.0) & 0xffff) << 8);
        speed = spray_car_pace(shadow_race, car, slot, (int32_t)shadow_race->tick);
    }
    const int drop = rally_dust_skid(kind, speed, car->airborne_ticks == 0) != 0;
    const int32_t at[3] = { car->x, car->y, car->z };
    dust_trail_step(trail, shadow_race->tick, kind, drop, at, yaw, speed);
}

static void dust_update(void)
{
    if (shadow_race == NULL) return;
    const int32_t n = shadow_race->car_count < RACE_CARS ? shadow_race->car_count : RACE_CARS;
    for (int32_t i = 0; i < n; i++) {
        const Car *car = shadow_race->cars[i];
        if (car != NULL && (car == followed_car() || (shadow_race->port_rally_road != 0 && port_rally_spray()))) dust_step(car, (int)i);
    }
}

void view_spray_dust_draw(int slot)
{
    if (spray_v4_on()) return;   /* port (spray v4): the dust is the spray's earth */
    dust_puffs(slot);
}

/* the puffs of a car's trail, all of them after the body (the exe's order) */
static void dust_puffs(int slot)
{
    const int kind = shadow_race->port_rally_gravel;
    DustTrail *const trail = &dust_trails[slot];
    if (!dust_trail_started(trail)) return;   /* no history yet: its zeroed puffs are not puffs */
    if (kind != RALLY_GRAVEL_DUST && kind != RALLY_GRAVEL_REGOLITH && kind != RALLY_GRAVEL_SNOW) return;
    const PuffView v = puff_view();
    const int lunar = shadow_race->port_lunar_gravity != 0;
    for (int n = 0; n < DUST_TRAIL_PUFFS; n++) {   /* the ring's oldest first: the farthest from a camera behind the car */
        const DustPuff *p = &trail->puff[(trail->next + n) % DUST_TRAIL_PUFFS];
        int32_t place[3], size, camera[3], screen[2];
        if (!dust_puff_at(p, shadow_race->tick, kind, lunar, place, &size)) continue;
        if (!world_screen(place, camera, screen) || camera[2] < 64) continue;
        const int32_t depth = 0x38400 / camera[2];
        const int32_t scale = depth * size * DUST_SIZE / 0x100 * 8;
        puff_smoke(&v, screen[0], screen[1], scale);
        if (kind != RALLY_GRAVEL_SNOW && (p->seed & 3) < DUST_DIRT_SHARE) {   /* port (snow_weather): the powder is white smoke, with no grit in it */
            const int32_t dx = (p->seed >> 2 & 0x1f) - 0x10, dy = (p->seed >> 7 & 0xf) - 8;
            puff_dirt(&v, screen[0] + dx * depth / 0x1000, screen[1] + dy * depth / 0x1000, scale / 2);
        }
    }
}

/* 00490570 with *004c55e0 ('smok') */
void view_spray_trail_puff(void *ctx, int32_t scale, int32_t x, int32_t y)
{
    (void)ctx;
    sprite_scaled_blend_at(object_pixels.target, puff_blend->rows, puff_art->smok, scale, x, y);
}

/* the frame's setup, once the 3D's tables are made: the dust's trails stepped, spray v4's tick hook and colours */
void view_spray_frame(const TrackArt *art)
{
    dust_update();   /* port (cam_trail) */
    static int logged = -1;
    if (spray_v4_on() != logged) port_log("mark:spray_v4=%d\n", logged = spray_v4_on());   /* port: a headless run can tell which dust a race draws */
    spray_v4_switch_hook(spray_v4_tick);   /* port (spray v4): its emitter while it is on, none left behind when it is off */
    if (spray_v4_on()) spray_v4_frame(art);
}

void view_spray_v4_hook(void)
{
    spray_v4_switch_hook(spray_v4_tick);
}

/* a road row's spray and dust: far_side 1 the puffs farther off than their car (before the row's cars), 0 the nearer (after) */
void view_spray_row(int32_t node, int far_side)
{
    if (spray_v4_on()) spray_v4_draw_row(node, far_side);
}
