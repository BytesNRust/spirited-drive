#include "game/sys/video.h"

#include "game/sys/palette.h"
#include "platform/platform.h"

#include <stdlib.h>
#include <string.h>

enum { TRANSPARENT = 0xFF };

static uint8_t *back;
static uint8_t *target;  /* where drawing goes: back, or a picture (00473680) */
static int target_w = VIDEO_WIDTH, target_h = VIDEO_HEIGHT;
static uint8_t *front;
static uint8_t *composed;
static uint8_t palette[PALETTE_BYTES];  /* 0052ced8 / 0052f178 */
static const FshEntry *overlay;
static int overlay_x, overlay_y;

static void compose_overlay(void)
{
    memcpy(composed, front, VIDEO_WIDTH * VIDEO_HEIGHT);
    for (int row = 0; row < overlay->height; row++) {
        const int dy = overlay_y + row;
        if (dy < 0 || dy >= VIDEO_HEIGHT) continue;
        for (int col = 0; col < overlay->width; col++) {
            const int dx = overlay_x + col;
            const uint8_t c = overlay->payload[(size_t)row * overlay->width + col];
            if (c != TRANSPARENT && dx >= 0 && dx < VIDEO_WIDTH) composed[dy * VIDEO_WIDTH + dx] = c;
        }
    }
}

static void show(void)
{
    PlatformColor colors[PALETTE_COLORS];
    for (int i = 0; i < PALETTE_COLORS; i++) {
        colors[i] = (PlatformColor){ palette[3 * i], palette[3 * i + 1], palette[3 * i + 2] };
    }
    if (overlay != NULL) compose_overlay();
    platform_present(overlay != NULL ? composed : front, colors);
}

void video_set_overlay(const FshEntry *shape, int x, int y)
{
    overlay = shape;
    overlay_x = x;
    overlay_y = y;
    if (front != NULL) show();
}

int video_init(void)
{
    back = calloc(VIDEO_WIDTH * VIDEO_HEIGHT, 1);
    front = calloc(VIDEO_WIDTH * VIDEO_HEIGHT, 1);
    composed = calloc(VIDEO_WIDTH * VIDEO_HEIGHT, 1);
    memset(palette, 0, sizeof palette);
    overlay = NULL;
    if (back == NULL || front == NULL || composed == NULL) {
        video_shutdown();
        return 0;
    }
    return 1;
}

void video_shutdown(void)
{
    free(back);
    free(front);
    free(composed);
    back = front = composed = NULL;
    overlay = NULL;
}

uint8_t *video_back_buffer(void)
{
    return back;
}

static uint8_t *drawn(void)
{
    return target != NULL ? target : back;
}

void video_select_target(uint8_t *pixels, int w, int h)
{
    target = pixels != NULL ? pixels : back;
    target_w = pixels != NULL ? w : VIDEO_WIDTH;
    target_h = pixels != NULL ? h : VIDEO_HEIGHT;
}

uint8_t *video_target(int *w, int *h)
{
    *w = target_w, *h = target_h;
    return drawn();
}

void video_clear_back(void)
{
    memset(back, 0, VIDEO_WIDTH * VIDEO_HEIGHT);
}

void video_flip(void)
{
    memcpy(front, back, VIDEO_WIDTH * VIDEO_HEIGHT);
    show();
}

void video_wait_vblank(void)
{
    platform_wait_vblank();
}

void video_set_palette(int first, int count, const uint8_t *rgb)
{
    memcpy(palette + first * 3, rgb, (size_t)count * 3);
    show();
}

void video_get_palette(uint8_t rgb[768])
{
    memcpy(rgb, palette, PALETTE_BYTES);
}

void video_fade_in(int first, int count, const uint8_t *target)
{
    uint8_t scaled[PALETTE_BYTES];
    for (int frame = 0; frame < PALETTE_FADE_STEPS; frame++) {
        palette_scale(target, scaled, first, count, palette_fade_in_step(frame));
        video_wait_vblank();
        video_set_palette(first, count, scaled);
    }
}

