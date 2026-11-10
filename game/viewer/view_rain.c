/* port (rain_fx, rain v4, weather_zones): what a wet road does round the cars on the screen and how much weather there is at a node, taken out of ported_view.c: the tail lights' reflections,
 * the rain's vanishing point, the closed zones (tunnels, galleries) that keep the weather out. ported_view.c calls these; what this file takes of the view is in view_shared.h. */

#include "game/race/car_progress.h"
#include "game/race/race_chase.h"
#include "game/race/psx_light_synth.h"
#include "game/sys/fixed_math.h"
#include "game/sys/port_settings.h"
#include "viewer/drive_lamps.h"
#include "viewer/drive_light.h"
#include "viewer/view_rain.h"
#include "viewer/view_shared.h"
#include "viewer/view_spray.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* port (rain_fx, rain v4): what a wet road does round every car on the screen (the followed one and the rivals ahead): spray, a cloud of grainy dithered dots behind the rear wheels (a
 * plume from weather_rain_spray_puff: a puff's scatter of dots, thicker at its heart, widening and thinning toward the plume's end; some dots dark, so it is grit and not a lit
 * disc), and the tail lights' reflections, a short dithered streak of their red under each lamp. Painted once the frame's 3D is whole (the recorder replayed), as the streaks are. */

/* port (rain v4, dev env only): TNFS_RAIN_WET_OFF=1 leaves the grey scene and the tail reflections out, for the before / after captures */
int rain_wet_off(void)
{
    static int off = -1;
    if (off < 0) off = getenv("TNFS_RAIN_WET_OFF") != NULL;
    return off;
}

uint32_t rain_hash(uint32_t a, uint32_t b)
{
    uint32_t h = a * 0x9e3779b1u ^ (b + 0x7f4a7c15u) * 0x85ebca6bu;
    h ^= h >> 15, h *= 0x2c1b3c6du, h ^= h >> 12;
    return h;
}

static void rain_dot(uint8_t *pixels, int32_t pitch, const int32_t box[4], int32_t px, int32_t py, const uint8_t table[256])
{
    if (px < box[0] || px >= box[2] || py < box[1] || py >= box[3]) return;
    uint8_t *d = pixels + (size_t)py * (size_t)pitch + (size_t)px;
    *d = table[*d];
}

/* port (weather_zones): how much weather there is at a node, 0..16: none inside a closed zone (the light's tunnel nodes, psx_light_weather_closed, alone when they are a places file's, Coastal's galleries among them; else Coastal's galleries, rain_cover, and the
 * road's own tunnels where there is no light), fading in over WEATHER_MOUTH_NODES nodes outside its mouths. The rain, the haze and the fog, the lightning and the lens drops follow it.
 * A hall is not a closed zone and a canyon is open air. */
static int weather_closed_at(const uint8_t *tunnel, const uint8_t *cover, uint32_t node)
{
    if (psx_light != NULL && psx_light->from_places) return psx_light_weather_closed(psx_light, node);   /* port (tunnel_dry): a places file is the only source of the closed zones */
    if (cover != NULL && cover[node]) return 1;
    if (psx_light != NULL) return psx_light_weather_closed(psx_light, node);
    return tunnel != NULL && tunnel[node] == PSX_SYNTH_FLAG_TUNNEL;
}

int rain_open_at(int32_t node)
{
    if (shadow_race == NULL) return 16;
    const uint32_t nodes = shadow_race->tri->ring_nodes;
    const uint8_t *tunnel = road_tunnels(shadow_race, nodes);
    const uint8_t *cover = rain_cover(shadow_race, nodes);   /* a roof the lighting's tunnel runs do not mark: Coastal's galleries */
    if (tunnel == NULL && cover == NULL && psx_light == NULL) return 16;
    for (int32_t d = 0; d < WEATHER_MOUTH_NODES; d++)
        if (weather_closed_at(tunnel, cover, (uint32_t)(node + d) & (nodes - 1u)) || weather_closed_at(tunnel, cover, (uint32_t)(node - d) & (nodes - 1u)))
            return weather_open_for_distance(d);
    return 16;
}

int ported_view_weather_open(uint32_t node) { return rain_open_at((int32_t)node); }

/* The lamps' reflections on a wet road: each lamp (at the car's bumper, 0.36 of its width out, a little above the road) is mirrored in the road below it, a short vertical dithered streak from
 * the lamp's ground point (its x and z, the road's y) towards the viewer, wider at the top and thinning. A wet road is a mirror: it shows a lamp only when the lamp faces the viewer, so the
 * tail lights' (red) show when the car's rear is toward the camera and the headlights' (warm white, lit ones only) when its front is, both gone at a side view (the dot of the car's forward
 * vector with the car-to-camera one, |dot| 0.3 .. 0.7 smooth) and with distance (full to 25 m, none from 75 m). The car's own heading as it is drawn (traffic draws with its matrix, not the
 * yaw) and its own length and width place the lamps. */
