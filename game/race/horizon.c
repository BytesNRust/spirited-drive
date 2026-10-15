#include "game/race/horizon.h"

#include "game/sys/fixed_math.h"

#include <string.h>

void horizon_tables(HorizonTables *t, int width, int fov, int32_t viewport_scale)
{
    /* port guard: wider ones write past the columns (2 x (width / 2) is the last) */
    if (width > HORIZON_MAX_WIDTH + 1) width = HORIZON_MAX_WIDTH + 1;
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
    if (half_fov <= 10) return;  /* the tables stay as they were */
    t->scale = scale;
    const int32_t step = fx_div(0x10000, scale);
    int32_t acc = 0;
    for (int i = 0; i < HORIZON_ROWS; i++, acc += step) t->row_map[i] = (uint8_t)(acc >> 16);
    t->width = width;
    for (int i = 0; i <= half_w; i++) {
        const int32_t column = fx_mul_round(fx_atan2(i << 16, focal), per_angle);
        t->columns[half_w + i] = column;
        t->columns[half_w - i] = -column;
    }
}

void horizon_point(const View *view, int world_shift, int32_t yaw, const int32_t matrix[9], int32_t screen[2])
{
    const uint32_t a = (uint32_t)(yaw >> 14);
    const int32_t far[3] = { (fx_sin(a) * 0x100) >> world_shift, 0, (fx_cos(a) * 0x100) >> world_shift };
    int32_t cam[3];
    mat3_apply(far, matrix, cam);
    view_project(view, cam, screen);
}

int32_t horizon_line(const View *view, int world_shift, int32_t yaw, const int32_t matrix[9])
{
    int32_t screen[2];
    horizon_point(view, world_shift, yaw, matrix, screen);
    return screen[1];
}

static int32_t scaled(int32_t rows, int32_t scale) { return (int32_t)((uint32_t)rows * (uint32_t)scale) >> 16; }

void horizon_flat(const FshEntry *horz, int32_t yaw, int mirrored, int sky_off, int32_t scale, const HorizonBox *box,
                  int32_t line, const HorizonFlatSink *sink)
{
    const int32_t w = (int16_t)horz->width, h = (int16_t)horz->height;
    const int32_t turn = mirrored ? (int32_t)(0u - (uint32_t)yaw) : yaw;
    const int32_t x_offset = (fx_mul_round(turn, 0x50000) >> 16) % w;
    const int32_t above = sky_off ? 0 : (int16_t)horz->misc[1];
    const int32_t rows = sky_off ? 0 : h;
    const int32_t y = (int32_t)((uint32_t)line - (uint32_t)scaled(above, scale));
    const int32_t bottom = (int32_t)((uint32_t)y + (uint32_t)scaled(rows - 1, scale));
    sink->fill(sink->user, box->x, box->y, box->w, y, horz->payload[0]);
    if (!sky_off) sink->band(sink->user, horz, x_offset, 1, box->x, y);
    sink->fill(sink->user, box->x, bottom, box->w, (int32_t)((uint32_t)box->y + (uint32_t)box->h - (uint32_t)bottom),
               horz->payload[w * h - 1]);
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

typedef struct {
    const HorizonTables *t;
    const HorizonTarget *target;
    const ClipRect *clip;
} FlatDraw;

/* 004829b0 over the clip's width, clipped to its rows. */
static void flat_fill(void *user, int32_t x, int32_t y, int32_t w, int32_t rows, uint8_t colour)
{
    const FlatDraw *d = user;
    (void)x, (void)w;
    fill(d->target, d->clip, y, rows, colour);
}

/* 004fbb10 + i: past the 0x1e0 rows the original reads on into 004fbcf0 (the columns). */
static int row_at(const HorizonTables *t, int32_t i)
{
    if (i < HORIZON_ROWS) return t->row_map[i];
    const int32_t c = (i - HORIZON_ROWS) / 4;
    if (c > HORIZON_MAX_WIDTH) return t->row_map[HORIZON_ROWS - 1];  /* port guard: past 004fc6f4 */
    return (int)(((uint32_t)t->columns[c] >> (8 * ((i - HORIZON_ROWS) % 4))) & 0xFF);
}

void horizon_band(const HorizonTables *t, const HorizonTarget *target, int32_t clip_top, int32_t clip_bottom,
                  const FshEntry *horz, int32_t x_offset, int32_t first_row, int32_t x, int32_t y)
{
    const int32_t w = (int16_t)horz->width;
    int32_t rows = fx_mul_round(t->scale, (int16_t)horz->height - 1);
    int32_t skip = 0;
    if (y < clip_top) {
        skip = clip_top - y;
        y = clip_top;
        rows -= skip;
    }
    if (clip_bottom < y + rows) rows = clip_bottom - y;
    int split = 0;
    if (t->columns[0] + x_offset < 0) {
        first_row++;
        while (split < t->width && t->columns[split] + x_offset < 0) split++;
    } else {
        while (split < t->width && t->columns[split] + x_offset < w) split++;
    }
    for (int32_t r = 0; r < rows; r++) {
        const int32_t base = x_offset + w * (first_row + row_at(t, skip + r));
        uint8_t *dst = target->pixels + (size_t)(y + r) * target->pitch + x;
        if (split != 0) gather(dst, horz, base, t->columns, split);
        if (t->width - split != 0) gather(dst + split, horz, base - w, t->columns + split, t->width - split);
    }
}

static void flat_band(void *user, const FshEntry *horz, int32_t x_offset, int32_t first_row, int32_t x, int32_t y)
{
    const FlatDraw *d = user;
    horizon_band(d->t, d->target, d->clip->y0, d->clip->y1, horz, x_offset, first_row, x, y);
}

void horizon_draw_flat(const HorizonTables *t, const HorizonTarget *target, const ClipRect *clip,
                       const FshEntry *horz, int32_t yaw, int mirrored, int sky_off, int32_t line)
{
    if (t->width == 0 || line == INT32_MIN) return;
    const FlatDraw d = { t, target, clip };
    const HorizonFlatSink sink = { (void *)&d, flat_fill, flat_band };
    const HorizonBox box = { clip->x0, clip->y0, clip->x1 - clip->x0, clip->y1 - clip->y0 };
    horizon_flat(horz, yaw, mirrored, sky_off, t->scale, &box, line, &sink);
}
