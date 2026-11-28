/* The SDL3 platform: the window, the renderer and its texture, the log header and the window's sizes (platform_sdl3.c split by responsibility; the shared names are in platform_sdl3_private.h). */
#include "platform/platform.h"

#include "formats/modpath.h"
#include "formats/validate.h"
#include "game/platform/window_fit.h"
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

SDL_Window *sdl3_window;
SDL_Renderer *sdl3_renderer;
SDL_Texture *sdl3_texture;
uint32_t *sdl3_rgb_buffer;
int sdl3_fb_width;
int sdl3_fb_height;

enum { VBLANK_HZ = 60 };
static int refresh_hz = VBLANK_HZ;

/* TNFS_CAPTURE_DIR=dir [TNFS_CAPTURE_EVERY=n] [TNFS_CAPTURE_FROM=m]: write every n-th presented
 * frame from the m-th on as dir/frame_NNNNN.ppm, to check the game's screens headless. */
const char *sdl3_capture_dir;
int sdl3_capture_every = 30;
unsigned sdl3_capture_from;
unsigned sdl3_presented_frames;
uint64_t sdl3_next_vblank_ns;

bool sdl3_window_focused;  /* the window has the keyboard focus: the system cursor is hidden over it then */

/* port (the bug-report log): GAME_LOG_FILE in the user directory, what the game said this run (the last run's with ".1"), headed with what a report needs */
void sdl3_open_log(void)
{
    static int opened;   /* once a run: another window's init must not rotate this run's log away */
    char path[1024];
    if (opened) return;
    opened = 1;
    if (sdl3_user_path(GAME_LOG_FILE, path, sizeof path) == NULL || !port_log_open_file(path)) return;
#ifdef TNFS_VERSION
    port_log(GAME_NAME " %s\n", TNFS_VERSION);
#else
    port_log(GAME_NAME " (no version)\n");
#endif
    port_log("system: %s, SDL %d.%d.%d, %d logical cpus\n", SDL_GetPlatform(), SDL_VERSIONNUM_MAJOR(SDL_GetVersion()),
             SDL_VERSIONNUM_MINOR(SDL_GetVersion()), SDL_VERSIONNUM_MICRO(SDL_GetVersion()), SDL_GetNumLogicalCPUCores());
    port_log("log: %s\n", path);
}

/* port: the window's size in points and in pixels, the renderer's output and the presentation rectangle, so a log shows what a retina display gave */
static void log_window_sizes(const char *when)
{
    int w = 0, h = 0, pw = 0, ph = 0, ow = 0, oh = 0, wx = 0, wy = 0;
    SDL_GetWindowPosition(sdl3_window, &wx, &wy);
    SDL_GetWindowSize(sdl3_window, &w, &h);
    SDL_GetWindowSizeInPixels(sdl3_window, &pw, &ph);
    SDL_GetRenderOutputSize(sdl3_renderer, &ow, &oh);
    SDL_FRect frect = { 0, 0, 0, 0 };
    SDL_GetRenderLogicalPresentationRect(sdl3_renderer, &frect);
    port_log("platform: window %s: at %d,%d %dx%d points, %dx%d pixels (density %.2f), render output %dx%d, presentation rect %.0f,%.0f %.0fx%.0f for %dx%d\n",
             when, wx, wy, w, h, pw, ph, (double)SDL_GetWindowPixelDensity(sdl3_window), ow, oh,
             (double)frect.x, (double)frect.y, (double)frect.w, (double)frect.h, sdl3_fb_width, sdl3_fb_height);
}

/* port (Wayland): a window asked full screen takes the screen's size only when the compositor says so, and nothing read what it said until the first picture: weston's desktop shell, given that picture first, keeps the place of
 * the small window when it then grows (cut off on the right and below). The events are pumped until the size is no longer the one asked, WAIT_MS at most; never SDL_SyncWindow, whose round trips have no limit. */
static void wait_wayland_fullscreen(int asked_w, int asked_h)
{
    enum { WAIT_MS = 2000, LOOK_MS = 5 };
    const char *driver = SDL_GetCurrentVideoDriver();
    if (driver == NULL || SDL_strcmp(driver, "wayland") != 0) return;
    int before_w = 0, before_h = 0;
    SDL_GetWindowSize(sdl3_window, &before_w, &before_h);
    SDL_Rect screen = { 0, 0, 0, 0 };
    const bool screen_is_asked = SDL_GetDisplayBounds(SDL_GetDisplayForWindow(sdl3_window), &screen) && screen.w == asked_w && screen.h == asked_h;   /* no size to wait for */
    int w = before_w, h = before_h;
    const Uint64 start = SDL_GetTicks();
    while (!screen_is_asked && w == asked_w && h == asked_h && SDL_GetTicks() - start < WAIT_MS) {
        SDL_PumpEvents();
        SDL_Delay(LOOK_MS);
        SDL_GetWindowSize(sdl3_window, &w, &h);
    }
    port_log("platform: Wayland full screen: window %dx%d before, %dx%d after %u ms\n", before_w, before_h, w, h, (unsigned)(SDL_GetTicks() - start));
}

