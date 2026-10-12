/* The dash's track map (00422ce0) and its pieces. */
#include "game/race/hud_map.h"

#include "game/gfx/blit.h"
#include "game/gfx/clip.h"
#include "game/gfx/flat.h"
#include "game/sys/fixed_math.h"
#include "game/sys/shape_bank.h"

#include <string.h>

/* 004b9178 / 004b919c: the road's first and last steps about the car;
 * 004b9574: the start node (tracks 0..2, by variant). */
static const int32_t ROAD_FIRST[HUD_MAP_TRACKS] = { -29, -29, -29, -19, -29, -29, -29, -31, -29 };
static const int32_t ROAD_LAST[HUD_MAP_TRACKS] = { 30, 30, 30, 20, 30, 30, 30, 32, 30 };
static const int16_t START_NODE[3 * 3] = { 0x6cc, 0x62c, 0x7e4, 0x6cc, 0x67c, 0x67c, 0x76c, 0x854, 0x7c4 };

/* 00423aec */
void hud_map_marker(QuadTarget *t, uint8_t *bank, int32_t high, int32_t x, int32_t y, uint32_t colour,
                    int32_t surface_w, int32_t surface_h)
{
    if (high != 0) {
        clip_set(t, 2, 0x72, 9, 0x85, surface_w, surface_h);
        flat_fill_rect(t, x - 3, y - 3, 7, 5, colour);
        flat_line(t, x - 1, y - 4, x + 1, y - 4, colour);
        flat_line(t, x - 1, y + 2, x + 1, y + 2, colour);
        const uint8_t *pos = shape_bank_find(bank, (const uint8_t *)"pos1");
        if (pos == NULL) return;
        blit_shape_at_transparent(t, pos, x - 4, y - 5);
        clip_set(t, 2, 0x72, 2, 0x8c, surface_w, surface_h);
        return;
    }
    clip_set(t, 1, 0x39, 6, 0x30, surface_w, surface_h);
    flat_fill_rect(t, x - 2, y + 1, 4, 4, colour);
    const uint8_t *pos = shape_bank_find(bank, (const uint8_t *)"pos1");
    if (pos == NULL) return;
    blit_shape_at_transparent(t, pos, x - 3, y);
    clip_set(t, 1, 0x39, 1, 0x35, surface_w, surface_h);
}

typedef struct {
    const HudMapInputs *in;
    int shift;                  /* 0x13 high, 0x14 low */
    int32_t ox, oz, cosine, sine, sign;
} MapTurn;

static TriNode map_node(const HudMapInputs *in, int32_t node)
{
    TriNode n;
    if (tri_node(in->tri, (uint32_t)(node & in->node_mask), &n) != TRI_OK) memset(&n, 0, sizeof n);
    return n;
}

/* A node about the viewed car's, turned: x across, y ahead. */
static void map_turn(const MapTurn *m, int32_t node, int32_t *x, int32_t *y)
{
    const TriNode n = map_node(m->in, node);
    const int32_t dx = (n.position.x >> m->shift) - m->ox, dz = (n.position.z >> m->shift) - m->oz;
    *x = (int32_t)((uint32_t)(fx_mul_round(m->cosine, dx) - fx_mul_round(m->sine, dz)) * (uint32_t)m->sign);
    *y = fx_mul_round(m->sine, dx) + fx_mul_round(m->cosine, dz);
}

static int32_t abs32(int32_t v)
{
    return v > 0 ? v : (int32_t)(0u - (uint32_t)v);
}

/* The road's node `step` steps from the car's (16-bit, as the original). */
static int16_t road_node(const HudMapInputs *in, int16_t at, int32_t step)
{
    int16_t s = (int16_t)(in->track == 3 ? (step * 0x6c) >> 3 : step << 4);
    s = (int16_t)(s + at);
    if (in->track < 3) {
        if (s < 0) return 0;
        const int16_t last = START_NODE[in->track * 3 + in->variant];
        if (s > last + 0x3c) return (int16_t)(last + 0x3c);
        return s;
    }
    if (s < 0) s = (int16_t)(s + (int16_t)in->ring_nodes);
    return s;
}

