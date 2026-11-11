#include "game/race/track_node_light.h"
#include "game/race/light_tint.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void track_node_light_free(TrackNodeLight *light)
{
    free(light->data);
    free(light->hall);
    free(light->tunnel);
    free(light->car_dim);
    light->car_dim = NULL;
    free(light->canyon);
    light->canyon = NULL;
    free(light->pool_class);
    free(light->car_pool);
    free(light->car_tone);
    free(light->lamp_lift);
    light->lamp_lift = NULL;
    free(light->lamps);
    light->lamps = NULL, light->lamp_n = 0, light->lamp_placed = 0, light->lamp_reach_nodes = 0;
    free(light->lamp_node);
    light->lamp_node = NULL, light->lamp_node_count = 0, light->lamp_ring = 0;
    free(light->tone_id);
    free(light->tone_mask);
    light->tone_id = NULL, light->tone_mask = NULL, light->tone_count = 0;
    light->car_tone = NULL;
    light->car_pool = NULL;
    light->pool_class = NULL;
    light->data = NULL;
    light->hall = NULL;
    light->tunnel = NULL;
    light->scripted = 0;
    light->pooled = 0;
    light->reversed = 0;
    light->nodes = 0;
}

int track_node_light_set_script(TrackNodeLight *light, const PsxLgs *script)
{
    if (light == NULL || light->data == NULL || script == NULL) return 0;
    light->script = *script;
    light->scripted = 1;
    return 1;
}

int track_node_light_set_reversed(TrackNodeLight *light, int reversed)
{
    if (light == NULL || light->data == NULL) return 0;
    light->reversed = reversed != 0;
    return 1;
}

uint32_t track_node_light_old_node(const TrackNodeLight *light, uint32_t node)
{
    if (light == NULL || light->nodes == 0) return node;
    node %= light->nodes;
    return light->reversed ? (light->nodes - node) % light->nodes : node;
}

/* a point of a row as the file has it: the chains swap on a reversed track */
static int old_point(const TrackNodeLight *light, int point)
{
    return light->reversed && point > 0 ? (point + 5 - 1) % 10 + 1 : point;
}

uint8_t track_node_light_byte(const TrackNodeLight *light, uint32_t node, int point)
{
    if (light == NULL || light->data == NULL || light->nodes < 2 || point < 0 || point >= TRACK_NODE_LIGHT_POINTS) return TRACK_NODE_LIGHT_FULL;
    node = track_node_light_old_node(light, node);
    point = old_point(light, point);
    if (node == light->nodes - 1) node--;
    return light->data[(size_t)node * TRACK_NODE_LIGHT_POINTS + (size_t)point];
}

static int row_is_tunnel(const uint8_t *row)
{
    return (row[2] >= TRACK_NODE_LIGHT_WALL_FOOT && row[4] == 0 && row[5] == 0) || (row[7] >= TRACK_NODE_LIGHT_WALL_FOOT && row[9] == 0 && row[10] == 0);
}

/* the tunnel test on a node of the file's own direction */
static int tunnel_in_file(const TrackNodeLight *light, uint32_t node)
{
    if (light == NULL || light->data == NULL || light->nodes < 2) return 0;
    node %= light->nodes;
    if (node == light->nodes - 1) node--;
    if (light->tunnel != NULL && light->tunnel[node]) return 1;   /* a made light's road tunnel (made_tunnels); the rest is the rows' */
    if (light->scripted) return psx_lgs_switched(&light->script, (int)(node >> 2));
    return row_is_tunnel(light->data + (size_t)node * TRACK_NODE_LIGHT_POINTS);
}

int track_node_light_tunnel(const TrackNodeLight *light, uint32_t node)
{
    return tunnel_in_file(light, track_node_light_old_node(light, node));
}

int track_node_light_weather_closed(const TrackNodeLight *light, uint32_t node)
{
    if (light == NULL || light->nodes == 0) return 0;
    const uint32_t at = track_node_light_old_node(light, node) % light->nodes;
    if (light->canyon != NULL && light->canyon[at]) return 0;
    if (light->hall != NULL && light->hall[at]) return 1;   /* a hall is a tunnel with a lot of light: it has a roof */
    if (light->tunnel != NULL) return light->tunnel[at] != 0;
    return track_node_light_tunnel(light, node);
}

