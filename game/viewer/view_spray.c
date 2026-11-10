/* port (spray, the dust, the water mist, the CD's smoke): the particles the cars throw up and how they are drawn, taken out of ported_view.c: rain_fx's spray (race/rain_spray.h), the water mist, the rally's dust trail, the CD smoke
 * sprites and spray v4's glue. What they share with the view (the frame's target and palette, the race, the camera) comes through view_spray.h. */
#include "game/gfx/sprite_scaled.h"
#include "game/race/spray_tint.h"
#include "game/race/trail_rise.h"
#include "game/race/water_mist.h"
#include "game/viewer/screen_layout.h"
#include "game/race/dust_trail.h"
#include "game/race/rally_gravel.h"
#include "viewer/drive_light.h"
#include "game/race/rain_spray.h"
#include "game/race/spray_pace.h"
#include "game/race/race_sim.h"
#include "viewer/spray_v4_draw.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "viewer/view_spray.h"

static DustTrail dust_trails[RACE_CARS];   /* port (rally_spray): one a car, so a rival's dust is its own; the followed car's is the car's slot too */
static int trail_rise_on;   /* port (trail_rise): the frame's switch, read where the frame is set up */

enum { DUST_SIZE = 14, DUST_DIRT_SHARE = 1 };   /* a puff's size units per 0x100 of the trail's, and the share (of 4) that gets gravel over its smoke */

/* port (spray v4, dev prototype: TNFS_SPRAY_V4=1): the spray of race/spray_v4.h in place of rain_fx's. Its emitter runs after every simulation tick (race_sim_set_tick_hook), so each wheel's drop has that tick's own place
 * and height whatever the frames drawn; the frame composites the particles by road row (viewer/spray_v4_draw.h). Off: nothing here runs. */
static int spray_ground_colour(const int32_t ground[3], uint8_t rgb[3]);

/* port (traffic_spray): the pace a car's spray goes by, its kept one capped by how far it really goes (game/race/spray_pace.h): a traffic car that waits keeps its pace and threw spray from the one spot */
static SprayPace spray_paces[RACE_CARS];

static int32_t spray_car_pace(const Car *car, int slot, int32_t tick)
{
    const int32_t kept = rain_spray_speed(car->speed, car->velocity_x, car->velocity_z);
    return car == followed_car() ? kept : spray_pace(&spray_paces[slot], tick, car->x, car->z, kept);   /* the followed car is driven by the player: its pace is real */
}