bool platform_init(const PlatformConfig *config)
{
    data_path_set_base(SDL_GetBasePath());   /* port: our own data beside the program (the bundle's Contents/Resources/ on macOS): data_path.h */
    sdl3_open_log();
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        port_fatal("platform: SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }
    sdl3_fb_width = config->width;
    sdl3_fb_height = config->height;
    refresh_hz = config->refresh_hz > 0 ? config->refresh_hz : VBLANK_HZ;
    sdl3_next_vblank_ns = 0;

    /* real pixels, not points: on a retina display the integer scale counts the display's pixels (3x, 4x), not the half-size point grid */
    const SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | (config->fullscreen != 0 ? SDL_WINDOW_FULLSCREEN : 0);
    if (!SDL_CreateWindowAndRenderer(config->title, sdl3_fb_width, sdl3_fb_height, flags, &sdl3_window, &sdl3_renderer)) {
        port_fatal("platform: window creation failed: %s\n", SDL_GetError());
        platform_shutdown();
        return false;
    }
    {   /* the editor's Test drive puts the window beside its own: TNFS_WINDOW_POS=X,Y */
        const char *at = SDL_getenv("TNFS_WINDOW_POS");
        int px = 0, py = 0;
        if (at != NULL && SDL_sscanf(at, "%d,%d", &px, &py) == 2) SDL_SetWindowPosition(sdl3_window, px, py);
    }
    SDL_SetRenderLogicalPresentation(sdl3_renderer, sdl3_fb_width, sdl3_fb_height,
                                     config->scale_integer != 0 ? SDL_LOGICAL_PRESENTATION_INTEGER_SCALE
                                                                : SDL_LOGICAL_PRESENTATION_LETTERBOX);
    if (config->fullscreen != 0) wait_wayland_fullscreen(sdl3_fb_width, sdl3_fb_height);
    log_window_sizes("created");
    input_script_load();
    sdl3_window_focused = true;
    sdl3_cursor_apply();
    SDL_StartTextInput(sdl3_window);
    sdl3_capture_dir = SDL_getenv("TNFS_CAPTURE_DIR");
    const char *every = SDL_getenv("TNFS_CAPTURE_EVERY");
    if (every != NULL && SDL_atoi(every) > 0) sdl3_capture_every = SDL_atoi(every);
    const char *from = SDL_getenv("TNFS_CAPTURE_FROM");
    if (from != NULL && SDL_atoi(from) > 0) sdl3_capture_from = (unsigned)SDL_atoi(from);

    sdl3_texture = SDL_CreateTexture(sdl3_renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, sdl3_fb_width, sdl3_fb_height);
    sdl3_rgb_buffer = malloc((size_t)sdl3_fb_width * (size_t)sdl3_fb_height * sizeof *sdl3_rgb_buffer);
    if (sdl3_texture == NULL || sdl3_rgb_buffer == NULL) {
        port_fatal("platform: framebuffer allocation failed: %s\n", SDL_GetError());
        platform_shutdown();
        return false;
    }
    SDL_SetTextureScaleMode(sdl3_texture, SDL_SCALEMODE_NEAREST);
    return true;
}

/* port (window_fit): the windowed window in the framebuffer's shape, as tall as it is now when that fits the display's usable area (the desktop less the taskbar) with its title bar and borders, and inside it */
static void fit_windowed_window(void)
{
    int w = 0, h = 0, x = 0, y = 0;
    if (!SDL_GetWindowSize(sdl3_window, &w, &h) || h <= 0) return;
    SDL_GetWindowPosition(sdl3_window, &x, &y);
    SDL_Rect usable;
    WindowBorders borders = { 0, 0, 0, 0 };
    const SDL_DisplayID display = SDL_GetDisplayForWindow(sdl3_window);
    if (display == 0 || !SDL_GetDisplayUsableBounds(display, &usable)) {
        SDL_SetWindowSize(sdl3_window, h * sdl3_fb_width / sdl3_fb_height, h);   /* no area to keep inside: the shape only */
        return;
    }
    (void)SDL_GetWindowBordersSize(sdl3_window, &borders.top, &borders.left, &borders.bottom, &borders.right);   /* none known: 0 */
    const WindowRect area = { usable.x, usable.y, usable.w, usable.h };
    const WindowRect fit = window_fit(sdl3_fb_width, sdl3_fb_height, h, x, y, &area, &borders);
    SDL_SetWindowSize(sdl3_window, fit.w, fit.h);
    if (fit.x != x || fit.y != y) SDL_SetWindowPosition(sdl3_window, fit.x, fit.y);
}

void platform_log_window(const char *when)
{
    if (sdl3_window != NULL && sdl3_renderer != NULL) log_window_sizes(when);
}

bool platform_reconfigure(const PlatformConfig *config)
{
    if (sdl3_window == NULL || sdl3_renderer == NULL || config->width <= 0 || config->height <= 0) return false;
    SDL_Texture *t = SDL_CreateTexture(sdl3_renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, config->width,
                                       config->height);
    uint32_t *rgb = malloc((size_t)config->width * (size_t)config->height * sizeof *rgb);
    if (t == NULL || rgb == NULL) {
        port_log("platform: framebuffer reallocation failed: %s\n", SDL_GetError());
        if (t != NULL) SDL_DestroyTexture(t);
        free(rgb);
        return false;
    }
    SDL_SetTextureScaleMode(t, SDL_SCALEMODE_NEAREST);
    SDL_DestroyTexture(sdl3_texture);
    free(sdl3_rgb_buffer);
    sdl3_texture = t, sdl3_rgb_buffer = rgb;
    sdl3_fb_width = config->width, sdl3_fb_height = config->height;
    refresh_hz = config->refresh_hz > 0 ? config->refresh_hz : VBLANK_HZ;
    sdl3_next_vblank_ns = 0;
    SDL_SetWindowFullscreen(sdl3_window, config->fullscreen != 0);
    /* the change is asynchronous (Windows, macOS): sized before it ends, a window leaving fullscreen is still the screen's size and comes out taller than the screen's usable part */
    if (!SDL_SyncWindow(sdl3_window)) port_log("platform: the window did not finish its change of fullscreen in time: %s\n", SDL_GetError());
    sdl3_cursor_apply();   /* the mode change may have brought the system's back */
    if (config->fullscreen == 0) fit_windowed_window();
    SDL_SetRenderLogicalPresentation(sdl3_renderer, sdl3_fb_width, sdl3_fb_height,
                                     config->scale_integer != 0 ? SDL_LOGICAL_PRESENTATION_INTEGER_SCALE
                                                                : SDL_LOGICAL_PRESENTATION_LETTERBOX);
    log_window_sizes("reconfigured");
    return true;
}

void platform_framebuffer_size(int *width, int *height)
{
    *width = sdl3_fb_width;
    *height = sdl3_fb_height;
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

static SDL_Surface *icon_surface(const PlatformIconImage *image)
{
    if (image->rgba == NULL || image->w <= 0 || image->h <= 0) return NULL;
    return SDL_CreateSurfaceFrom(image->w, image->h, SDL_PIXELFORMAT_RGBA32, (void *)(uintptr_t)image->rgba, image->w * 4);
}

void platform_set_window_icon(const PlatformIconImage *images, int count)
{
    if (sdl3_window == NULL || images == NULL || count <= 0) return;
    SDL_Surface *const icon = icon_surface(&images[0]);
    if (icon == NULL) return;
    for (int i = 1; i < count; i++) {   /* the high-DPI versions (SDL_SetWindowIcon picks the closest for the display's scale) */
        SDL_Surface *const alt = icon_surface(&images[i]);
        if (alt != NULL) SDL_AddSurfaceAlternateImage(icon, alt);
        SDL_DestroySurface(alt);
    }
    SDL_SetWindowIcon(sdl3_window, icon);
    SDL_DestroySurface(icon);
}

void platform_shutdown(void)
{
#ifndef __EMSCRIPTEN__
    sdl3_pool_stop();
#endif
    if (sdl3_window != NULL) SDL_ShowCursor();
    sdl3_window_focused = false;
    if (sdl3_audio != NULL) platform_audio_close();
    free(sdl3_rgb_buffer);
    sdl3_rgb_buffer = NULL;
    if (sdl3_texture != NULL) SDL_DestroyTexture(sdl3_texture);
    if (sdl3_renderer != NULL) SDL_DestroyRenderer(sdl3_renderer);
    if (sdl3_window != NULL) SDL_DestroyWindow(sdl3_window);
    sdl3_texture = NULL;
    sdl3_renderer = NULL;
    sdl3_window = NULL;
    SDL_Quit();
}