int track_node_light_zone_look_at(const TrackNodeLight *light, uint32_t at, double *percent, float cast[3])
{
    if (light == NULL || !light->look_set) return 0;
    *percent = light->look_light;
    memcpy(cast, light->look_cast, 3 * sizeof(float));
    if (light->tone_id != NULL && light->nodes > 0) {
        const int id = light->tone_id[at % light->nodes];
        if (id > 0 && id <= light->tone_count && light->tone[id - 1].look) {
            *percent = (double)light->tone[id - 1].light;
            memcpy(cast, light->tone[id - 1].cast, 3 * sizeof(float));
        }
    }
    return 1;
}

void track_node_light_tunnel_look(const TrackNodeLight *light, const float shown[3], const float lit_tint[3], int lamp_count, TrackNodeTunnelLook *out)
{
    static const float TUNNEL_CAST[3] = { 1.0f, 0.8f, 0.15f }, NO_CAST[3] = { 1, 1, 1 };
    static const float LEAST = 0.6f;   /* the least light a tunnel's surfaces take (the dusk's mean): its own lamps light it */
    memset(out, 0, sizeof *out);
    for (int k = 0; k < 3; k++) out->tint[k] = lit_tint[k];
    if (light == NULL) return;
    const float mean = (shown[0] + shown[1] + shown[2]) / 3.0f;
    out->min_light = light->tunnel != NULL ? 0.0f : LEAST;
    if (light->look_set) {   /* the file's: its light is of full light at any time of day, so the tables take it over the time's own tint */
        const float gain = (float)(mean > 0.05f ? light->look_light / 100.0 / (double)mean : light->look_light / 100.0);
        out->cast_on = 1;
        out->tint[0] = out->tint[1] = out->tint[2] = gain;
        memcpy(out->cast, light->look_cast, sizeof out->cast);
        return;
    }
    const int dark = mean < LEAST;
    const int warm = light->city_tunnel != 0 && light->tunnel != NULL && !light->neutral_cast;
    const float *warm_cast = warm && light->warm_percent[dark] > 0 ? light->warm_cast[dark] : NULL;   /* the made light's look of this kind of tunnel */
    const int percent = warm_cast != NULL ? light->warm_percent[dark] : 0;
    const int kind = warm_cast != NULL ? 3 : light->neutral_cast ? 2 : lamp_count > 0 || light->tunnel != NULL;   /* a tunnel track: its darkened levels take the olive cast and the tunnel's least light */
    if (light->tunnel != NULL) {   /* the palette shown is the night's, so the tables are made brighter by as much as it takes off, to LEAST on the screen */
        const float base = mean > 0.05f && mean < LEAST ? LEAST / mean : 1.0f, gain = warm_cast != NULL ? base * (float)percent / 100.0f : base;
        out->tint[0] = out->tint[1] = out->tint[2] = gain;
    }
    out->cast_on = kind != 0;
    memcpy(out->cast, warm_cast != NULL ? warm_cast : kind == 2 ? NO_CAST : TUNNEL_CAST, sizeof out->cast);
}

int track_node_light_tunnel_class(const TrackNodeLight *light, uint32_t node)
{
    const uint32_t file_node = light != NULL ? track_node_light_old_node(light, node) : 0;
    const int forest = light != NULL && light->forest_road && file_node >= light->forest_from && file_node <= light->forest_to;   /* port (forest_neutral): its shade is not a tunnel's */
    if (!forest && track_node_light_tunnel(light, node)) return 1;
    if (light == NULL || light->pool_class == NULL || light->nodes == 0) return 0;
    const uint8_t c = light->pool_class[node % light->nodes];
    return c > 0 ? 1 + c : 0;
}

int track_node_light_vertex_class(const TrackNodeLight *light, uint32_t node, int point)
{
    int cls = track_node_light_tunnel_class(light, node);
    if (light != NULL && light->tone_id != NULL && light->nodes > 0 && point >= 0 && point < TRACK_NODE_LIGHT_POINTS) {
        const uint32_t at = track_node_light_old_node(light, node) % light->nodes;
        const int id = light->tone_id[at];
        if (id > 0 && id <= TRACK_NODE_LIGHT_TONES && (light->tone_mask[at] >> old_point(light, point) & 1u)) cls |= id << TRACK_NODE_LIGHT_TONE_BASE_BITS;
    }
    return cls;
}

