/* The SDL3 platform's present: the palettes to the texture, the capture and the pacing (platform_sdl3.c split by responsibility; the shared names are in platform_sdl3_private.h). */
#include "platform/platform.h"

#include "formats/modpath.h"
#include "formats/validate.h"
#include "game/sys/key_latch.h"
#include "game/sys/data_path.h"
#include "game/sys/file_url.h"
#include "game/sys/port_log.h"
#include "tnfs_version.h"
#include "platform/sdl3/input_script.h"
#include "platform/sdl3/input_shared.h"
#include "platform/sdl3/pad_keys.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
#include <unistd.h>   /* fsync */
#include <limits.h>   /* realpath: symbolic links out of the mods directory are refused */
#define TNFS_HAVE_REALPATH 1
#endif
#include "platform/sdl3/platform_sdl3_private.h"

static uint64_t next_frontend_ns;

static void capture_frame(void)
{
    if (sdl3_capture_dir == NULL || sdl3_presented_frames++ % (unsigned)sdl3_capture_every != 0 || sdl3_presented_frames <= sdl3_capture_from) return;
    char path[1024];
    SDL_snprintf(path, sizeof path, "%s/frame_%05u.ppm", sdl3_capture_dir, sdl3_presented_frames - 1);
    FILE *f = fopen(path, "wb");
    if (f == NULL) return;
    fprintf(f, "P6\n%d %d\n255\n", sdl3_fb_width, sdl3_fb_height);
    for (int i = 0; i < sdl3_fb_width * sdl3_fb_height; i++) {
        const uint8_t px[3] = { (uint8_t)(sdl3_rgb_buffer[i] >> 16), (uint8_t)(sdl3_rgb_buffer[i] >> 8), (uint8_t)sdl3_rgb_buffer[i] };
        fwrite(px, 1, 3, f);
    }
    fclose(f);
}

/* One step of the pacing: the time of the next boundary (`next`, 0 at the first), after one `period`; resynced when more than a period behind. *wait_ns is how long to sleep from `now`. */
static uint64_t pace_step(uint64_t next, uint64_t now, uint64_t period, uint64_t *wait_ns)
{
    if (next == 0 || now > next + period) next = now;  /* resync after stalls */
    next += period;
    *wait_ns = next > now ? next - now : 0;
    return next;
}

void platform_wait_vblank(void)
{
    uint64_t wait_ns;
    sdl3_next_vblank_ns = pace_step(sdl3_next_vblank_ns, SDL_GetTicksNS(), platform_refresh_period_ns(), &wait_ns);
    if (wait_ns >= SDL_NS_PER_MS) platform_sleep_ms((uint32_t)(wait_ns / SDL_NS_PER_MS));
}

uint64_t platform_frontend_period_ns(void)
{
    return SDL_NS_PER_SECOND / PLATFORM_FRONTEND_HZ;
}

void platform_wait_frontend_frame(void)
{
    uint64_t wait_ns;
    next_frontend_ns = pace_step(next_frontend_ns, SDL_GetTicksNS(), platform_frontend_period_ns(), &wait_ns);
    if (wait_ns >= SDL_NS_PER_MS) platform_sleep_ms((uint32_t)(wait_ns / SDL_NS_PER_MS));
}

typedef struct {
    const uint8_t *pixels;
    const uint32_t *lut;     /* 256 entries; with a mask 4 x 256, the mask's value picking the table */
    const uint8_t *mask;     /* NULL: one table */
    uint32_t *out;
    int width, height, count;
    size_t stride;   /* out's row, in pixels (the texture's pitch may be wider than width) */
} LutJob;

/* the masks a pixel can have: 0 plain, 1 world, 2 half, 3 lit, then one per colour slot (PLATFORM_LIT_SLOTS) */
enum { MASK_KINDS = PLATFORM_MASK_LIT_FIRST + PLATFORM_LIT_SLOTS };

static void lut_band(void *ctx, int index)
{
    const LutJob *j = ctx;
    const int from = (int)((int64_t)j->height * index / j->count), to = (int)((int64_t)j->height * (index + 1) / j->count);
    for (int y = from; y < to; y++) {
        const uint8_t *in = j->pixels + (size_t)y * (size_t)j->width;
        uint32_t *row = j->out + (size_t)y * j->stride;
        if (j->mask == NULL) {
            for (int x = 0; x < j->width; x++) row[x] = j->lut[in[x]];
        } else {
            const uint8_t *m = j->mask + (size_t)y * (size_t)j->width;
            for (int x = 0; x < j->width; x++) row[x] = j->lut[(m[x] >= MASK_KINDS ? 0 : m[x]) * 256 + in[x]];
        }
    }
}

void platform_palette_to_xrgb_pitch(const uint8_t *pixels, const PlatformColor palette[256], void *out, int pitch, int width,
                                    int height, int threads)
{
    uint32_t lut[256];
    for (int i = 0; i < 256; i++) {
        lut[i] = ((uint32_t)palette[i].r << 16) | ((uint32_t)palette[i].g << 8) | palette[i].b;
    }
    enum { MIN_ROWS = 32 };   /* a band of fewer rows costs more to hand out than to convert */
    int count = threads > 1 ? threads * 2 : 1;
    if (count > height / MIN_ROWS) count = height / MIN_ROWS;
    if (count < 1) count = 1;
    const LutJob job = { pixels, lut, NULL, out, width, height, count, (size_t)pitch / sizeof(uint32_t) };
    platform_parallel_for(count, lut_band, (void *)&job, threads);
}

