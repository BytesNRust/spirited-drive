/* The SDL3 platform: the window, the renderer and its texture, the log header and the window's sizes (platform_sdl3.c split by responsibility; the shared names are in platform_sdl3_private.h). */
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

SDL_Window *window;
SDL_Renderer *renderer;
SDL_Texture *texture;
uint32_t *rgb_buffer;
int fb_width;
int fb_height;

enum { VBLANK_HZ = 60 };
static int refresh_hz = VBLANK_HZ;

/* TNFS_CAPTURE_DIR=dir [TNFS_CAPTURE_EVERY=n] [TNFS_CAPTURE_FROM=m]: write every n-th presented
 * frame from the m-th on as dir/frame_NNNNN.ppm, to check the game's screens headless. */
const char *capture_dir;
int capture_every = 30;
unsigned capture_from;
unsigned presented_frames;
uint64_t next_vblank_ns;

bool window_focused;  /* the window has the keyboard focus: the system cursor is hidden over it then */

/* port (the bug-report log): tnfs.log in the user directory, what the game said this run (the last run's as tnfs.log.1), headed with what a report needs */
void open_log(void)
{
    static int opened;   /* once a run: another window's init must not rotate this run's log away */
    char path[1024];
    if (opened) return;
    opened = 1;
    if (user_path("tnfs.log", path, sizeof path) == NULL || !port_log_open_file(path)) return;
#ifdef TNFS_VERSION
    port_log("tnfs %s\n", TNFS_VERSION);
#else
    port_log("tnfs (no version)\n");
#endif
    port_log("system: %s, SDL %d.%d.%d, %d logical cpus\n", SDL_GetPlatform(), SDL_VERSIONNUM_MAJOR(SDL_GetVersion()),
             SDL_VERSIONNUM_MINOR(SDL_GetVersion()), SDL_VERSIONNUM_MICRO(SDL_GetVersion()), SDL_GetNumLogicalCPUCores());
    port_log("log: %s\n", path);
}

/* port: the window's size in points and in pixels, the renderer's output and the presentation rectangle, so a log shows what a retina display gave */
static void log_window_sizes(const char *when)
{
    int w = 0, h = 0, pw = 0, ph = 0, ow = 0, oh = 0;
    SDL_GetWindowSize(window, &w, &h);
    SDL_GetWindowSizeInPixels(window, &pw, &ph);
    SDL_GetRenderOutputSize(renderer, &ow, &oh);
    SDL_FRect frect = { 0, 0, 0, 0 };
    SDL_GetRenderLogicalPresentationRect(renderer, &frect);
    port_log("platform: window %s: %dx%d points, %dx%d pixels (density %.2f), render output %dx%d, presentation rect %.0f,%.0f %.0fx%.0f for %dx%d\n",
             when, w, h, pw, ph, (double)SDL_GetWindowPixelDensity(window), ow, oh,
             (double)frect.x, (double)frect.y, (double)frect.w, (double)frect.h, fb_width, fb_height);
}

bool platform_init(const PlatformConfig *config)
{
    data_path_set_base(SDL_GetBasePath());   /* port: our own data beside the program (the bundle's Contents/Resources/ on macOS): data_path.h */
    open_log();
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        port_fatal("platform: SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }
    fb_width = config->width;
    fb_height = config->height;
    refresh_hz = config->refresh_hz > 0 ? config->refresh_hz : VBLANK_HZ;
    next_vblank_ns = 0;

    /* real pixels, not points: on a retina display the integer scale counts the display's pixels (3x, 4x), not the half-size point grid */
    const SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | (config->fullscreen != 0 ? SDL_WINDOW_FULLSCREEN : 0);
    if (!SDL_CreateWindowAndRenderer(config->title, fb_width, fb_height, flags, &window, &renderer)) {
        port_fatal("platform: window creation failed: %s\n", SDL_GetError());
        platform_shutdown();
        return false;
    }
    SDL_SetRenderLogicalPresentation(renderer, fb_width, fb_height,
                                     config->scale_integer != 0 ? SDL_LOGICAL_PRESENTATION_INTEGER_SCALE
                                                                : SDL_LOGICAL_PRESENTATION_LETTERBOX);
    log_window_sizes("created");
    input_script_load();
    window_focused = true;
    cursor_apply();
    SDL_StartTextInput(window);
    capture_dir = SDL_getenv("TNFS_CAPTURE_DIR");
    const char *every = SDL_getenv("TNFS_CAPTURE_EVERY");
    if (every != NULL && SDL_atoi(every) > 0) capture_every = SDL_atoi(every);
    const char *from = SDL_getenv("TNFS_CAPTURE_FROM");
    if (from != NULL && SDL_atoi(from) > 0) capture_from = (unsigned)SDL_atoi(from);

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, fb_width, fb_height);
    rgb_buffer = malloc((size_t)fb_width * (size_t)fb_height * sizeof *rgb_buffer);
    if (texture == NULL || rgb_buffer == NULL) {
        port_fatal("platform: framebuffer allocation failed: %s\n", SDL_GetError());
        platform_shutdown();
        return false;
    }
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    return true;
}