static uint8_t light_at(const TrackNodeLight *light, uint32_t node, int point)
{
    if (light == NULL || light->data == NULL || light->nodes < 2 || point < 0 || point >= TRACK_NODE_LIGHT_POINTS) return TRACK_NODE_LIGHT_FULL;
    node = track_node_light_old_node(light, node);
    point = old_point(light, point);
    if (node == light->nodes - 1) node--;
    const uint8_t *row = light->data + (size_t)node * TRACK_NODE_LIGHT_POINTS;
    if (light->scripted) {   /* the exe's light: the context's colour or the LGT through T, as the luminance of the three channels */
        float f[3];
        light_tint_script_light(&light->script, (int)node, row[point], f);
        const float y = 0.30f * f[0] + 0.59f * f[1] + 0.11f * f[2];
        return (uint8_t)(y >= 1.0f ? 255 : y <= 0.0f ? 0 : y * 255.0f + 0.5f);
    }
    const int tunnel = tunnel_in_file(light, node);
    /* in a tunnel a 0 beyond a lit wall foot (point 2 right, 7 left) is no data, not black: the console version shows its walls and pillars at about half light;
     * anywhere else (a cutting, a bridge's shade) a 0 is the black it says */
    const int foot = point >= 3 && point <= 5 ? row[2] : point >= 8 ? row[7] : 0;
    if (tunnel && row[point] == 0 && foot >= TRACK_NODE_LIGHT_WALL_FOOT) return TRACK_NODE_LIGHT_WALL;
    /* the road inside a tunnel (a lit foot with 0s from point 4 on, either side) is dark on the console version although its bytes say 255 */
    if (tunnel && (point == 0 || point == 1 || point == 6) && row[point] > TRACK_NODE_LIGHT_TUNNEL_ROAD) return TRACK_NODE_LIGHT_TUNNEL_ROAD;
    if (tunnel && (point == 2 || point == 7) && row[point] > TRACK_NODE_LIGHT_WALL) return TRACK_NODE_LIGHT_WALL;   /* the shoulder's far edge: half light too */
    return row[point];
}

float track_node_light_road_light(const TrackNodeLight *light, uint32_t node)
{
    return ((float)light_at(light, node, 0) + (float)light_at(light, node, 1) + (float)light_at(light, node, 6)) / (3.0f * (float)TRACK_NODE_LIGHT_FULL);
}

int track_node_light_look_tunnel(const TrackNodeLight *light, uint32_t node) { return light != NULL && light->look_set && track_node_light_tunnel(light, node); }

static int made_tunnel(const TrackNodeLight *light, uint32_t node)
{
    return light->tunnel != NULL && light->nodes > 0 && light->tunnel[node % light->nodes] != 0;
}