enum { REFLECT_DEFAULT_LENGTH = 0x48000, REFLECT_DEFAULT_WIDTH = 0x1cccc };   /* a car's length and width (16.16) when the model gives none */

/* The reflection's strength in percent for `car` seen from the camera (0 none), and whether it is the front that faces the camera. */
static int reflection_weight(const Car *car, int32_t forward[2], int *front)
{
    int32_t fx, fz;
    if (car->use_matrix) fx = car->matrix[6], fz = car->matrix[8];
    else {
        const int32_t angle = (car->yaw >> 8) & 0xffff;
        fx = fx_sin16(angle), fz = fx_cos16(angle);
    }
    const double len = sqrt((double)fx * fx + (double)fz * fz);
    if (len < 1.0) return 0;
    const double ux = fx / len, uz = fz / len;
    const double dx = ((double)model_view.camera_position[0] - car->x) / 65536.0, dz = ((double)model_view.camera_position[2] - car->z) / 65536.0;
    const double dist = sqrt(dx * dx + dz * dz);
    if (dist < 0.5 || dist >= WEATHER_REFLECT_FAR_M) return 0;
    const double dot = (ux * dx + uz * dz) / dist;
    forward[0] = (int32_t)(ux * 65536.0), forward[1] = (int32_t)(uz * 65536.0);
    *front = dot > 0;
    return weather_reflection_weight((float)dot, (float)dist);
}

/* Whether the road between the camera and `foot` (a lamp's point on the road) rises above the line of sight: the lamp is behind a crest, and its reflection, painted over the finished frame with no
 * depth, would hang over the road in front of the crest. The road's height is sampled at eight points of the line (the plane of the node each lies in, as the car's shadow takes it). */
static int foot_behind_crest(const Car *car, const int32_t foot[3])
{
    const int32_t eye[3] = { model_view.camera_position[0], model_view.camera_position[1], model_view.camera_position[2] };
    const int32_t margin = 0x2666;   /* 0.15 m: the plane under the mesh's own steps */
    int32_t node = car->node;
    for (int k = 7; k >= 1; k--) {
        int32_t at[3];
        for (int i = 0; i < 3; i++) at[i] = (int32_t)(eye[i] + (int64_t)(foot[i] - eye[i]) * k / 8);
        point_find_node(at, shadow_race, &node);
        if (race_chase_road_height(shadow_race, node, at) > at[1] + margin) return 1;
    }
    return 0;
}

/* port (rain_v2): whether a ground point (16.16) is on the carriageway of the node: its sideways distance from the road's centre (weather_across_road) is within the verge on its side
 * (the node's left or right verge, in 1/8 m). Where the node gives no verge the point counts as on it. */
static int foot_on_carriageway(const int32_t foot[3], int32_t node)
{
    if (shadow_race == NULL) return 1;
    const TriNode n = race_node(shadow_race, node);
    const int32_t across = weather_across_road(n.orientation * 4, foot[0] - n.position.x, foot[2] - n.position.z);   /* not car_lateral's: that one is the distance along the road */
    return weather_on_carriageway(across, n.left_verge, n.right_verge);
}

static void reflect_car(const Car *car, uint8_t *pixels, int32_t pitch, const int32_t box[4], const WeatherTables *wt)
{
    const int night = drive_light_night();
    int32_t forward[2];
    int front = 0;
    const int weight = reflection_weight(car, forward, &front);
    if (weight <= 0) return;
    if (front && drive_lamps_lit_entry(car->index, LAMP_HEADLIGHT) == 0) return;   /* the headlights' only when they are lit */
    const uint8_t *table = front ? wt->head : wt->tail;
    const int32_t length = car->model_length > 0 ? car->model_length : REFLECT_DEFAULT_LENGTH, width = car->model_width > 0 ? car->model_width : REFLECT_DEFAULT_WIDTH;
    const int32_t along = (int32_t)(((int64_t)length * (front ? 19 : -19)) / 40);   /* the bumper: 0.475 of the length from the centre */
    for (int side = -1; side <= 1; side += 2) {
        int32_t foot[3], mirror[3], c0[3], c1[3], s0[2], s1[2];
        const int32_t across = (int32_t)(((int64_t)width * 9 * side) / 25);   /* 0.36 of the width out */
        foot[0] = mirror[0] = car->x + (int32_t)(((int64_t)forward[0] * along) >> 16) + (int32_t)(((int64_t)forward[1] * across) >> 16);
        foot[2] = mirror[2] = car->z + (int32_t)(((int64_t)forward[1] * along) >> 16) - (int32_t)(((int64_t)forward[0] * across) >> 16);
        /* the road under the lamp: its own height there (the bumper is a couple of metres off the car's origin, which is on a slope) */
        foot[1] = race_chase_road_height(shadow_race, car->node, foot);
        mirror[1] = foot[1] - 0xc000;   /* and the lamp as the road shows it, below it (a wet road smears it longer than the lamp is high) */
        if (foot_behind_crest(car, foot)) continue;   /* behind a crest the road does not show it */
        if (port_rain_v2() && !foot_on_carriageway(foot, car->node)) continue;   /* port (rain_v2): the shoulder is no mirror: the reflection stays on the carriageway */
        if (!world_screen(foot, c0, s0) || c0[2] < 64 || !world_screen(mirror, c1, s1) || c1[2] < 64) continue;
        const int32_t half = (0x38400 / c0[2] * 220 / SPRAY_DIV + 1) * (night ? 3 : 2) / 2;   /* wider at night: the lamps are the light */
        const int32_t steps = abs(s1[1] - s0[1]) > 48 ? 48 : abs(s1[1] - s0[1]);
        if (steps < 1) continue;
        for (int32_t t = 0; t <= steps; t++) {
            const int32_t cy = s0[1] + (s1[1] - s0[1]) * t / steps, cx = s0[0] + (s1[0] - s0[0]) * t / steps;
            const int32_t w = half * (steps - t / 2) / steps;   /* a little narrower toward the end */
            const int32_t keep = ((night ? 100 : 75) * (steps - t) / steps + 10) * weight / 100;   /* and thinner, and thinner still where the weight is low */
            for (int32_t x = -w; x <= w; x++)
                if ((int32_t)(rain_hash((uint32_t)((cx + x) * 31 + cy), 5u) % 100u) < keep && (night || ((cy + t) % 3) != 0)) rain_dot(pixels, pitch, box, cx + x, cy, table);
        }
    }
}