bool platform_reconfigure(const PlatformConfig *config)
{
    if (window == NULL || renderer == NULL || config->width <= 0 || config->height <= 0) return false;
    SDL_Texture *t = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, config->width,
                                       config->height);
    uint32_t *rgb = malloc((size_t)config->width * (size_t)config->height * sizeof *rgb);
    if (t == NULL || rgb == NULL) {
        port_log("platform: framebuffer reallocation failed: %s\n", SDL_GetError());
        if (t != NULL) SDL_DestroyTexture(t);
        free(rgb);
        return false;
    }
    SDL_SetTextureScaleMode(t, SDL_SCALEMODE_NEAREST);
    SDL_DestroyTexture(texture);
    free(rgb_buffer);
    texture = t, rgb_buffer = rgb;
    fb_width = config->width, fb_height = config->height;
    refresh_hz = config->refresh_hz > 0 ? config->refresh_hz : VBLANK_HZ;
    next_vblank_ns = 0;
    SDL_SetWindowFullscreen(window, config->fullscreen != 0);
    cursor_apply();   /* the mode change may have brought the system's back */
    if (config->fullscreen == 0) {
        int w = 0, h = 0;
        if (SDL_GetWindowSize(window, &w, &h) && h > 0) SDL_SetWindowSize(window, h * fb_width / fb_height, h);
    }
    SDL_SetRenderLogicalPresentation(renderer, fb_width, fb_height,
                                     config->scale_integer != 0 ? SDL_LOGICAL_PRESENTATION_INTEGER_SCALE
                                                                : SDL_LOGICAL_PRESENTATION_LETTERBOX);
    log_window_sizes("reconfigured");
    return true;
}

void platform_framebuffer_size(int *width, int *height)
{
    *width = fb_width;
    *height = fb_height;
}

bool platform_desktop_size(int *width, int *height)
{
    if (!SDL_WasInit(SDL_INIT_VIDEO) && !SDL_InitSubSystem(SDL_INIT_VIDEO)) return false;   /* asked before the window (the profiles' format): the video system first */
    const SDL_DisplayID display = SDL_GetPrimaryDisplay();
    const SDL_DisplayMode *mode = display != 0 ? SDL_GetDesktopDisplayMode(display) : NULL;
    if (mode == NULL || mode->w <= 0 || mode->h <= 0) return false;
    *width = mode->w;
    *height = mode->h;
    return true;
}

uint64_t platform_refresh_period_ns(void)
{
    return SDL_NS_PER_SECOND / (uint64_t)refresh_hz;
}

void platform_set_window_icon(const uint8_t *rgba, int w, int h)
{
    if (window == NULL || rgba == NULL || w <= 0 || h <= 0) return;
    SDL_Surface *const icon = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_RGBA32, (void *)(uintptr_t)rgba, w * 4);
    if (icon == NULL) return;
    SDL_SetWindowIcon(window, icon);
    SDL_DestroySurface(icon);
}

void platform_shutdown(void)
{
#ifndef __EMSCRIPTEN__
    pool_stop();
#endif
    if (window != NULL) SDL_ShowCursor();
    window_focused = false;
    if (audio != NULL) platform_audio_close();
    free(rgb_buffer);
    rgb_buffer = NULL;
    if (texture != NULL) SDL_DestroyTexture(texture);
    if (renderer != NULL) SDL_DestroyRenderer(renderer);
    if (window != NULL) SDL_DestroyWindow(window);
    texture = NULL;
    renderer = NULL;
    window = NULL;
    SDL_Quit();
}