/* xz: the point's place on the ground in metres, or NULL when only the node is known (the node's centre is used with headlight_cone) */
static uint8_t light_at_dark(const TrackNodeLight *light, uint32_t node, int point, const double *xz)
{
    uint8_t v = light_at(light, node, point);
    if (light == NULL || light->pool == NULL) return v;
    const int lift = light->pool->cone && xz != NULL ? headlight_pool_lift_xz(light->pool, xz[0], xz[1]) : headlight_pool_beam(light->pool, node);   /* port (headlight_cone): the beam at the point itself */
    if (light->shoulder_road && (point == 2 || point == 7) && !track_node_light_tunnel(light, node)) {   /* port (shoulder_light): where the beam is, the shoulder's far edge takes the road's light (the made rows leave it near black and the beam only scales the row's light); the more of the beam, the more of it, so far ground is as it was */
        const int road = light_at(light, node, point == 2 ? 1 : 6);
        if (road > v) v = (uint8_t)(v + (road - v) * (lift * lift / 256) / 256);   /* the beam's weight squared: the far, faint end of the cone leaves the row alone */
    }
    if (!track_node_light_tunnel(light, node) && !headlight_pool_tunnel(light->pool, node)) {
        int gain = headlight_pool_gain_at_lift(HEADLIGHT_FLOOR, lift);   /* the pool is as wide as the road and its shoulders: the ground beyond takes less of it */
        int floor = HEADLIGHT_FLOOR;
        if (light->night_lift > 0) floor += (256 - floor) * light->night_lift / 256;   /* port (city_lift, vegas_lift): the dark of the open night is less deep, a lit pool keeps its full light */
        const int share = light->pool->cone && xz != NULL ? 256 : headlight_pool_width(point);   /* port (headlight_cone): the cone has its own sides, so the verges and what stands beyond take the beam by where they are */
        const int lamp_floor = track_node_light_lamp_floor(light, node);   /* port (lamp_pool_bright): under a street lamp the floor lifts toward full, by the pool's class (1..4 of 4) */
        if (lamp_floor > floor) floor = lamp_floor;
        if (light->lamp_lift != NULL && light->nodes > 0) {   /* port (`lamp`): the baked pool lifts the floor toward full by the point's weight */
            const int w = light->lamp_lift[(size_t)track_node_light_old_node(light, node) * TRACK_NODE_LIGHT_POINTS + (size_t)old_point(light, point)];
            if (w > 0) floor += (256 - floor) * w / 255;
        }
        if (light->night_lift > 0 && light->beam_on_top) gain = headlight_pool_gain_at_lift(floor, lift);   /* port (city_lift 3): the beam adds on top of the lifted floor, it is not lost under it */
        if (gain < floor) gain = floor;
        uint8_t open = headlight_pool_apply(v, floor + (gain - floor) * share / 256);
        if (light->beam_open > 0) {   /* port (city_lift 3): a visible patch where the rows are dark: the beam lights the ground to beam_open at its full, not only a share of the row's own light */
            const int lit = light->beam_open * lift / 256 * share / 256;
            if (lit > open) open = (uint8_t)lit;
        }
        if (light->beam_tunnel > 0 && light->hall != NULL && light->nodes > 0 && light->hall[node % light->nodes]) {   /* port (the tunnel headlight): the lit hall's walls and pillars take the beam too */
            const int hall_lit = light->beam_tunnel * lift / 256;
            return hall_lit > open ? (uint8_t)hall_lit : open;
        }
        return open;
    }
    /* a tunnel: less dark than the open night (its own lamps), and the beam lights it to TUNNEL_BEAM where its own rows are darker (a tunnel with no lamps is black) */
    const uint8_t scaled = made_tunnel(light, node) ? v : headlight_pool_apply(v, headlight_pool_gain_at_lift(light->tunnel_floor > 0 ? light->tunnel_floor : TUNNEL_FLOOR, lift));   /* a made light's tunnel (made_tunnels) keeps the dusk's light its rows carry: the night's floor is the open road's */
    const int lit = (light->beam_tunnel > 0 ? light->beam_tunnel : TUNNEL_BEAM) * lift / 256;
    return lit > scaled ? (uint8_t)lit : scaled;
}

int track_node_light_lamp_floor(const TrackNodeLight *light, uint32_t node)
{
    if (light == NULL || !light->lamp_bright || light->car_pool == NULL || light->nodes == 0 || node == TRACK_NODE_LIGHT_NO_NODE) return 0;
    const int pc = light->car_pool[node % light->nodes];
    if (pc <= 0) return 0;
    if (light->lamp_local) {   /* port (city_lift 3): local pools, full at the lamp and a few metres round it, the dark base between lamps */
        static const int LOCAL[5] = { 0, 0, 24, 96, 256 };   /* the share (of 256) of the way from the night's floor to full, by class */
        return HEADLIGHT_FLOOR + (256 - HEADLIGHT_FLOOR) * LOCAL[pc > 4 ? 4 : pc] / 256;
    }
    return HEADLIGHT_FLOOR + (256 - HEADLIGHT_FLOOR) * pc / 4;
}

int track_node_light_open_gain_floor(const TrackNodeLight *light, int lift, int lamp_floor)
{
    if (light == NULL || light->pool == NULL) return 256;
    int gain = headlight_pool_gain_at_lift(HEADLIGHT_FLOOR, lift);
    int floor = HEADLIGHT_FLOOR;
    if (light->night_lift > 0) floor += (256 - floor) * light->night_lift / 256;
    if (lamp_floor > floor) floor = lamp_floor;
    if (light->night_lift > 0 && light->beam_on_top) gain = headlight_pool_gain_at_lift(floor, lift);   /* the beam on top of the lifted floor, as light_at_dark */
    if (gain < floor) gain = floor;
    return gain;
}