/* port (rain_fx, rain v5): where the rain streams from: the screen point the followed car's velocity direction projects to in the CURRENT camera (the screen's centre in the cockpit and the
 * bumper, the road's far end ahead of a chase camera), possibly off the screen (a helicopter's: the streaks are then parallel, along the travel). When that direction's far point is behind
 * the lens the point is the travel's own direction on the screen, far out. `out` (screen pixels) holds the centre, which stays when the car is not known. */
void rain_vanishing_point(int32_t out[2])
{
    if (shadow_race == NULL) return;
    const Car *car = followed_car();
    if (car == NULL) return;
    const int32_t angle = (car->yaw >> 8) & 0xffff;
    const int32_t fx = fx_sin16(angle), fz = fx_cos16(angle);
    const int32_t at[3] = { car->x, car->y, car->z };
    int32_t far[3], camera[3], s_far[2], s_at[2], s_near[2], near[3], c0[3], c1[3];
    const int64_t far_m = (int64_t)300 << 16, near_m = (int64_t)20 << 16;
    far[0] = at[0] + (int32_t)(((int64_t)fx * far_m) >> 16), far[1] = at[1], far[2] = at[2] + (int32_t)(((int64_t)fz * far_m) >> 16);
    if (world_screen(far, camera, s_far) && camera[2] > 64) {
        out[0] = s_far[0], out[1] = s_far[1];
    } else {
        near[0] = at[0] + (int32_t)(((int64_t)fx * near_m) >> 16), near[1] = at[1], near[2] = at[2] + (int32_t)(((int64_t)fz * near_m) >> 16);
        if (!world_screen(at, c0, s_at) || c0[2] <= 64 || !world_screen(near, c1, s_near) || c1[2] <= 64) return;
        const float dx = (float)(s_near[0] - s_at[0]), dy = (float)(s_near[1] - s_at[1]), n = sqrtf(dx * dx + dy * dy);
        if (n < 1.0f) return;
        out[0] = s_at[0] + (int32_t)(dx / n * 6000.0f), out[1] = s_at[1] + (int32_t)(dy / n * 6000.0f);
    }
    out[0] = out[0] < -8000 ? -8000 : out[0] > 8000 ? 8000 : out[0];
    out[1] = out[1] < -8000 ? -8000 : out[1] > 8000 ? 8000 : out[1];
}

void rain_cars_paint(uint8_t *pixels, int32_t pitch, const int32_t box[4], const WeatherTables *wt)
{
    if (shadow_race == NULL) return;
    const Car *player = followed_car();
    const int32_t n = shadow_race->car_count < RACE_CARS ? shadow_race->car_count : RACE_CARS;
    for (int32_t i = 0; i < n; i++) {
        const Car *car = shadow_race->cars[i];
        if (car == NULL) continue;
        if (car != player && (body_drawn_tick[i] == 0 || shadow_race->tick + 1 - body_drawn_tick[i] > 4 || shadow_race->tick + 1 < body_drawn_tick[i])) continue;   /* a rival that is on the screen */
        if (shadow_race->port_rally_road != 0 && port_rally_dry_lamps()) continue;   /* port (rally_dry_lamps): dirt is no mirror, no lamp reflections (an early skip at the call, reflect_car itself untouched) */
        if (!rain_wet_off() && rain_open_at(car->node) > 0) reflect_car(car, pixels, pitch, box, wt);   /* port (tunnel_dry): no mirror road inside a closed zone */
    }
}