static const PlatformColor *lit_palette;

void platform_set_lit_palette(const PlatformColor lit[256]) { lit_palette = lit; }

static const PlatformColor (*lit_colours)[256];
static int lit_colour_count;

void platform_set_lit_colours(const PlatformColor (*lit)[256], int count)
{
    lit_colours = lit;
    lit_colour_count = lit != NULL && count > 0 ? (count > PLATFORM_LIT_SLOTS ? PLATFORM_LIT_SLOTS : count) : 0;
}

void platform_palette_to_xrgb_split_pitch(const uint8_t *pixels, const PlatformColor palette[256], const PlatformColor world_palette[256],
                                          const PlatformColor half_palette[256], const uint8_t *mask, void *out, int pitch, int width,
                                          int height, int threads)
{
    uint32_t lut[MASK_KINDS * 256];
    const PlatformColor *by_mask[MASK_KINDS] = { palette, world_palette, half_palette, lit_palette != NULL ? lit_palette : palette };
    for (int m = PLATFORM_MASK_LIT_FIRST; m < MASK_KINDS; m++)
        by_mask[m] = m - PLATFORM_MASK_LIT_FIRST < lit_colour_count ? lit_colours[m - PLATFORM_MASK_LIT_FIRST] : by_mask[3];
    for (int m = 0; m < MASK_KINDS; m++)
        for (int i = 0; i < 256; i++)
            lut[m * 256 + i] = ((uint32_t)by_mask[m][i].r << 16) | ((uint32_t)by_mask[m][i].g << 8) | by_mask[m][i].b;
    enum { MIN_ROWS = 32 };
    int count = threads > 1 ? threads * 2 : 1;
    if (count > height / MIN_ROWS) count = height / MIN_ROWS;
    if (count < 1) count = 1;
    const LutJob job = { pixels, lut, mask, out, width, height, count, (size_t)pitch / sizeof(uint32_t) };
    platform_parallel_for(count, lut_band, (void *)&job, threads);
}

void platform_palette_to_xrgb(const uint8_t *pixels, const PlatformColor palette[256], uint32_t *out, int width, int height,
                              int threads)
{
    platform_palette_to_xrgb_pitch(pixels, palette, out, width * (int)sizeof *out, width, height, threads);
}

static int present_threads;

void platform_set_present_threads(int threads) { present_threads = threads; }

static int present_lock = 1;

void platform_set_present_lock(int on) { present_lock = on; }

/* the frame through one palette, or through three by the mask (the baked light's time of day); the texture's memory locked or the buffer */
static void present_frame(const uint8_t *pixels, const PlatformColor palette[256], const PlatformColor world_palette[256],
                          const PlatformColor half_palette[256], const uint8_t *mask)
{
    input_script_note_present();
    void *locked = NULL;
    int pitch = 0;
    /* the frame goes straight into the texture's memory, no second copy; a capture needs rgb_buffer, the web build keeps
     * the upload, and a failed lock falls back to it */
#ifndef __EMSCRIPTEN__
    if (sdl3_capture_dir == NULL && present_lock && SDL_LockTexture(sdl3_texture, NULL, &locked, &pitch)) {
        if (mask != NULL)
            platform_palette_to_xrgb_split_pitch(pixels, palette, world_palette, half_palette, mask, locked, pitch, sdl3_fb_width, sdl3_fb_height,
                                                 present_threads);
        else platform_palette_to_xrgb_pitch(pixels, palette, locked, pitch, sdl3_fb_width, sdl3_fb_height, present_threads);
        SDL_UnlockTexture(sdl3_texture);
    } else
#endif
    {
        if (mask != NULL)
            platform_palette_to_xrgb_split_pitch(pixels, palette, world_palette, half_palette, mask, sdl3_rgb_buffer,
                                                 sdl3_fb_width * (int)sizeof *sdl3_rgb_buffer, sdl3_fb_width, sdl3_fb_height, present_threads);
        else platform_palette_to_xrgb(pixels, palette, sdl3_rgb_buffer, sdl3_fb_width, sdl3_fb_height, present_threads);
        capture_frame();
        SDL_UpdateTexture(sdl3_texture, NULL, sdl3_rgb_buffer, sdl3_fb_width * (int)sizeof *sdl3_rgb_buffer);
    }
    SDL_RenderClear(sdl3_renderer);
    SDL_RenderTexture(sdl3_renderer, sdl3_texture, NULL, NULL);
    SDL_RenderPresent(sdl3_renderer);
}

void platform_present(const uint8_t *pixels, const PlatformColor palette[256])
{
    present_frame(pixels, palette, NULL, NULL, NULL);
}

void platform_present_split(const uint8_t *pixels, const PlatformColor palette[256], const PlatformColor world_palette[256],
                            const PlatformColor half_palette[256], const uint8_t *mask)
{
    present_frame(pixels, palette, world_palette, half_palette, mask);
}
