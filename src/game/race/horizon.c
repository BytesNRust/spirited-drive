#include "game/race/horizon.h"

#include "game/sys/fixed_math.h"

#include <string.h>

/* (value * scale) >> 16 rounded half up, low 32 bits. */
static int32_t mul_round(int32_t value, int32_t scale)
{
    const int64_t p = (int64_t)value * scale;
    return (int32_t)((uint32_t)((uint64_t)p >> 16) + (uint32_t)(((uint64_t)p >> 15) & 1));
}

void horizon_tables(HorizonTables *t, int width, int fov, int32_t viewport_scale)
{
    if (width > HORIZON_MAX_WIDTH) width = HORIZON_MAX_WIDTH;
    const int half_fov = fov / 2;
    /* 00405154: the bitmap spans 0x500 pixels per turn. */
    const int32_t span = (int32_t)((uint32_t)(((fov << 8) / 0x168) * 0x50000) >> 16);
    const int32_t scale = viewport_scale * width / 0x140;
    const int half_w = width / 2;
    const int32_t angle = (half_fov << 16) / 0x168;
    const int32_t c = fx_cos16(angle);
    const int32_t tangent = c == 0 ? 0x11 : fx_div(fx_sin16(angle), c);
    const int32_t focal = fx_div(half_w << 16, tangent);
    const int32_t per_angle = fx_div(span / 2, angle);
    memset(t, 0, sizeof *t);
    if (half_fov <= 10) return;
    t->scale = scale;
    const int32_t step = fx_div(0x10000, scale);
    int32_t acc = 0;
    for (int i = 0; i < HORIZON_ROWS; i++, acc += step) t->row_map[i] = (uint8_t)(acc >> 16);
    t->width = width;
    for (int i = 0; i <= half_w; i++) {
        const int32_t column = mul_round(fx_atan2(i << 16, focal), per_angle);
        t->columns[half_w + i] = column;
        t->columns[half_w - i] = -column;
    }
}

int32_t horizon_line(const View *view, int world_shift, int32_t yaw, const int32_t matrix[9])
{
    const uint32_t a = (uint32_t)(yaw >> 14);
    const int32_t far[3] = { (fx_sin(a) * 0x100) >> world_shift, 0, (fx_cos(a) * 0x100) >> world_shift };
    int32_t cam[3], screen[2];
    mat3_apply(far, matrix, cam);
    view_project(view, cam, screen);
    return screen[1];
}

/* dst[i] = src[base + columns[i]] (0048cc49), skipping reads outside the
 * bitmap. */
static void gather(uint8_t *dst, const FshEntry *src, int32_t base, const int32_t *columns, int count)
{
    const int64_t size = (int64_t)src->width * src->height;
    for (int i = 0; i < count; i++) {
        const int64_t at = (int64_t)base + columns[i];
        if (at >= 0 && at < size) dst[i] = src->payload[at];
    }
}

/* 004829b0 over the viewport's width, clipped. */
static void fill(const HorizonTarget *target, const ClipRect *clip, int32_t y, int32_t rows, uint8_t colour)
{
    if (y < clip->y0) rows -= clip->y0 - y, y = clip->y0;
    if (y + rows > clip->y1) rows = clip->y1 - y;
    for (int32_t r = 0; r < rows; r++)
        memset(target->pixels + (size_t)(y + r) * target->pitch + clip->x0, colour, (size_t)(clip->x1 - clip->x0));
}

void horizon_draw_flat(const HorizonTables *t, const HorizonTarget *target, const ClipRect *clip, int left,
                       const FshEntry *horz, int32_t yaw, int mirrored, int32_t line)
{
    if (t->width == 0 || line == INT32_MIN) return;
    const int32_t w = horz->width;
    /* 0043ae64: the bitmap's horizon row (misc[1]) sits on the line; it
     * turns 0x500 pixels per turn. */
    const int32_t turn = mirrored ? -yaw : yaw;
    const int32_t x_offset = (mul_round(turn, 0x50000) >> 16) % w;
    int32_t y = line - ((int32_t)((uint32_t)horz->misc[1] * (uint32_t)t->scale) >> 16);
    const int32_t bottom = y + ((int32_t)((uint32_t)(horz->height - 1) * (uint32_t)t->scale) >> 16);
    /* Above and below the bitmap: its first and last pixels. */
    fill(target, clip, clip->y0, y - clip->y0, horz->payload[0]);
    fill(target, clip, bottom, clip->y1 - bottom, horz->payload[(size_t)horz->width * horz->height - 1]);
    /* 0043a18c */
    int32_t rows = mul_round(t->scale, horz->height - 1);
    int skip = 0;
    if (y < clip->y0) {
        skip = clip->y0 - y;
        y = clip->y0;
        rows -= skip;
    }
    if (clip->y1 < y + rows) rows = clip->y1 - y;
    int first_row = 1;
    int split = 0;
    if (t->columns[0] + x_offset < 0) {
        first_row++;
        while (split < t->width && t->columns[split] + x_offset < 0) split++;
    } else {
        while (split < t->width && t->columns[split] + x_offset < w) split++;
    }
    for (int r = 0; r < rows; r++) {
        const int source_row = skip + r < HORIZON_ROWS ? t->row_map[skip + r] : t->row_map[HORIZON_ROWS - 1];
        const int32_t base = x_offset + w * (first_row + source_row);
        uint8_t *dst = target->pixels + (size_t)(y + r) * target->pitch + left;
        gather(dst, horz, base, t->columns, split);
        gather(dst + split, horz, base - w, t->columns + split, t->width - split);
    }
}
