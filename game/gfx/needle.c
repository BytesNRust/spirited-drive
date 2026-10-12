#include "game/gfx/needle.h"

#include "game/gfx/flat.h"
#include "game/gfx/flat_internal.h"
#include "game/gfx/x86_int.h"

/* imul + shrd 16 + adc 0: bit 15 of the product rounds. */
static int32_t mul_round(int32_t a, int32_t b)
{
    const uint64_t p = (uint64_t)((int64_t)a * b);
    return (int32_t)((uint32_t)(p >> 16) + (uint32_t)((p >> 15) & 1));
}

/* cdq / idiv, then the remainder << 16 (edx:eax) over the same divisor:
 * a / b in 16.16. */
static int32_t div_16(int32_t a, int32_t b)
{
    const int32_t q = x_div(a, b);
    const int32_t r = x_sub(a, x_mul(q, b));
    return x_add(x_div64((int64_t)r * 0x10000, b), x_shl(q, 16));
}

int32_t needle_clip_y(NeedleLine *n)
{
    if (n->y_end < n->top) return 0;
    if (n->y_start >= n->bottom) return 0;
    if (n->y_start < n->top) {
        const int32_t skip = x_sub(n->top, n->y_start);
        n->xa = x_add(n->xa, mul_round(n->slope_xa, skip));
        n->xb = x_add(n->xb, mul_round(n->slope_xb, skip));
        n->y_start = n->top;
    }
    if (n->y_end > n->bottom) n->y_end = n->bottom;
    n->clipped = n->xa < n->left || n->xb >= n->right;
    if (n->xb < n->left) {
        if (n->slope_xb <= 0) return 0;
        const int32_t skip = div_16(x_sub(n->left, n->xb), n->slope_xb);
        n->y_start = x_add(n->y_start, skip);
        if (n->y_start >= n->right) return 0;
        if (n->y_start < n->left) return 0;
        n->xb = n->left;
        n->xa = x_add(n->xa, mul_round(n->slope_xa, skip));
        n->clipped = 1;
    }
    if (n->xa >= n->right) {
        if (n->slope_xa >= 0) return 0;
        const int32_t skip = div_16(x_add(x_sub(n->right, n->xa), 1), n->slope_xa);
        n->y_start = x_add(n->y_start, skip);
        if (n->y_start >= n->right) return 0;
        if (n->y_start < n->left) return 0;
        n->xb = x_add(n->xb, mul_round(n->slope_xb, skip));
        n->xa = x_sub(n->right, 1);
        n->clipped = 1;
    }
    return 1;
}

int32_t needle_clip_x(NeedleLine *n)
{
    if (n->x_end < n->left) return 0;
    if (n->x_start >= n->right) return 0;
    if (n->x_start < n->left) {
        const int32_t skip = x_sub(n->left, n->x_start);
        n->ya = x_add(n->ya, mul_round(n->slope_ya, skip));
        n->yb = x_add(n->yb, mul_round(n->slope_yb, skip));
        n->x_start = n->left;
    }
    if (n->x_end > n->right) n->x_end = n->right;
    n->clipped = n->ya < n->top || n->yb >= n->bottom;
    if (n->yb < n->top) {
        if (n->slope_yb <= 0) return 0;
        const int32_t skip = div_16(x_sub(n->top, n->yb), n->slope_yb);
        n->x_start = x_add(n->x_start, skip);
        if (n->x_start >= n->right) return 0;
        if (n->x_start < n->left) return 0;
        n->yb = n->top;
        n->ya = x_add(n->ya, mul_round(n->slope_ya, skip));
        n->clipped = 1;
    }
    if (n->ya >= n->bottom) {
        if (n->slope_ya >= 0) return 0;
        const int32_t skip = div_16(x_add(x_sub(n->bottom, n->ya), 1), n->slope_ya);
        n->x_start = x_add(n->x_start, skip);
        if (n->x_start >= n->right) return 0;
        if (n->x_start < n->left) return 0;
        n->yb = x_add(n->yb, mul_round(n->slope_yb, skip));
        n->ya = x_sub(n->bottom, 1);
        n->clipped = 1;
    }
    return 1;
}

/* The pixel under *p faded toward the needle's colour by level / 16: the
 * fade block's row level - 1 (n->fade is the block + 4, its first word the
 * colour and its three bytes). Port guard: a level whose row starts before
 * the block (the original reads the heap in front of it) leaves the pixel. */
static void fade_pixel(const NeedleLine *n, uint8_t *p, int32_t level)
{
    const int32_t at = (level - 1) * 256 + *p;
    if (at < -4) return;
    *p = n->fade[at];
}