static int spray_v4_on(void)
{
    static int on = -1;
    if (on < 0) on = port_spray_v4() != 0;   /* port.cfg's spray_v4, TNFS_SPRAY_V4 over it */
    return on;
}

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
        SprayV4Emit e = { (int)i, (uint32_t)race->tick, car->node, (float)spray_car_pace(car, (int)i, (int32_t)race->tick) / 65536.0f,
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

/* port (rain_fx, rain v5): the water the cars throw up, as particles of the world (race/rain_spray.h): each wheel drops one a tick where it passes and it stays there, a low mist that widens
 * and thins for two thirds of a second. A particle is drawn when the draw of the road reaches the node it was dropped at, with the cars of that row (draw_cars), a grainy dithered sprite
 * through the recorder like the tyre smoke: so the scenery nearer than it covers it, and the cars, the player's own too. A shape's texels pick a table: 1 lightens, 2 less, 3 darkens (grit). */
static RainSpray rain_spray;
static int rain_spray_on;   /* the frame draws rain_fx's spray */
static int rally_spray_on;  /* port (rally_spray): in a rally the spray is every car's and takes the road's colour */
enum { SPRAY_SIDE = 64, SPRAY_VARIANTS = 3, SPRAY_SHAPE_BYTES = 16 + SPRAY_SIDE * SPRAY_SIDE };
static uint8_t spray_shape[RAIN_SPRAY_LEVELS][SPRAY_VARIANTS][SPRAY_SHAPE_BYTES];
static uint8_t spray_identity[256];
static const uint8_t *spray_rows[256];

static void spray_shapes_make(void)
{
    static int done;
    if (done) return;
    done = 1;
    for (int i = 0; i < 256; i++) spray_identity[i] = (uint8_t)i;
    static const int DENSITY[RAIN_SPRAY_LEVELS] = { 26, 46, 66, 86 };   /* percent at the heart */
    for (int l = 0; l < RAIN_SPRAY_LEVELS; l++)
        for (int v = 0; v < SPRAY_VARIANTS; v++) {
            uint8_t *sh = spray_shape[l][v];
            sh[0] = 0x7b, sh[4] = SPRAY_SIDE & 0xff, sh[5] = SPRAY_SIDE >> 8, sh[6] = SPRAY_SIDE & 0xff, sh[7] = SPRAY_SIDE >> 8;
            for (int y = 0; y < SPRAY_SIDE; y++)
                for (int x = 0; x < SPRAY_SIDE; x++) {
                    const float dx = ((float)x - 31.5f) / 32.0f, dy = ((float)y - 31.5f) / 32.0f, r2 = dx * dx + dy * dy;
                    const uint32_t h = rain_hash((uint32_t)(y * SPRAY_SIDE + x), (uint32_t)(l * 17 + v * 101 + 5));
                    uint8_t t = 0;
                    if (r2 < 1.0f && (int)(h % 100u) < (int)((float)DENSITY[l] * (1.0f - 0.85f * r2))) t = (h >> 12) % 5 == 0 ? 3 : (h >> 12) % 3 == 0 ? 2 : 1;   /* thicker at the heart */
                    sh[16 + y * SPRAY_SIDE + x] = t;
                }
        }
}

static void spray_tables_set(const WeatherTables *wt)
{
    spray_shapes_make();
    const int level = weather_rain_spray_level(drive_light_night());
    for (int i = 0; i < 256; i++) spray_rows[i] = spray_identity;
    spray_rows[1] = wt->rain4[level], spray_rows[2] = wt->rain4[level > 0 ? level - 1 : 0], spray_rows[3] = wt->rain4[level > 1 ? level - 2 : 0];
}

/* port (water_mist, 1..3): the rain's spray as a soft, low, wide veil (water_mist.h), in place of the speckle and of the CD's smoke; a dirt road's keeps what it had */
static int water_mist_variant;
static uint8_t water_mist_shape_data[WATER_MIST_BYTES];
static uint8_t water_mist_table[WATER_MIST_VARIANTS][WATER_MIST_DENSITIES][WATER_MIST_LEVELS + 1][256];
static const uint8_t *water_mist_rows[256];
static uint8_t water_mist_palette[768];
static uint8_t water_streak_table[WATER_MIST_LEVELS + 1][256];
static const uint8_t *water_streak_rows[256];
static uint8_t water_streak_shape[2][WATER_STREAK_BYTES];
static int water_mist_made;

static const uint8_t *const *water_mist_rows_of(int variant, int density)
{
    if (!water_mist_made || memcmp(water_mist_palette, object_pixels.palette, sizeof water_mist_palette) != 0) {
        memcpy(water_mist_palette, object_pixels.palette, sizeof water_mist_palette);
        water_mist_shape(water_mist_shape_data);
        for (int v = 1; v <= WATER_MIST_VARIANTS; v++) {
            uint8_t rgb[3];
            water_mist_colour(v, rgb);
            for (int d = 0; d < WATER_MIST_DENSITIES; d++)
                for (int k = 0; k <= WATER_MIST_LEVELS; k++)
                    spray_add_table(object_pixels.palette, rgb, water_mist_amount(v, d) * k / WATER_MIST_LEVELS, water_mist_table[v - 1][d][k]);
        }
        const uint8_t white[3] = { 242, 246, 252 };
        for (int k = 0; k <= WATER_MIST_LEVELS; k++) spray_add_table(object_pixels.palette, white, water_mist_streak_amount() * k / WATER_MIST_LEVELS, water_streak_table[k]);
        water_mist_streak_shape(water_streak_shape[0], 0), water_mist_streak_shape(water_streak_shape[1], 1);
        water_mist_made = 1;
    }
    for (int i = 0; i < 256; i++) water_mist_rows[i] = water_mist_table[variant - 1][density][0];
    for (int k = 0; k <= WATER_MIST_LEVELS; k++) water_mist_rows[k] = water_mist_table[variant - 1][density][k];
    return water_mist_rows;
}

/* the droplet streaks of a young particle: a sprite of thin bright lines, one of two leans by the particle's side, drawn with the veil */
static void water_streaks_draw(const RainSprayParticle *p, uint32_t tick, int slot, int32_t cx, int32_t cy, int32_t px)
{
    if (tick - p->born > 5u || px < 3 || ((uint32_t)slot + p->born) % 4u != 0) return;   /* the young, some of them */
    water_mist_rows_of(1, 0);   /* the tables, made when wanted */
    for (int i = 0; i < 256; i++) water_streak_rows[i] = water_streak_table[0];
    for (int k = 0; k <= WATER_MIST_LEVELS; k++) water_streak_rows[k] = water_streak_table[k];
    const int32_t sw = px * 2 + 4;
    sprite_scaled_blend(object_pixels.target, water_streak_rows, water_streak_shape[p->drift_x + p->drift_z < 0 ? 0 : 1], sw, sw, cx - sw / 2, cy - sw + sw / 5);
}

void ported_view_trail_stats(int *rain, int *dust)
{
    *rain = *dust = 0;
    if (shadow_race == NULL) return;
    const int slot = race_slot(shadow_race->player);
    uint64_t rain_ticks = 0, dust_ticks = 0;
    const uint32_t now = (uint32_t)shadow_race->tick;
    for (int i = 0; i < RAIN_SPRAY_MAX; i++) {
        const RainSprayParticle *p = &rain_spray.p[i];
        if (p->live && p->car == slot && now >= p->born && now - p->born < p->life && now - p->born < 64u) rain_ticks |= (uint64_t)1 << (now - p->born);
    }
    if (slot >= 0 && slot < RACE_CARS)
        for (int i = 0; i < DUST_TRAIL_PUFFS; i++) {
            const DustPuff *d = &dust_trails[slot].puff[i];
            int32_t place[3], size;
            if (dust_puff_at(d, shadow_race->tick, shadow_race->port_rally_gravel, shadow_race->port_lunar_gravity != 0, place, &size) && shadow_race->tick - d->born < 64)
                dust_ticks |= (uint64_t)1 << (shadow_race->tick - d->born);
        }
    for (int b = 0; b < 64; b++) *rain += (int)(rain_ticks >> b & 1), *dust += (int)(dust_ticks >> b & 1);
}

/* once a tick, each car's wheels drop their water */
static void spray_update(void)
{
    if (shadow_race == NULL) return;
    const Car *player = followed_car();
    const int32_t n = shadow_race->car_count < RACE_CARS ? shadow_race->car_count : RACE_CARS;
    for (int32_t i = 0; i < n; i++) {
        const Car *car = shadow_race->cars[i];
        if (car == NULL) continue;
        int32_t forward[2], wheel[2][3], right[2];   /* port (cam_trail): every car, drawn or not (a car off the screen, or a view that does not draw it, kept no trail and left a gap) */
        const int32_t at[3] = { car->x, car->y, car->z };
        rain_spray_heading(car->use_matrix, car->yaw, car->matrix[6], car->matrix[8], forward);   /* a rival is drawn with its matrix and its yaw stays 0 */
        rain_spray_wheels(at, forward, wheel, right);
        const int wet = car->airborne_ticks == 0 && (car->off_road == 0 || rally_spray_on) && rain_open_at(car->node) > 0;   /* port (tunnel_dry): no water inside a closed zone; port (rally_spray): a rally's road is all off_road (the player's never threw any) */
        const int32_t pace = spray_car_pace(car, (int)i, (int32_t)shadow_race->tick);   /* the traffic's physics does not run: its speed is 0 while it drives */
        if (water_mist_variant > 0 && !rally_spray_on)   /* port (water_mist): every car's water is the plume, the traffic's too (a wisp of three ticks is no veil) */
            rain_spray_step_dense(&rain_spray, (int)i, (uint32_t)shadow_race->tick, wheel[0], wheel[1], right, car->node, pace, wet);
        else if (port_traffic_spray() && port_rain_v2() && i >= shadow_race->racers && car != player)   /* port (rain_v2): a wisp from each wheel, a little longer, at any pace */
            rain_spray_step_wisp2(&rain_spray, (int)i, (uint32_t)shadow_race->tick, wheel[0], wheel[1], right, car->node, pace, wet);
        else if (port_traffic_spray() && i >= shadow_race->racers && car != player)   /* port (traffic_spray): the cops' and traffic's water is a short wisp */
            rain_spray_step_wisp(&rain_spray, (int)i, (uint32_t)shadow_race->tick, wheel[0], wheel[1], right, car->node, pace, wet);
        else
            rain_spray_step(&rain_spray, (int)i, (uint32_t)shadow_race->tick, wheel[0], wheel[1], right, car->node, pace, wet);
    }
}

/* port (rally_spray): the spray's table in the colour of the road it flies from: the pixels the road was drawn with, around the particle's ground point, their mean (spray_tint.h makes the colour and the table;
 * the tables are kept by the colour's key and made again when the palette changes) */
enum { SPRAY_TINTS = 24 };
static struct { uint32_t key; int used; uint8_t row[3][256]; } spray_tints[SPRAY_TINTS];
static int spray_tint_next;
static uint8_t spray_tint_palette[768];
static const uint8_t *spray_tint_rows[256];

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

static int spray_road_colour(const RainSprayParticle *p, uint8_t rgb[3])
{
    const int32_t ground[3] = { p->x, p->y, p->z };
    return spray_ground_colour(ground, rgb);
}

/* the rows of a particle's tint (the heart, the thinner part, the grit), or NULL when the road's colour is not to be had */
static const uint8_t *const *spray_tinted_rows(const RainSprayParticle *p)
{
    static const int SHARE[3] = { 52, 34, 70 };   /* percent toward the tint: the sprite's texels 1, 2 and 3 (grit darkest) */
    uint8_t road[3], tint[3];
    if (!spray_road_colour(p, road)) return NULL;
    if (memcmp(spray_tint_palette, object_pixels.palette, sizeof spray_tint_palette) != 0) {
        memcpy(spray_tint_palette, object_pixels.palette, sizeof spray_tint_palette);
        memset(spray_tints, 0, sizeof spray_tints);
    }
    const uint32_t key = spray_tint_key(road);
    int at = -1;
    for (int i = 0; i < SPRAY_TINTS; i++)
        if (spray_tints[i].used && spray_tints[i].key == key) at = i;
    if (at < 0) {
        at = spray_tint_next++ % SPRAY_TINTS;
        spray_tint_target(road, tint);
        for (int k = 0; k < 3; k++) spray_tint_table(object_pixels.palette, tint, SHARE[k], spray_tints[at].row[k]);
        spray_tints[at].key = key, spray_tints[at].used = 1;
    }
    for (int i = 0; i < 256; i++) spray_tint_rows[i] = spray_identity;
    for (int k = 0; k < 3; k++) spray_tint_rows[1 + k] = spray_tints[at].row[k];
    return spray_tint_rows;
}

/* port (spray_cd_smoke): the CD's smoke sprite ('smok', SMOKE.QFS) as the spray's and the dust's mist: the exe's additive way (the sprite's grey lifts what is under it), through tables of ours that add
 * the surface's colour (spray_tint.h: white for water and snow, the road's own hue lifted for mud and dust), more transparent than the exe's; and, for water and mud, a few fine droplets by the wheel, stretched along the spray. */
enum { MIST_TINTS = 12, MIST_DENSITIES = RAIN_SPRAY_LEVELS, MIST_SHAPE_BYTES = 16 + 128 * 128 };
typedef struct {
    uint32_t key;
    int used, kind;
    uint8_t made[MIST_DENSITIES];
    uint8_t row[MIST_DENSITIES][SPRAY_MIST_LEVELS + 1][256];   /* a table for each level of the sprite's grey, by how thick the particle is */
    uint8_t drop[256];                                         /* the droplets' table: lighter (water) or darker (mud) */
    uint8_t streak[WATER_MIST_LEVELS + 1][256];                /* the droplet streaks' tables by level (water lighter, mud darker), made when first wanted */
    int streak_made;
    uint8_t colour[3], road[3];
} MistTint;
static MistTint mist_tints[MIST_TINTS];
static int mist_tint_next, spray_cd_on, spray_cd_dir;   /* spray_cd_dir: the setting's 2, the CD's gravel puffs (dir1..4) in place of the cloud on dust and for the mud by the wheel */
static uint8_t mist_shape[MIST_SHAPE_BYTES], mist_palette[768];
static const uint8_t *mist_made_from;
static const uint8_t *mist_rows[256];

/* the sprite's levels, made once per race's sprite */
static const uint8_t *mist_sprite(void)
{
    if (puff_art == NULL || puff_art->smok == NULL || puff_blend == NULL) return NULL;
    if (mist_made_from != puff_art->smok) {
        uint8_t grey[256];
        for (int i = 0; i < 256; i++) grey[i] = (uint8_t)((puff_blend->palette[4 * i] + puff_blend->palette[4 * i + 1] + puff_blend->palette[4 * i + 2]) / 3);
        mist_made_from = spray_mist_shape(puff_art->smok, grey, mist_shape, sizeof mist_shape) ? puff_art->smok : NULL;
        if (mist_made_from == NULL) return NULL;
    }
    return mist_shape;
}

static MistTint *mist_tint(const uint8_t road[3])
{
    if (memcmp(mist_palette, object_pixels.palette, sizeof mist_palette) != 0) {
        memcpy(mist_palette, object_pixels.palette, sizeof mist_palette);
        memset(mist_tints, 0, sizeof mist_tints);
    }
    const uint32_t key = spray_tint_key(road);
    for (int i = 0; i < MIST_TINTS; i++)
        if (mist_tints[i].used && mist_tints[i].key == key) return &mist_tints[i];
    MistTint *m = &mist_tints[mist_tint_next++ % MIST_TINTS];
    memset(m, 0, sizeof *m);
    m->key = key, m->used = 1, m->kind = spray_mist_kind(road);
    spray_mist_colour(road, m->colour);
    memcpy(m->road, road, 3);
    uint8_t dark[3];
    for (int k = 0; k < 3; k++) dark[k] = (uint8_t)(road[k] * 40 / 100);
    if (m->kind == SPRAY_MIST_MUD) spray_tint_table(object_pixels.palette, dark, 78, m->drop);   /* a mud droplet is darker than the road */
    else spray_add_table(object_pixels.palette, m->colour, 255, m->drop);                        /* a water droplet catches the light */
    return m;
}

/* the rows of a thickness 0..3: the sprite's level k adds MAX x k / levels of the colour; made when first wanted */
static const uint8_t *const *mist_rows_of(MistTint *m, int density)
{
    static const int MAX_ADD[MIST_DENSITIES] = { 110, 150, 190, 230 }, MAX_ADD_MUD[MIST_DENSITIES] = { 55, 80, 105, 130 };   /* water's white reads as strongly as the speckles; the mud's and the dust's as they were */   /* of 255 at the sprite's whitest: far less than the exe's puff */
    if (density < 0) density = 0;
    if (density >= MIST_DENSITIES) density = MIST_DENSITIES - 1;
    if (!m->made[density]) {
        for (int k = 0; k <= SPRAY_MIST_LEVELS; k++) spray_add_table(object_pixels.palette, m->colour, (m->kind == SPRAY_MIST_MUD ? MAX_ADD_MUD : MAX_ADD)[density] * k / SPRAY_MIST_LEVELS, m->row[density][k]);
        const int road_warm = m->road[0] - m->road[2];   /* on a warm ground (dirt, dust) a cool colour is the car's body, not the ground: the beige is not drawn over it, so it does not turn grey-blue */
        for (int i = 0; i < 255; i++) {
            const int warm = object_pixels.palette[i * 3] - object_pixels.palette[i * 3 + 2];
            if (road_warm > 20 && warm < 0)
                for (int k = 0; k <= SPRAY_MIST_LEVELS; k++) m->row[density][k][i] = (uint8_t)i;
        }
        m->made[density] = 1;
    }
    for (int i = 0; i < 256; i++) mist_rows[i] = m->row[density][0];
    for (int k = 0; k <= SPRAY_MIST_LEVELS; k++) mist_rows[k] = m->row[density][k];
    return mist_rows;
}

/* the CD's gravel sprites ('dir1'..'dir4', opaque) with their colours moved toward the road's, made once for a road colour */
enum { DIR_TINTS = 6, DIR_BYTES = 16 + 75 * 38 + 16 };
typedef struct { uint32_t key; int used; uint8_t shape[PUFF_SHAPES][DIR_BYTES]; } DirTint;
static DirTint dir_tints[DIR_TINTS];
static int dir_tint_next;
static uint8_t dir_palette[768];

static void dir_draw(const uint8_t road[3], uint32_t seed, int32_t cx, int32_t cy, int32_t scale)
{
    if (puff_art == NULL || scale < 0x400) return;
    if (memcmp(dir_palette, object_pixels.palette, sizeof dir_palette) != 0) {
        memcpy(dir_palette, object_pixels.palette, sizeof dir_palette);
        memset(dir_tints, 0, sizeof dir_tints);
    }
    const uint32_t key = spray_tint_key(road);
    DirTint *d = NULL;
    for (int i = 0; i < DIR_TINTS && d == NULL; i++)
        if (dir_tints[i].used && dir_tints[i].key == key) d = &dir_tints[i];
    if (d == NULL) {
        d = &dir_tints[dir_tint_next++ % DIR_TINTS];
        memset(d, 0, sizeof *d);
        uint8_t table[256];
        spray_tint_table(object_pixels.palette, road, 80, table);
        for (int k = 0; k < PUFF_SHAPES; k++) {
            const uint8_t *src = puff_art->dirt[k];
            if (src == NULL || src[0] != 0x7b) continue;
            const size_t w = (size_t)(src[4] | src[5] << 8), h = (size_t)(src[6] | src[7] << 8);
            if (16 + w * h > DIR_BYTES) continue;
            memcpy(d->shape[k], src, 16);
            for (size_t i = 0; i < w * h; i++) d->shape[k][16 + i] = src[16 + i] == 0xFF ? 0xFF : table[src[16 + i]];
        }
        d->key = key, d->used = 1;
    }
    const int pick = (int)(seed % 3u);
    if (d->shape[pick][0] != 0x7b) return;
    sprite_scaled_at(object_pixels.target, d->shape[pick], scale > 0x10000 ? 0x10000 : scale, cx, cy);
}

static void mist_draw(const uint8_t road[3], int density, int32_t cx, int32_t cy, int32_t w, int32_t h, uint32_t seed)
{
    const uint8_t *shape = mist_sprite();
    if (shape == NULL || w < 3 || h < 2) return;
    MistTint *m = mist_tint(road);
    const int32_t ww = seed & 1 ? -w : w, hh = seed & 2 ? -h : h;   /* the sprite mirrored by the seed: one cloud, four looks */
    sprite_scaled_blend(object_pixels.target, mist_rows_of(m, density), shape, ww, hh, cx - w / 2, cy - h / 2);
}

/* a few fine droplets thrown along the spray by a young particle (water and mud only), a streak each, stretched along (dx, dy) on the screen */
static void mist_droplets(const uint8_t road[3], const RainSprayParticle *p, uint32_t tick, int slot, int32_t cx, int32_t cy, int32_t px, int dx, int dy)
{
    if (tick - p->born > 8u || px < 2) return;
    MistTint *m = mist_tint(road);
    if (m->kind == SPRAY_MIST_SNOW) return;
    if (spray_cd_dir && m->kind == SPRAY_MIST_MUD) {   /* the mud thrown by the wheel: the CD's gravel puffs in the road's colour, a few and small */
        if ((slot & 1) == 0) dir_draw(road, p->born * 5u + (uint32_t)slot, cx + (int)(rain_hash((uint32_t)slot, p->born) % (uint32_t)(px + 1)) - px / 2, cy, (int32_t)px * 0x10000 / 40 + 0x1000);
        return;
    }
    (void)dx, (void)dy;   /* the streaks are a sprite of thin lines (the pixels set in the target here were overwritten when the road is replayed by bands) */
    if (((uint32_t)slot + p->born) % 2u != 0) return;   /* the young, some of them */
    water_mist_rows_of(1, 0);   /* the streak sprites, made when wanted */
    if (!m->streak_made) {
        uint8_t dark[3];
        for (int k = 0; k < 3; k++) dark[k] = (uint8_t)(road[k] * 40 / 100);
        for (int k = 0; k <= WATER_MIST_LEVELS; k++) {
            if (m->kind == SPRAY_MIST_MUD) spray_tint_table(object_pixels.palette, dark, 78 * k / WATER_MIST_LEVELS, m->streak[k]);
            else spray_add_table(object_pixels.palette, m->colour, 255 * k / WATER_MIST_LEVELS, m->streak[k]);
        }
        m->streak_made = 1;
    }
    for (int i = 0; i < 256; i++) water_streak_rows[i] = m->streak[0];
    for (int k = 0; k <= WATER_MIST_LEVELS; k++) water_streak_rows[k] = m->streak[k];
    const int32_t sw = px * 2 + 4;
    sprite_scaled_blend(object_pixels.target, water_streak_rows, water_streak_shape[p->drift_x + p->drift_z < 0 ? 0 : 1], sw, sw, cx - sw / 2, cy - sw + sw / 5);
}

/* the particles dropped at road node `node`, drawn with that row's cars */
static void spray_row(int32_t node, int far_side)
{
    const uint32_t tick = (uint32_t)shadow_race->tick;
    const int32_t focal = abs(model_view.view->focal_x);
    const int open = rain_open_at(node);   /* port (tunnel_dry): the openness where the water was dropped, not where the eye is */
    for (int i = 0; i < RAIN_SPRAY_MAX; i++) {
        const RainSprayParticle *p = &rain_spray.p[i];
        int32_t place[3], radius, camera[3], screen[2];
        int density;
        if (!p->live || p->node != node || !rain_spray_look(p, tick, open, trail_rise_on && (water_mist_variant == 0 || rally_spray_on), place, &radius, &density)) continue;   /* port (water_mist): the veil stays low */
        if (!world_screen(place, camera, screen) || camera[2] < 64) continue;
        const Car *owner = p->car < RACE_CARS ? shadow_race->cars[p->car] : NULL;   /* a puff farther from the eye than its car is drawn before the row's cars, a nearer one after them */
        int32_t own_camera[3], own_screen[2];
        const int32_t own_at[3] = { owner != NULL ? owner->x : 0, owner != NULL ? owner->y : 0, owner != NULL ? owner->z : 0 };
        const int farther = owner == NULL || !world_screen(own_at, own_camera, own_screen) || camera[2] >= own_camera[2];
        if (farther != far_side) continue;
        const int32_t px = (int32_t)((int64_t)(radius >> VIEW_WORLD_SHIFT) * focal / camera[2]);   /* the radius in pixels */
        const int32_t w = px * 2 + 3, h = w * 6 / 11 + 2;   /* flatter than wide: a mist along the road */
        uint8_t ground_rgb[3];
        if (water_mist_variant > 0 && !rally_spray_on) {   /* port (water_mist) */
            const int32_t mw = w * water_mist_width_pct(water_mist_variant) / 100, mh = mw * water_mist_height_pct(water_mist_variant) / 100 + 2;
            if (mw >= 3) sprite_scaled_blend(object_pixels.target, water_mist_rows_of(water_mist_variant, density), water_mist_shape_data, mw, mh, screen[0] - mw / 2, screen[1] - mh / 2);
            if (water_mist_streaks(water_mist_variant)) water_streaks_draw(p, tick, i, screen[0], screen[1], px);
            continue;
        }
        if (spray_cd_on && spray_road_colour(p, ground_rgb)) {   /* port (spray_cd_smoke): the CD's cloud and a few droplets */
            int32_t ahead[3] = { place[0] + p->drift_x, place[1], place[2] + p->drift_z }, ahead_camera[3], ahead_screen[2];
            const int have = world_screen(ahead, ahead_camera, ahead_screen);
            mist_draw(ground_rgb, density, screen[0], screen[1], w, w * 3 / 5 + 2, p->born * 7u + (uint32_t)i);
            mist_droplets(ground_rgb, p, tick, i, screen[0], screen[1], px, have ? ahead_screen[0] - screen[0] : 0, have ? ahead_screen[1] - screen[1] : -1);
            continue;
        }
        const uint8_t *const *rows = rally_spray_on ? spray_tinted_rows(p) : NULL;   /* port (rally_spray) */
        sprite_scaled_blend(object_pixels.target, rows != NULL ? rows : spray_rows, spray_shape[density][(p->born * 7u + (uint32_t)i) % SPRAY_VARIANTS], w, h, screen[0] - w / 2, screen[1] - h / 2);
    }
}

enum { DUST_BODY_REAR = 60 };   /* a puff within the car's rear (its half length and a puff's radius, in the camera's depth units) is behind the body, not in front of it */
static void dust_puffs(const Car *car, int slot, int pass);

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
        speed = spray_car_pace(car, slot, (int32_t)shadow_race->tick);
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

void view_spray_dust_draw(const Car *car, int slot)
{
    if (spray_v4_on()) return;   /* port (spray v4): the dust is the spray's earth */
    if (spray_cd_on) return;   /* port (spray_cd_smoke): the dust is drawn by its row, the puffs behind the car before its body (dust_row) */
    dust_puffs(car, slot, -1);
}

/* the puffs of a car's trail: pass -1 all of them (the exe's order, after the body), 1 those not nearer the eye than the car (before its body), 0 the nearer */
static void dust_puffs(const Car *car, int slot, int pass)
{
    const int kind = shadow_race->port_rally_gravel;
    DustTrail *const trail = &dust_trails[slot];
    if (!dust_trail_started(trail)) return;   /* no history yet: its zeroed puffs are not puffs */
    if (rally_spray_on) return;   /* port (rally_spray): the wet road's spray is drawn in its place */
    if (kind != RALLY_GRAVEL_DUST && kind != RALLY_GRAVEL_REGOLITH && kind != RALLY_GRAVEL_SNOW) return;
    int32_t own_camera[3], own_screen[2];
    const int32_t own_at[3] = { car->x, car->y, car->z };
    const int own_seen = world_screen(own_at, own_camera, own_screen);
    const PuffView v = puff_view();
    const int lunar = shadow_race->port_lunar_gravity != 0;
    uint8_t mist_road[3];
    int mist_have = 0;   /* port (spray_cd_smoke): the road's colour by the oldest puff, before this car's puffs are over it */
    if (spray_cd_on && kind != RALLY_GRAVEL_REGOLITH)
        for (int n = 0; n < DUST_TRAIL_PUFFS && !mist_have; n++) {
            const DustPuff *p = &trail->puff[(trail->next + n) % DUST_TRAIL_PUFFS];
            int32_t place[3], size;
            if (dust_puff_at(p, shadow_race->tick, kind, shadow_race->port_lunar_gravity != 0, place, &size)) {
                const int32_t ground[3] = { p->pos[0], p->pos[1], p->pos[2] };
                mist_have = spray_ground_colour(ground, mist_road);
            }
        }
    const int rise = trail_rise_on && kind != RALLY_GRAVEL_REGOLITH;   /* port (trail_rise): the dust climbs to a car's height and thins; the regolith is thrown up and falls back as it is */
    for (int n = 0; n < DUST_TRAIL_PUFFS; n++) {   /* the ring's oldest first: the farthest from a camera behind the car */
        const DustPuff *p = &trail->puff[(trail->next + n) % DUST_TRAIL_PUFFS];
        int32_t place[3], size, camera[3], screen[2];
        if (!dust_puff_at(p, shadow_race->tick, kind, lunar, place, &size)) continue;
        if (rise) {
            const int age = (int)(shadow_race->tick - p->born), life = (int)dust_life(kind, lunar);
            place[1] = p->pos[1] + trail_rise_height(age, life);
            size = size * trail_rise_fade(age, life) / 100;
            if (size < 1) size = 1;
        }
        if (!world_screen(place, camera, screen) || camera[2] < 64) continue;
        if (pass >= 0 && (!own_seen || camera[2] >= own_camera[2] - DUST_BODY_REAR) != (pass == 1)) continue;   /* port (spray_cd_smoke): a puff behind its car is drawn before the body, one in front after it */
        const int32_t depth = 0x38400 / camera[2];
        const int32_t scale = depth * size * DUST_SIZE / 0x100 * 8;
        if (mist_have) {   /* port (spray_cd_smoke): the cloud only, the CD's sprite tinted by the road */
            const int32_t w = (int32_t)((int64_t)scale * 75 >> 16);
            if (spray_cd_dir) dir_draw(mist_road, p->seed, screen[0], screen[1], scale);   /* the CD's gravel puffs in the road's colour, not the cloud */
            else mist_draw(mist_road, 3, screen[0], screen[1], w, w * 7 / 10 + 1, p->seed);
            continue;
        }
        puff_smoke(&v, screen[0], screen[1], scale);
        if (kind != RALLY_GRAVEL_SNOW && (p->seed & 3) < DUST_DIRT_SHARE) {   /* port (snow_weather): the powder is white smoke, with no grit in it */
            const int32_t dx = (p->seed >> 2 & 0x1f) - 0x10, dy = (p->seed >> 7 & 0xf) - 8;
            puff_dirt(&v, screen[0] + dx * depth / 0x1000, screen[1] + dy * depth / 0x1000, scale / 2);
        }
    }
}

/* port (spray_cd_smoke): the dust of the cars on a row, the puffs behind each car before the row's bodies and the nearer after them, as spray_row does the water */
static void dust_row(int32_t node, int far_side)
{
    if (!spray_cd_on || spray_v4_on()) return;
    for (int i = 0; i < RACE_CARS; i++) {
        const Car *car = shadow_race->cars[i];
        if (car == NULL || car->node != node) continue;
        if (car == followed_car() || (shadow_race->port_rally_road != 0 && port_rally_spray())) dust_puffs(car, i, far_side);
    }
}

/* 00490570 with *004c55e0 ('smok') */
void view_spray_trail_puff(void *ctx, int32_t scale, int32_t x, int32_t y)
{
    (void)ctx;
    sprite_scaled_blend_at(object_pixels.target, puff_blend->rows, puff_art->smok, scale, x, y);
}

/* the frame's setup, once the 3D's tables are made: the switches of this frame (read from the settings), the rain's particles stepped and their tables, the dust's trails stepped, spray v4's tick hook and colours */
void view_spray_frame(const TrackArt *art, const WeatherTables *wt, int wet)
{
    rain_spray_on = !spray_v4_on() && wt != NULL && wet && port_rain_fx() && shadow_race != NULL;
    rally_spray_on = rain_spray_on && shadow_race->port_rally_road != 0 && port_rally_spray();   /* port (rally_spray) */
    trail_rise_on = port_trail_rise();   /* port (trail_rise) */
    water_mist_variant = port_water_mist();   /* port (water_mist) */
    spray_cd_on = port_spray_cd_smoke() != 0 && mist_sprite() != NULL;
    spray_cd_dir = spray_cd_on && port_spray_cd_smoke() == 2;   /* port (spray_cd_smoke) */
    if (rain_spray_on) {   /* port (rain v5) */
        spray_tables_set(wt);
        spray_update();
    }
    dust_update();   /* port (cam_trail) */
    if (spray_v4_on()) {   /* port (spray v4) */
        race_sim_set_tick_hook(spray_v4_tick);
        spray_v4_frame(art);
    }
}

/* a road row's spray and dust: far_side 1 the puffs farther off than their car (before the row's cars), 0 the nearer (after) */
void view_spray_row(int32_t node, int far_side)
{
    if (spray_v4_on()) spray_v4_draw_row(node, far_side);
    if (rain_spray_on) spray_row(node, far_side);   /* port (rain v5): the water */
    dust_row(node, far_side);
}

/* the track's draw is over: the rain's spray and the rally's tint are not drawn again until the next frame sets them */
void view_spray_frame_end(void)
{
    rain_spray_on = 0, rally_spray_on = 0;
}
