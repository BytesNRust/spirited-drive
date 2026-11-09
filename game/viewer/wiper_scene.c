#include "viewer/wiper_scene.h"

#include "game/race/dash_mask.h"
#include "viewer/layer_compose.h"
#include "viewer/screen_layout.h"
#include "viewer/track_load.h"

#include <math.h>
#include <string.h>

/* port (wipers): where the blades are on the dash (layer pixels), for every dash from its art, as a tandem pair on the FULL windscreen: the dash view shows only about 62% of the glass, the rest is past the right edge, so
 * the glass's width is the visible width over WIPER_VISIBLE_SHARE; the left (driver) pivot is low, a gap right of the wheel's left rim, a little below the glass's bottom edge (hidden by the dash: the blade comes out from behind the wheel and the dash top, as in DriveClub), the second near the glass's centre (often past the view), both blades the same length, parked flat to the
 * right of their pivot along the bottom edge's slope and sweeping together up to the left. The glass is raked and seen from the left of its centre, so a blade turns in the glass's plane and the plane's axes put it
 * on the screen: along the edge (u) as it is, up the glass (v) squashed by WIPER_RAKE. The sweep is cut only against the left pillar and the glass's top (the right edge of the view cuts nothing). */
enum { WIPER_PROBE_X = 300, WIPER_SPAN_DEG = 90, WIPER_FREE_X = 540, WIPER_LEFT_PIVOT_X = 333, WIPER_PIVOT_DROP = 8, WIPER_FIT_FROM = 7 };   /* the driver's pivot: a gap right of the wheel's left rim (wheel_left_rim; WIPER_LEFT_PIVOT_X is its fallback), y this far below the edge */
static const float WIPER_VISIBLE_SHARE = 0.62f, WIPER_RAKE = 0.55f, WIPER_LEN_SHARE = 0.45f;

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

/* the glass's lowest row in column x, less 2 (inside it); -1 when the column has none */
float wiper_scene_windscreen_bottom(const uint8_t *flags, int x)
{
    int y = SCREEN_H - 1;
    while (y >= 0 && flags[y * SCREEN_W + x] == 0) y--;
    return y < 0 ? -1.0f : (float)y - 2.0f;
}

/* the glass's extent along a row: its first and last flagged columns */
void wiper_scene_glass_extent(const uint8_t *flags, int y, int *left, int *right)
{
    int x = 0;
    while (x < SCREEN_W - 1 && flags[y * SCREEN_W + x] == 0) x++;
    *left = x;
    x = SCREEN_W - 1;
    while (x > 0 && flags[y * SCREEN_W + x] == 0) x--;
    *right = x;
}

/* the lowest row with a wide stretch of glass (the windscreen's bottom, wheel and all), 10 rows up from it */
int wiper_scene_glass_row(const uint8_t *flags)
{
    for (int y = SCREEN_H - 1; y > HUD_BAR_HEIGHT; y--) {
        int n = 0;
        for (int x = 0; x < SCREEN_W; x++) n += flags[y * SCREEN_W + x] != 0;
        if (n >= 250) return y - 10;
    }
    return SCREEN_H / 2;
}

/* how far (degrees) a blade turns from its park and stays on the glass: the angle before the first at which a point of its outer 40%, is off the art's glass, past the left side or above the
 * view. Points in the right part of the view (WIPER_FREE_X on) are not looked at. */
float wiper_scene_blade_fit(const uint8_t *art, const uint8_t *mask, const WiperBlade *b, float max_span)
{
    float ok = 0.0f;
    const float rad = 3.14159265f / 180.0f;
    for (float a = 24.0f;   /* the bottom edge is curved, a line is only its slope: the first degrees are the edge itself */ a <= max_span + 1.9f; a += 2.0f) {
        if (a > max_span) a = max_span;   /* the last step ends exactly at the top */
        for (int k = WIPER_FIT_FROM; k <= 10; k++) {   /* the blade's outer part: its root is behind the wheel and the dash */
            float dx, dy;
            wiper_offset(b, b->len * (float)k / 10.0f, b->park + a * rad, &dx, &dy);
            const int px = (int)(b->x + dx), py = (int)(b->y + dy);
            if (px >= WIPER_FREE_X) continue;   /* the right part of the view: the mirror is drawn over it and the right pillar is off the view */
            if (px < 0 || py < 0 || py >= SCREEN_H || !wiper_glass(art, mask, px, py)) return ok;
        }
        ok = a;
        if (a >= max_span) break;
    }
    return ok;
}