void video_fade_out(int first, int count)
{
    uint8_t start[PALETTE_BYTES], scaled[PALETTE_BYTES];
    video_get_palette(start);
    for (int frame = 0; frame < PALETTE_FADE_STEPS; frame++) {
        palette_scale(start, scaled, first, count, palette_fade_out_step(frame));
        video_wait_vblank();
        video_set_palette(first, count, scaled);
    }
}

static void blit(const FshEntry *shape, int x, int y, int transparent)
{
    int tw, th;
    uint8_t *const to = video_target(&tw, &th);
    for (int row = 0; row < shape->height; row++) {
        const int dy = y + row;
        if (dy < 0 || dy >= th) continue;
        for (int col = 0; col < shape->width; col++) {
            const int dx = x + col;
            const uint8_t p = shape->payload[(size_t)row * shape->width + col];
            if (dx >= 0 && dx < tw && !(transparent && p == 0xFF)) to[dy * tw + dx] = p;
        }
    }
}

void video_blit(const FshEntry *shape, int x, int y)
{
    blit(shape, x, y, 0);
}

void video_blit_transparent_at_own_position(const FshEntry *shape)
{
    blit(shape, shape->misc[2], shape->misc[3], 1);
}

void video_blit_transparent(const FshEntry *shape, int x, int y)
{
    blit(shape, x, y, 1);
}

void video_blit_at_own_position(const FshEntry *shape)
{
    video_blit(shape, shape->misc[2], shape->misc[3]);
}

void video_copy_front_to_back(void)
{
    memcpy(back, front, VIDEO_WIDTH * VIDEO_HEIGHT);
}

/* Clips a block to the target; returns 0 when nothing is left. */
static int clip(int *x, int *y, int *w, int *h, int *skip_x, int *skip_y)
{
    *skip_x = *x < 0 ? -*x : 0;
    *skip_y = *y < 0 ? -*y : 0;
    *x += *skip_x;
    *y += *skip_y;
    *w -= *skip_x;
    *h -= *skip_y;
    if (*x + *w > target_w) *w = target_w - *x;
    if (*y + *h > target_h) *h = target_h - *y;
    return *w > 0 && *h > 0;
}

void video_read_rect(int x, int y, int w, int h, uint8_t *out)
{
    const int stride = w;
    int sx, sy;
    if (!clip(&x, &y, &w, &h, &sx, &sy)) return;
    for (int row = 0; row < h; row++) memcpy(out + (size_t)(row + sy) * stride + sx, drawn() + (y + row) * target_w + x, (size_t)w);
}

void video_write_rect(int x, int y, int w, int h, const uint8_t *in)
{
    const int stride = w;
    int sx, sy;
    if (!clip(&x, &y, &w, &h, &sx, &sy)) return;
    for (int row = 0; row < h; row++) memcpy(drawn() + (y + row) * target_w + x, in + (size_t)(row + sy) * stride + sx, (size_t)w);
}

void video_shade_rect(int x, int y, int w, int h, const uint8_t table[256])
{
    int sx, sy;
    if (!clip(&x, &y, &w, &h, &sx, &sy)) return;
    for (int row = 0; row < h; row++) {
        uint8_t *p = drawn() + (y + row) * target_w + x;
        for (int col = 0; col < w; col++) p[col] = table[p[col]];
    }
}

static void plot(int x, int y, uint8_t color)
{
    if (x >= 0 && x < target_w && y >= 0 && y < target_h) drawn()[y * target_w + x] = color;
}

void video_frame_rect(int x0, int y0, int x1, int y1, uint8_t color)
{
    for (int x = x0; x <= x1; x++) {
        plot(x, y0, color);
        plot(x, y1, color);
    }
    for (int y = y0; y <= y1; y++) {
        plot(x0, y, color);
        plot(x1, y, color);
    }
}
