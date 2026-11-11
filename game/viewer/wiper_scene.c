#include "viewer/wiper_scene.h"

#include "game/race/dash_mask.h"
#include "viewer/layer_compose.h"
#include "viewer/screen_layout.h"
#include "viewer/track_load.h"

#include <math.h>
#include <string.h>

/* port (wipers): where the blades are, for every dash and every screen width, as a tandem pair on the windscreen the frame shows (wiper_scene_frame_glass: at 21:9 the dash's sides are stretched to the
 * screen's edges, and the glass with them): the pivots and the length are shares of the glass's width at the pair's row (wiper_scene.h, from the owner's sketch), both pivots on the glass's bottom edge (the driver's behind
 * the wheel, which hides that blade's root and, at rest, the part of it behind the rim), both blades the same length, parked flat to the right of their pivot along the bottom edge's slope and sweeping together up to the
 * left. The glass is raked and seen from the left of its centre, so a blade turns in the glass's plane and the plane's axes put it on the screen: along the edge (u) as it is, up the glass (v) squashed by
 * WIPER_RAKE, so a blade is shorter on the screen near the top of its sweep than at rest. The sweep goes a little past upright (the sides of a wide view are swept too) and is cut against the glass. */
enum { WIPER_PROBE_X = 300, WIPER_FIT_FROM = 7, WIPER_PAST_UPRIGHT_DEG = 20 };
static const float WIPER_RAKE = 0.84f;

/* the glass (the art's nothing, or what its mask gives to the blades) */
static int wiper_glass(const uint8_t *art, const uint8_t *mask, int x, int y)
{
    const size_t i = (size_t)y * SCREEN_W + (size_t)x;
    return art[i] == LAYER_KEY || (mask != NULL && mask[i] == DASH_MASK_BLADE);
}

/* the windscreen's glass alone: the art's glass connected, by 4 neighbours, to the first glass found going down from the top of the view (at the middle columns), so the holes of the wheel and the res
 * art are not in it; `flags` (1 = glass) is filled, `stack` is the work area (SCREEN_W * SCREEN_H ints) */
void wiper_scene_glass_flags(const uint8_t *art, const uint8_t *mask, uint8_t *flags, int *stack)
{
    memset(flags, 0, (size_t)SCREEN_W * SCREEN_H);
    size_t top = 0;
    for (int x = WIPER_PROBE_X; x < WIPER_PROBE_X + 200 && top == 0; x += 20)
        for (int y = HUD_BAR_HEIGHT + 10; y < SCREEN_H && top == 0; y++)
            if (wiper_glass(art, mask, x, y)) stack[top++] = y * SCREEN_W + x, flags[y * SCREEN_W + x] = 1;
    while (top > 0) {
        const int at = stack[--top], ax = at % SCREEN_W, ay = at / SCREEN_W;
        const int nx[4] = { ax - 1, ax + 1, ax, ax }, ny[4] = { ay, ay, ay - 1, ay + 1 };
        for (int k = 0; k < 4; k++) {
            if (nx[k] < 0 || nx[k] >= SCREEN_W || ny[k] < 0 || ny[k] >= SCREEN_H) continue;
            const int n = ny[k] * SCREEN_W + nx[k];
            if (flags[n] == 0 && wiper_glass(art, mask, nx[k], ny[k])) flags[n] = 1, stack[top++] = n;
        }
    }
}

void wiper_scene_frame_glass(const uint8_t *flags, int width, uint8_t *map)
{
    if (width <= SCREEN_W) {
        memcpy(map, flags, (size_t)SCREEN_W * SCREEN_H);
        return;
    }
    const int *const column = screen_dash_columns(width);
    for (int y = 0; y < SCREEN_H; y++) {
        const uint8_t *const from = flags + (size_t)screen_dash_y(y) * SCREEN_W;
        uint8_t *const to = map + (size_t)y * (size_t)width;
        for (int x = 0; x < width; x++) to[x] = from[column[x]];
    }
}

/* the glass's lowest row in column x, less 2 (inside it); -1 when the column has none */
float wiper_scene_windscreen_bottom(const uint8_t *map, int width, int x)
{
    int y = SCREEN_H - 1;
    while (y >= 0 && map[(size_t)y * (size_t)width + (size_t)x] == 0) y--;
    return y < 0 ? -1.0f : (float)y - 2.0f;
}

/* the glass's extent along a row: its first and last flagged columns */
void wiper_scene_glass_extent(const uint8_t *map, int width, int y, int *left, int *right)
{
    const uint8_t *const row = map + (size_t)y * (size_t)width;
    int x = 0;
    while (x < width - 1 && row[x] == 0) x++;
    *left = x;
    x = width - 1;
    while (x > 0 && row[x] == 0) x--;
    *right = x;
}