void track_node_light_relight_xz(const TrackNodeLight *light, const int32_t (*world)[3], const uint32_t *node, const uint8_t *point, int32_t count, uint8_t *out)
{
    if (light == NULL || light->pool == NULL || !light->pool->cone || world == NULL || node == NULL || point == NULL || out == NULL) return;
    for (int32_t i = 0; i < count; i++) {
        const double at[2] = { world[i][0] / 65536.0, world[i][2] / 65536.0 };
        out[i] = track_node_light_at_xz(light, node[i], point[i], at);
    }
}

uint8_t track_node_light_at_xz(const TrackNodeLight *light, uint32_t node, int point, const double *xz)
{
    const uint8_t v = light_at_dark(light, node, point, xz);
    /* port (vegas_objects): the points beyond the road's shoulder (3, 4, 5, 8..10: the walls and the buildings) keep almost full light, the road's own (0, 1, 2, 6, 7) take the dark and the pools */
    const int beyond = point == 3 || point == 4 || point == 5 || point >= 8;
    return light != NULL && light->object_floor > v && beyond ? light->object_floor : v;
}

uint8_t track_node_light_at(const TrackNodeLight *light, uint32_t node, int point) { return track_node_light_at_xz(light, node, point, NULL); }

uint8_t track_node_light_node_xz(const TrackNodeLight *light, uint32_t node, const double *xz)
{
    static const int TERRAIN[] = { 2, 3, 4, 5, 7, 8, 9, 10 };
    if (light == NULL || light->data == NULL) return TRACK_NODE_LIGHT_FULL;
    unsigned sum = 0;
    for (size_t i = 0; i < sizeof TERRAIN / sizeof *TERRAIN; i++) sum += track_node_light_at_xz(light, node, TERRAIN[i], xz);
    return (uint8_t)((sum + sizeof TERRAIN / sizeof *TERRAIN / 2) / (sizeof TERRAIN / sizeof *TERRAIN));
}

uint8_t track_node_light_node(const TrackNodeLight *light, uint32_t node) { return track_node_light_node_xz(light, node, NULL); }

float track_node_light_lamp_falloff(float distance, float reach)
{
    if (reach <= 0.0f || distance >= reach) return 0.0f;
    const float u = 1.0f - (distance < 0.0f ? 0.0f : distance) / reach;
    return u * u;
}

int track_node_light_lamps_near(const TrackNodeLight *light, const uint32_t node[4], int out[], int max)
{
    int n = 0;
    if (light == NULL || light->lamps == NULL || light->lamp_n <= 0 || light->nodes == 0 || node[0] == TRACK_NODE_LIGHT_NO_NODE) return 0;
    const int64_t at = track_node_light_old_node(light, node[0]), reach = light->lamp_reach_nodes + 2, size = (int64_t)light->nodes;
    for (int w = -1; w <= 1; w++) {   /* the ring's seam: the lamps just past the end are just before the start */
        if (w != 0 && !light->lamp_ring) continue;
        const int64_t lo = at - reach - (int64_t)w * size, hi = at + reach - (int64_t)w * size;   /* the lamp node range this wrap sees */
        int a = 0, b = light->lamp_n;
        while (a < b) {
            const int mid = (a + b) / 2;
            if ((int64_t)light->lamps[mid].node < lo) a = mid + 1;
            else b = mid;
        }
        for (int i = a; i < light->lamp_n && (int64_t)light->lamps[i].node <= hi && n < max; i++) out[n++] = i;
    }
    return n;
}

float track_node_light_lamp_at(const TrackNodeLight *light, const int idx[], int n, double x, double z, float rgb[3])
{
    float heat = 0.0f, sum[3] = { 0, 0, 0 };
    rgb[0] = rgb[1] = rgb[2] = 0.0f;   /* none where no lamp reaches */
    for (int i = 0; i < n; i++) {
        const TrackNodeLightLamp *const l = &light->lamps[idx[i]];
        const double dx = x - l->x, dz = z - l->z;
        double distance = sqrt(dx * dx + dz * dz);
        if (l->reach_along > 0.0f) {   /* an ellipse: the distance along the road counts as reach / reach_along times as far */
            const double on = dx * l->along[0] + dz * l->along[1], across = dx * l->along[1] - dz * l->along[0];
            const double stretched = on * l->reach / l->reach_along;
            distance = sqrt(stretched * stretched + across * across);
        }
        const float f = track_node_light_lamp_falloff((float)distance, l->reach);
        if (f <= 0.0f) continue;
        heat += f;
        for (int k = 0; k < 3; k++) sum[k] += l->rgb[k] * f;
    }
    if (heat <= 0.0f) return 0.0f;
    float top = 0.0f;
    for (int k = 0; k < 3; k++) top = sum[k] > top ? sum[k] : top;
    for (int k = 0; k < 3; k++) rgb[k] = sum[k] / top;
    return top;
}

