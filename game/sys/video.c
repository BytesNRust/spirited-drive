#include "game/sys/video.h"

#include "game/sys/backdrop.h"
#include "game/sys/palette.h"
#include "game/viewer/screen_layout.h"
#include "platform/platform.h"

#include <stdlib.h>
#include <string.h>

enum { TRANSPARENT = 0xFF };

static uint8_t *back;
static uint8_t *target;  /* where drawing goes: back, or a picture (00473680) */
static int target_w = VIDEO_WIDTH, target_h = VIDEO_HEIGHT;
static int clip_l, clip_r = VIDEO_WIDTH, clip_t, clip_b = VIDEO_HEIGHT;  /* 004c78bc..004c78c8, the blits' box */
static uint8_t *front;
static uint8_t *composed;
static uint8_t *wide;  /* port: the picture centred on its darkened enlarged copy, when the platform is wider than 640 */
static int wide_w;
static uint8_t shade[256];
static int shade_valid;
static uint8_t palette[PALETTE_BYTES];  /* 0052ced8 / 0052f178 */
static const FshEntry *overlay;
static int overlay_x, overlay_y;
/* port: whether the palette on the display is entirely black, so a screen that fades the display out before putting
 * its own picture up has nothing to fade. Set by a fade_out over the whole palette (its last step is 0, so every
 * colour lands on zero) and cleared by anything that puts light back on the display. */
static int display_dark;

/* whether any colour in [first, first+count) is lit. A fade scales with >> 5, so a colour below 32 lands on zero
 * whatever the step -- the display counts as dark only when every one of them does. */
static int range_lit(int first, int count)
{
    for (int i = first; i < first + count; i++)
        if (palette[3 * i] || palette[3 * i + 1] || palette[3 * i + 2]) return 1;
    return 0;
}

/* The overlay's sprite drawn on `dst` (stride wide) with the 640 picture starting at column `left` */
static void draw_overlay(uint8_t *dst, int stride, int left)
{
    for (int row = 0; row < overlay->height; row++) {
        const int dy = overlay_y + row;
        if (dy < 0 || dy >= VIDEO_HEIGHT) continue;
        for (int col = 0; col < overlay->width; col++) {
            const int dx = overlay_x + col;
            const uint8_t c = overlay->payload[(size_t)row * overlay->width + col];
            if (c != TRANSPARENT && dx >= 0 && dx < VIDEO_WIDTH) dst[dy * stride + left + dx] = c;
        }
    }
}

static void compose_overlay(void)
{
    memcpy(composed, front, VIDEO_WIDTH * VIDEO_HEIGHT);
    draw_overlay(composed, VIDEO_WIDTH, 0);
}

/* port: the picture on a platform wider than 640: centred over a darkened, enlarged copy of itself */
static const uint8_t *compose_wide(int width)
{
    if (wide_w != width) {
        free(wide);
        wide = malloc((size_t)width * VIDEO_HEIGHT);
        wide_w = wide != NULL ? width : 0;
        if (wide == NULL) return NULL;
    }
    if (!shade_valid) backdrop_shade_table(palette, shade), shade_valid = 1;
    backdrop_compose(front, wide, width, shade);
    if (overlay != NULL) draw_overlay(wide, width, screen_menu_left(width));
    return wide;
}

static void show(void)
{
    PlatformColor colors[PALETTE_COLORS];
    for (int i = 0; i < PALETTE_COLORS; i++) {
        colors[i] = (PlatformColor){ palette[3 * i], palette[3 * i + 1], palette[3 * i + 2] };
    }
    int width = VIDEO_WIDTH, height = VIDEO_HEIGHT;
    platform_framebuffer_size(&width, &height);
    const uint8_t *const backdrop = width > VIDEO_WIDTH ? compose_wide(width) : NULL;
    if (backdrop != NULL) {
        platform_present(backdrop, colors);
        return;
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
    shade_valid = 0;
    display_dark = 1;   /* port: a fresh display has never been lit */
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
    free(wide);
    back = front = composed = wide = NULL;
    wide_w = shade_valid = 0;
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
    clip_l = 0, clip_r = target_w, clip_t = 0, clip_b = target_h;
}

void video_set_clip(int left, int right, int top, int bottom)
{
    clip_l = left < 0 ? 0 : left;
    clip_t = top < 0 ? 0 : top;
    clip_r = right > target_w ? target_w : right;
    clip_b = bottom > target_h ? target_h : bottom;
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
    shade_valid = 0;
    display_dark = !range_lit(0, PALETTE_COLORS);
    show();
}

int video_display_is_dark(void)
{
    return display_dark;
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
    /* port: a fade out over the whole palette leaves it entirely black (the last step is 0), which is what lets a
     * screen skip a second fade of the same display. A fade over part of it says nothing about the rest. */
    if (first == 0 && count >= PALETTE_COLORS) display_dark = 1;
}

static void blit(const FshEntry *shape, int x, int y, int transparent)
{
    int tw, th;
    uint8_t *const to = video_target(&tw, &th);
    for (int row = 0; row < shape->height; row++) {
        const int dy = y + row;
        if (dy < clip_t || dy >= clip_b || dy >= th) continue;
        for (int col = 0; col < shape->width; col++) {
            const int dx = x + col;
            const uint8_t p = shape->payload[(size_t)row * shape->width + col];
            if (dx >= clip_l && dx < clip_r && dx < tw && !(transparent && p == 0xFF)) to[dy * tw + dx] = p;
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