/* the lowest row with a wide stretch of glass (the windscreen's bottom, wheel and all; 250 of 640 columns), 10 rows up from it */
int wiper_scene_glass_row(const uint8_t *map, int width)
{
    for (int y = SCREEN_H - 1; y > HUD_BAR_HEIGHT; y--) {
        int n = 0;
        for (int x = 0; x < width; x++) n += map[(size_t)y * (size_t)width + (size_t)x] != 0;
        if (n * SCREEN_W >= 250 * width) return y - 10;
    }
    return SCREEN_H / 2;
}

/* 1 when a point is outside the windscreen's outline: off the frame, on a row with no glass (above the roof) or left of the row's first glass (the left pillar). What hangs in front of the glass (the
 * mirror, the wheel) is inside the outline: a blade passes behind it, hidden there by the draw */
static int outside_outline(const uint8_t *map, int width, int px, int py)
{
    if (px < 0 || py < 0 || py >= SCREEN_H || px >= width) return px >= width ? 0 : 1;   /* past the right edge: the glass goes on off the view */
    const uint8_t *const row = map + (size_t)py * (size_t)width;
    int x = 0;
    while (x < width && row[x] == 0) x++;
    return x == width || px < x;
}

/* how far (degrees) a blade turns from its park and stays inside the windscreen: the angle before the first at which a point of its outer 30% is outside the outline (outside_outline) */
float wiper_scene_blade_fit(const uint8_t *map, int width, const WiperBlade *b, float max_span)
{
    float ok = 0.0f;
    const float rad = 3.14159265f / 180.0f;
    for (float a = 24.0f;   /* the bottom edge is curved, a line is only its slope: the first degrees are the edge itself */ a <= max_span + 1.9f; a += 2.0f) {
        if (a > max_span) a = max_span;   /* the last step ends exactly at the top */
        for (int k = WIPER_FIT_FROM; k <= 10; k++) {   /* the blade's outer part: its root is behind the wheel and the dash */
            float dx, dy;
            wiper_offset(b, b->len * (float)k / 10.0f, b->park + a * rad, &dx, &dy);
            const int px = (int)(b->x + dx), py = (int)(b->y + dy);
            if (outside_outline(map, width, px, py)) return ok;
        }
        ok = a;
        if (a >= max_span) break;
    }
    return ok;
}

/* the blades on the glass map (its pixels) and how far they sweep (degrees): see the comment above */
void wiper_scene_measure(const uint8_t *map, int width, WiperBlade blades[WIPER_BLADES], float *span_out)
{
    const float deg = 3.14159265f / 180.0f;
    const int row = wiper_scene_glass_row(map, width);
    int gl, gr;
    wiper_scene_glass_extent(map, width, row, &gl, &gr);
    const float seen = (float)(gr - gl);
    const int xa = gl + (int)(seen * 0.1f), xb = gl + (int)(seen * 0.9f);
    const float ya = wiper_scene_windscreen_bottom(map, width, xa), yb = wiper_scene_windscreen_bottom(map, width, xb);
    const float slope = atan2f(ya - yb, (float)(xb - xa)), tn = tanf(slope);   /* the edge's, up to the right */
    const float lx = (float)gl + WIPER_LEFT_SHARE * seen, rx = (float)gl + WIPER_RIGHT_SHARE * seen, len = WIPER_LEN_SHARE * (float)SCREEN_H;
    const float ux = cosf(slope), uy = sinf(slope), vx = -sinf(slope) * WIPER_RAKE, vy = cosf(slope) * WIPER_RAKE;
    const WiperBlade pair[WIPER_BLADES] = {
        { lx, ya - tn * (lx - (float)xa), len, slope, 0.0f, ux, uy, vx, vy, 0.0f },
        { rx, ya - tn * (rx - (float)xa), len, slope, 0.0f, ux, uy, vx, vy, 0.0f }
    };
    blades[0] = pair[0], blades[1] = pair[1];
    /* upright ON THE SCREEN is the plane angle at which the blade's screen x offset is nil (cos t * ux + sin t * vx = 0), not 90 degrees in the glass's plane, which the slope and the raking tilt; the sweep may go
     * WIPER_PAST_UPRIGHT_DEG past it, as far as the glass allows */
    const float upright = atan2f(ux, -vx), max_span = (upright - slope) / deg + (float)WIPER_PAST_UPRIGHT_DEG;
    float span = wiper_scene_blade_fit(map, width, &blades[0], max_span);
    const float right = wiper_scene_blade_fit(map, width, &blades[1], max_span);
    if (right < span) span = right;
    if (span < 50.0f) span = 50.0f;
    *span_out = span;
}
