/* The SDL3 platform's file dialog for the game's own screens (platform.h's platform_pick_zip, dep59 step 4b): the system's open dialog,
 * over the game's window, waited for. The events that come meanwhile are pumped, not taken: the game reads them after (a quit included).
 * With no real window (SDL's dummy or offscreen video, the headless walks) there is no dialog: none is picked, and the log says so; the
 * walks drop their files with the input script's DROP instead. */
#include "platform/platform.h"

#include "game/sys/port_log.h"
#include "platform/sdl3/platform_sdl3_private.h"

#include <SDL3/SDL.h>
#include <string.h>

typedef struct {
    bool done;
    char *out;
    size_t out_size;
    bool picked;
} ZipPick;

static void zip_picked(void *user, const char *const *list, int filter)
{
    (void)filter;
    ZipPick *p = user;
    if (list == NULL) port_log("platform: the file dialog failed: %s\n", SDL_GetError());
    if (list != NULL && list[0] != NULL) {
        const size_t n = strlen(list[0]);
        if (n < p->out_size) memcpy(p->out, list[0], n + 1), p->picked = true;
        else port_log("platform: the picked file's path of %zu bytes does not fit: none picked\n", n);
    }
    p->done = true;
}

/* 1 when the video driver shows no window a dialog could stand over */
static bool no_window(void)
{
    const char *const driver = SDL_GetCurrentVideoDriver();
    return sdl3_window == NULL || driver == NULL || strcmp(driver, "dummy") == 0 || strcmp(driver, "offscreen") == 0;
}

bool platform_pick_zip(char *out, size_t out_size)
{
    if (out == NULL || out_size == 0) return false;
    out[0] = '\0';
    if (no_window()) {
        port_log("platform: no window for a file dialog: none picked\n");
        return false;
    }
    static const SDL_DialogFileFilter FILTERS[] = { { "Mod (zip)", "zip" } };
    ZipPick p = { false, out, out_size, false };
    SDL_ShowOpenFileDialog(zip_picked, &p, sdl3_window, FILTERS, 1, NULL, false);
    while (!p.done) {
        SDL_PumpEvents();
        SDL_Delay(30);
    }
    return p.picked;
}