uint8_t track_node_light_object_lamp(const TrackNodeLight *light, uint32_t node, double x, double z, uint8_t level)
{
    static const int TERRAIN[] = { 2, 3, 4, 5, 7, 8, 9, 10 };   /* track_node_light_node_xz's points */
    enum { NEAR_MAX = 24 };
    if (light == NULL || light->lamp_lift == NULL || light->lamps == NULL || light->lamp_n <= 0 || !light->lamp_placed || light->nodes == 0 || node == TRACK_NODE_LIGHT_NO_NODE) return level;
    const uint32_t nodes[4] = { node, node, node, node };
    int idx[NEAR_MAX];
    const int n = track_node_light_lamps_near(light, nodes, idx, NEAR_MAX);
    if (n == 0) return level;
    float rgb[3];
    float own = LAMP_BAKE * track_node_light_lamp_at(light, idx, n, x, z, rgb);
    if (own > 1.0f) own = 1.0f;
    const size_t base = (size_t)(track_node_light_old_node(light, node) % light->nodes) * TRACK_NODE_LIGHT_POINTS;
    unsigned sum = 0;
    for (size_t i = 0; i < sizeof TERRAIN / sizeof *TERRAIN; i++) sum += light->lamp_lift[base + (size_t)old_point(light, TERRAIN[i])];
    const float baked = (float)sum / (255.0f * (float)(sizeof TERRAIN / sizeof *TERRAIN));
    if (own <= baked || baked >= 1.0f) return level;
    const float lift = (own - baked) / (1.0f - baked);
    return (uint8_t)((float)level + (float)(TRACK_NODE_LIGHT_FULL - level) * lift + 0.5f);
}

int track_node_light_lamp_car(const TrackNodeLight *light, uint32_t node, double x, double z, float out[3])
{
    enum { RATIO_MAX = 2, NEAR_MAX = 16 };
    int idx[NEAR_MAX];
    const uint32_t nodes[4] = { node, node, node, node };
    const int n = track_node_light_lamps_near(light, nodes, idx, NEAR_MAX);
    float rgb[3];
    const float top = n > 0 ? track_node_light_lamp_at(light, idx, n, x, z, rgb) : 0.0f;
    if (top <= 0.0f) return 0;
    int changed = 0, tunnel_only = 1;   /* port (tunnel_lamps): a tunnel's lamps lift the car to full light, not to the street lamp's ceiling */
    for (int i = 0; i < n; i++) tunnel_only = tunnel_only && light->lamps[idx[i]].tunnel;
    const float ceiling = tunnel_only ? 1.0f : (float)RATIO_MAX;
    for (int k = 0; k < 3; k++) {
        const float lifted = out[k] + LAMP_CAR_ADD * top * rgb[k];
        if (lifted > out[k]) out[k] = lifted < ceiling ? lifted : (out[k] > ceiling ? out[k] : ceiling), changed = 1;
    }
    return changed;
}

int track_node_light_car_pool(const TrackNodeLight *light, uint32_t node, float out[3])
{
    static const float LAMP_GAIN[3] = { 2.6f, 2.4f, 1.8f };   /* at the lamp: the car's ratio times this, warm (the world's tint is dark, a ratio of 1 is the night as it is) */
    enum { RATIO_MAX = 2 };
    if (light == NULL || light->car_pool == NULL || light->nodes == 0 || out == NULL) return 0;
    const uint8_t c = light->car_pool[node % light->nodes];
    if (c == 0) return 0;
    for (int k = 0; k < 3; k++) {
        const float lifted = out[k] * (1.0f + (LAMP_GAIN[k] - 1.0f) * (float)c / 4.0f);
        if (lifted > out[k]) out[k] = lifted < (float)RATIO_MAX ? lifted : (out[k] > (float)RATIO_MAX ? out[k] : (float)RATIO_MAX);
    }
    return 1;
}