static void map_road(QuadTarget *t, const HudMapInputs *in, const MapTurn *m, int16_t at, int32_t bx, int32_t by)
{
    int16_t first = (int16_t)(at - (int16_t)in->span);
    if (in->track < 3) {
        if (first < 0) first = 0;
    } else if (first < 0) {
        first = (int16_t)(first + (int16_t)in->ring_nodes);
    }
    int32_t px, py;
    map_turn(m, first, &px, &py);
    for (int16_t step = (int16_t)ROAD_FIRST[in->track]; step <= ROAD_LAST[in->track]; step++) {
        int32_t x, y;
        map_turn(m, road_node(in, at, step), &x, &y);
        flat_line(t, bx + px, by - py, bx + x, by - y, in->road_colour);
        px = x, py = y;
    }
}

/* The direction at the start node: a short line along the road there. */
static void map_direction(QuadTarget *t, const HudMapInputs *in, const MapTurn *m, int32_t bx, int32_t by)
{
    const int32_t start = in->track < 3 ? START_NODE[in->track * 3 + in->variant] : 0;
    int32_t ax, ay, cx, cy;
    map_turn(m, start, &ax, &ay);
    map_turn(m, start + 8, &cx, &cy);
    const int32_t dy = cy - ay;
    const int32_t angle = fx_atan2_10(cx - ax, dy + (dy >> 2));
    const int32_t p = fx_mul_round(4, -fx_cos((uint32_t)angle));
    const int32_t q = fx_mul_round(4, fx_sin((uint32_t)angle));
    const int32_t r = q - (q >> 2) + (q >> 4);
    flat_line(t, ax + bx + (p >> 1), by - (ay + (r >> 1)), ax + bx - (p - (p >> 1)), by - (ay - (r - (r >> 1))),
              in->arrow_colour);
}

/* The nodes between the car and the viewed one, the shorter way round. */
static int32_t node_distance(int16_t d, int32_t ring)
{
    const int32_t a = abs32((int32_t)((uint32_t)d + (uint32_t)ring));
    const int32_t b = abs32((int32_t)((uint32_t)ring - (uint32_t)d));
    const int32_t round = b <= a ? b : a;
    return abs32(d) < round ? abs32(d) : round;
}

/* A car off the map: a triangle and 'aro1' (ahead of the viewed car's
 * place) or 'aru1' at its column. */
static int map_arrow(QuadTarget *t, const HudMapInputs *in, int i, int32_t l)
{
    const int32_t column = in->high != 0 ? (in->race_type == 1 ? l + 0x5b : l + 0x60 - 0xd * i)
                                         : (in->race_type == 1 ? l + 0x2e : l + 0x31 - 7 * i);
    const int32_t x = (int16_t)column;
    const uint32_t colour = in->car_colours[i];
    const int up = in->race_type == 1 ? in->place == 1 : (int32_t)in->car_places[i] > in->place;
    const char *name;
    int32_t y;
    if (up) {
        y = (int16_t)(in->high != 0 ? l + 0x83 : l + 0x2f);
        const int32_t far = in->high != 0 ? 9 : 5, tip = in->high != 0 ? 5 : 3;
        const int32_t pts[6] = { x + 1, y + 1, x + far, y + 1, x + tip, y + tip };
        flat_polygon(t, colour, 3, pts);
        name = "aro1";
    } else {
        const int32_t far = in->high != 0 ? 9 : 5, tip = in->high != 0 ? 5 : 3;
        const int32_t pts[6] = { x + 1, l + tip, x + far, l + tip, x + tip, l + 1 };
        flat_polygon(t, colour, 3, pts);
        y = in->high != 0 ? l : l - 1;
        name = "aru1";
    }
    const uint8_t *shape = shape_bank_find(in->bank, (const uint8_t *)name);
    if (shape == NULL) return -1;
    blit_shape_at_transparent(t, shape, x, y);
    return 0;
}