/* the x of the steering wheel's left rim (its outer edge), read from the art: where the glass's lower boundary is pushed above the edge's line by the wheel's top, the bump is the arc of a circle, whose chord (c) and height (h)
 * give its radius (c*c/4 + h*h) / 2h and its centre; the rim is the centre less the radius. `fallback` when there is no bump. */
float wiper_scene_wheel_left_rim(const uint8_t *flags, float ya, float tn, int xa, float fallback)
{
    int x1 = -1, x2 = -1;
    float h = 0.0f;
    for (int x = 0; x < SCREEN_W; x++) {
        int y = SCREEN_H - 1;
        while (y > 0 && flags[y * SCREEN_W + x] == 0) y--;   /* the glass's lowest pixel in this column */
        const float up = (ya - tn * (float)(x - xa)) - (float)y;   /* how far above the edge's line */
        if (up > 6.0f) {
            if (x1 < 0) x1 = x;
            x2 = x;
            if (up > h) h = up;
        }
    }
    if (x1 < 0 || x2 - x1 < 40 || h < 8.0f) return fallback;
    const float c = (float)(x2 - x1), r = (c * c / 4.0f + h * h) / (2.0f * h);
    return (float)(x1 + x2) / 2.0f - r;
}

/* the blades on the dash layer (pixels) and how far they sweep (degrees), measured from the art and the flags (the layer's own coordinates): see the comment above. */
void wiper_scene_measure(const uint8_t *art, const uint8_t *mask, const uint8_t *flags, WiperBlade layer[WIPER_BLADES], float *span_out)
{
    const float PI = 3.14159265f, deg = PI / 180.0f;
    float span;
    const int row = wiper_scene_glass_row(flags);
    int gl, gr;
    wiper_scene_glass_extent(flags, row, &gl, &gr);
    const float seen = (float)(gr - gl), full = seen / WIPER_VISIBLE_SHARE;
    const int xa = gl + (int)(seen * 0.1f), xb = gl + (int)(seen * 0.9f);
    const float ya = wiper_scene_windscreen_bottom(flags, xa), yb = wiper_scene_windscreen_bottom(flags, xb);
    const float slope = atan2f(ya - yb, (float)(xb - xa)), tn = tanf(slope);   /* the edge's, up to the right */
    const float rim = wiper_scene_wheel_left_rim(flags, ya, tn, xa, (float)WIPER_LEFT_PIVOT_X), gap = (float)gl + full * 0.5f - (float)WIPER_LEFT_PIVOT_X, len = full * WIPER_LEN_SHARE;
    const float lx = rim + gap, rx = rim + 2.0f * gap;   /* the driver's pivot a gap right of the wheel's left rim, the second a gap further */
    const float ux = cosf(slope), uy = sinf(slope), vx = -sinf(slope) * WIPER_RAKE, vy = cosf(slope) * WIPER_RAKE;
    const WiperBlade pair[WIPER_BLADES] = {
        { lx, ya - tn * (lx - (float)xa) + (float)WIPER_PIVOT_DROP, len, slope, 0.0f, ux, uy, vx, vy, (float)SCREEN_W },
        { rx, ya - tn * (rx - (float)xa), len, slope, 0.0f, ux, uy, vx, vy, (float)SCREEN_W }
    };
    layer[0] = pair[0], layer[1] = pair[1];
    /* the top of the sweep is vertical ON THE SCREEN: the plane angle at which the blade's screen x offset is nil (cos t * ux + sin t * vx = 0), not 90 degrees in the glass's plane, which the slope and the raking tilt */
    const float top_plane = atan2f(ux, -vx), max_span = (top_plane - slope) / deg;
    span = wiper_scene_blade_fit(art, mask, &layer[0], max_span);
    const float right = wiper_scene_blade_fit(art, mask, &layer[1], max_span);
    if (right < span) span = right;
    if (span < 50.0f) span = 50.0f;
    *span_out = span;
}