void needle_span_y(NeedleLine *n, uint8_t *pixels, int32_t pitch)
{
    int32_t row = x_sar(n->y_start, 16);
    while (n->y_start < n->y_end && n->xb < n->right && n->xa >= n->left) {
        const int32_t fb = (n->xb & 0xffff) >> 12;
        const int32_t fa = (0x10000 - (n->xa & 0xffff)) >> 12;
        int32_t count = x_sub(x_sar(n->xb, 16), x_sar(n->xa, 16));
        uint8_t *p = pixels + (intptr_t)row * pitch + x_sar(n->xa, 16);
        if (count == 0) {
            fade_pixel(n, p, fa + fb - 16);
        } else if (count > 0) {
            if (fa != 0) fade_pixel(n, p, fa);
            while (p++, --count != 0) *p = (uint8_t)n->fade_first;
            if (fb != 0) fade_pixel(n, p, fb);
        }
        n->xa = x_add(n->xa, n->slope_xa);
        n->xb = x_add(n->xb, n->slope_xb);
        n->y_start = x_add(n->y_start, n->dy);
        row++;
    }
}

void needle_span_x(NeedleLine *n, uint8_t *pixels, int32_t pitch)
{
    int32_t column = x_sar(n->x_start, 16);
    while (n->x_start < n->x_end && n->yb < n->bottom && n->ya >= n->top) {
        const int32_t fb = (n->yb & 0xffff) >> 12;
        const int32_t fa = (0x10000 - (n->ya & 0xffff)) >> 12;
        int32_t count = x_sub(x_sar(n->yb, 16), x_sar(n->ya, 16));
        uint8_t *p = pixels + (intptr_t)x_sar(n->ya, 16) * pitch + column;
        if (count == 0) {
            if (fa + fb - 16 > 0) fade_pixel(n, p, fa + fb - 16);
        } else if (count > 0) {
            if (fa != 0) fade_pixel(n, p, fa);
            while (p += pitch, --count != 0) *p = (uint8_t)n->fade_first;
            if (fb != 0) fade_pixel(n, p, fb);
        }
        n->ya = x_add(n->ya, n->slope_ya);
        n->yb = x_add(n->yb, n->slope_yb);
        n->x_start = x_add(n->x_start, n->dx);
        column++;
    }
}

/* An edge pixel through the surface's own read and plot (0048f5f0,
 * 0049f3a0): faded by level / 16, dropped outside the clip. */
static void fade_plot(const NeedleLine *n, const QuadTarget *t, int32_t x, int32_t y, int32_t level)
{
    flat_plot(t, x, y, n->fade[(level - 1) * 256 + (int32_t)flat_peek(t, x, y)]);
}

void needle_edges_y(NeedleLine *n, const QuadTarget *t)
{
    int32_t row = x_sar(n->y_start, 16);
    while (n->y_start < n->y_end) {
        const int32_t b = x_sar(n->xb, 16), a = x_sar(n->xa, 16);
        const int32_t left = (int16_t)t->clip_left, right = (int16_t)t->clip_right;
        if (left <= a && b < right) return;
        if (a < left && b < left) return;
        if (a >= right && b >= right) return;
        const int32_t fa = (0x10000 - (n->xa & 0xffff)) >> 12;
        const int32_t fb = (n->xb & 0xffff) >> 12;
        if (b == a) {
            if (fa + fb - 16 > 0) fade_plot(n, t, b, row, fa + fb - 16);
        } else {
            if (fa != 0 && left < a) fade_plot(n, t, a, row, fa);
            if (a + 1 < b) flat_fill_rect(t, a + 1, row, b - (a + 1), 1, n->fade_first & 0xff);
            if (fb != 0 && b < right) fade_plot(n, t, b, row, fb);
        }
        n->xa = x_add(n->xa, n->slope_xa);
        n->xb = x_add(n->xb, n->slope_xb);
        n->y_start = x_add(n->y_start, n->dy);
        row++;
    }
}

void needle_edges_x(NeedleLine *n, const QuadTarget *t)
{
    int32_t column = x_sar(n->x_start, 16);
    while (n->x_start < n->x_end) {
        const int32_t a = x_sar(n->ya, 16), b = x_sar(n->yb, 16);
        const int32_t top = (int16_t)t->clip_top, bottom = (int16_t)t->clip_bottom;
        if (top <= a && b < bottom) return;
        if (a < top && b < top) return;
        if (a >= bottom && b >= bottom) return;
        const int32_t fa = (0x10000 - (n->ya & 0xffff)) >> 12;
        const int32_t fb = (n->yb & 0xffff) >> 12;
        if (a == b) {
            if (fa + fb - 16 > 0) fade_plot(n, t, column, a, fa + fb - 16);
        } else {
            if (fa != 0 && top < a) fade_plot(n, t, column, a, fa);
            if (a + 1 < b) flat_fill_rect(t, column, a + 1, 1, b - (a + 1), n->fade_first & 0xff);
            if (fb != 0 && b < bottom) fade_plot(n, t, column, b, fb);
        }
        n->ya = x_add(n->ya, n->slope_ya);
        n->yb = x_add(n->yb, n->slope_yb);
        n->x_start = x_add(n->x_start, n->dx);
        column++;
    }
}
