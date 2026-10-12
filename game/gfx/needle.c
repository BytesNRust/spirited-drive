#include "game/gfx/needle.h"

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
