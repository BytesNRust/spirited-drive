#include "game/race/view.h"

#include "game/sys/fixed_math.h"

enum { NEAR_Z = 10 };

/* 00475fab: angles in degrees become 10-bit; focal = half * cos / sin as an
 * unsigned 64-by-32 division. */
static int32_t focal_length(int degrees, uint32_t half)
{
    const uint32_t angle = (uint32_t)(degrees << 10) / 0x168;
    const uint32_t num = (uint32_t)fx_cos(angle) * half;
    const uint32_t den = (uint32_t)fx_sin(angle);
    return den == 0 ? 0 : (int32_t)(num / den);
}

void view_init(View *view, int left, int top, int width, int height, int fov_x, int fov_y)
{
    const uint32_t half_w = (uint32_t)width >> 1, half_h = (uint32_t)height >> 1;
    view->centre_x = left + (int32_t)half_w;   /* 00475f68 + 00475fab */
    view->centre_y = top + (int32_t)half_h;
    view->focal_x = focal_length(fov_x / 2, half_w);
    /* port: 00484294 derives a zero vertical field of view from the aspect; every caller passes the horizontal one */
    view->focal_y = -focal_length(fov_y / 2, half_h);
    view->near_z = NEAR_Z;
    /* 00475e44 */
    for (int i = 0; i < VIEW_RECIPROCALS; i++)
        view->recip[i] = (int32_t)((view->focal_x << 16) / (i < 10 ? 10 : i));
}

/* imul + shrd + adc: (value * reciprocal) >> shift, rounded half up, low 32
 * bits. Like x86, the shift count is taken mod 32; with a count of 0 the
 * carry left by imul (product did not fit 32 bits) is added. Only depths far
 * beyond any scene reach those cases. */
static int32_t scale(int32_t value, int32_t reciprocal, int shift)
{
    const int64_t p = (int64_t)value * reciprocal;
    const uint32_t low = (uint32_t)(uint64_t)p, high = (uint32_t)((uint64_t)p >> 32);
    const int b = shift & 31;
    if (b == 0) return (int32_t)(low + ((int64_t)(int32_t)low != p));
    return (int32_t)(((low >> b) | (high << (32 - b))) + ((low >> (b - 1)) & 1));
}

int view_project(const View *view, const int32_t camera[3], int32_t screen[2])
{
    int32_t z = camera[2];
    if (z <= view->near_z) {
        screen[0] = screen[1] = INT32_MIN;
        return 0;
    }
    int shift = 16;
    if (z > 0xFFFF) z >>= 8, shift += 8;
    if (z > 0xFFFF) z >>= 8, shift += 8;
    if (z > 0xFFF) z >>= 4, shift += 4;
    if (z > 0x7FF) z >>= 2, shift += 2;
    if (z > 0x3FF) z >>= 1, shift += 1;
    const int32_t r = view->recip[z];
    screen[0] = view->centre_x + scale(camera[0], r, shift);
    screen[1] = view->centre_y - scale(camera[1], r, shift);
    return 1;
}