/* 00422ce0 */
int hud_map_draw(const HudMapOps *ops, const HudMapInputs *in, int32_t *blink)
{
    const int high = in->high != 0;
    const int32_t l = high ? 2 : 1;
    if (in->use_parts == 0) return 0;
    const int32_t sign = in->mirrored != 0 ? -1 : 1;
    const int32_t mark_x = (int16_t)(high ? l + 0x38 : l + 0x1c), mark_y = (int16_t)(high ? l + 0x46 : l + 0x19);
    const int cars = in->race_type > 1 ? 8 : in->race_type == 0 ? 1 : 2;
    QuadTarget *t = ops->select(ops->ctx, HUD_SURFACE_MAP);
    if (in->chase_kind == 0 && in->dash_camera == 0) {
        if (in->back_saved != 0) blit_shape_at(t, in->back_shape, l, l);
    } else {
        const uint8_t *bg = shape_bank_find(in->bank, (const uint8_t *)"mpbd");
        if (bg == NULL) return -1;
        blit_shape_at_transparent(t, bg, l, l);
    }
    if (high) {
        flat_frame_rect(t, l - 1, l - 1, l + 0x70, l + 0x8a, in->frame_colour);
        flat_frame_rect(t, l - 2, l - 2, l + 0x71, l + 0x8b, in->frame_colour);
    } else {
        flat_frame_rect(t, l - 1, l - 1, l + 0x38, l + 0x34, in->frame_colour);
    }
    const int16_t at = in->car_node[in->viewed];
    MapTurn m = { in, high ? 0x13 : 0x14, 0, 0, 0, 0, sign };
    const TriNode here = map_node(in, at);
    m.ox = here.position.x >> m.shift;
    m.oz = here.position.z >> m.shift;
    const int32_t angle = (int32_t)((uint32_t)(int32_t)here.orientation << 10) >> 14;
    m.cosine = fx_cos((uint32_t)angle);
    m.sine = fx_sin((uint32_t)angle);
    clip_set(t, l, high ? l + 0x70 : l + 0x38, l, high ? l + 0x8a : l + 0x34, in->surface_w, in->surface_h);
    const int32_t bx = high ? l + 0x38 : l + 0x1c, by = high ? l + 0x45 : l + 0x1c;
    map_road(t, in, &m, at, bx, by);
    map_direction(t, in, &m, bx, by);
    for (int i = cars - 1; i >= 0; i--) {
        const int16_t d = (int16_t)(at - in->car_node[i]);
        const int near = (int16_t)node_distance(d, in->ring_nodes) <= in->span;
        int32_t x = 0, y = 0;
        if (near) map_turn(&m, in->car_node[i], &x, &y);
        const int inside = high ? abs32(x) < 0x38 && abs32(y) < 0x3d : abs32(x) < 0x1c && abs32(y) < 0x17;
        if (near && inside) {
            hud_map_marker(t, in->bank, in->high, mark_x + x, mark_y - y, in->car_colours[i], in->surface_w,
                           in->surface_h);
        } else if (map_arrow(t, in, i, l) != 0) {
            return -1;
        }
    }
    if (in->police_present == 0 || (in->police_flags & 4) == 0 || in->police_chase == 0 || in->race_type != 1)
        return 0;
    const int16_t police = in->car_node[2];
    if ((int16_t)abs32((int32_t)at - police) >= 0x140) return 0;
    int32_t x, y;
    map_turn(&m, police, &x, &y);
    const int inside = high ? abs32(x) < 0x38 && abs32(y) < 0x44 : abs32(x) < 0x1c && abs32(y) < 0x1d;
    if (!inside) return 0;
    if (++*blink > 10) *blink = 0;
    hud_map_marker(t, in->bank, in->high, mark_x + x, mark_y - y, in->car_colours[*blink > 5 ? 0 : 6], in->surface_w,
                   in->surface_h);
    return 0;
}
