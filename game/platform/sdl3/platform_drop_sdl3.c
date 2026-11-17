/* The SDL3 platform's dropped files (platform.h's platform_dropped_file, dep59 step 4b): the paths of the files dropped on the window, kept
 * in turn until the game takes them. A path is copied whole or not at all: one that does not fit the queue's room, or the caller's, is left
 * out with a line in the log, never cut short into another file's name. */
#include "platform/platform.h"

#include "game/sys/port_log.h"
#include "platform/sdl3/input_script.h"
#include "platform/sdl3/platform_sdl3_private.h"

#include <SDL3/SDL.h>
#include <string.h>

enum { DROP_PATH = 1024 };
static char dropped[PLATFORM_DROPPED_MAX][DROP_PATH];
static int drop_head, drop_count;

void sdl3_drop_push(const char *path)
{
    const size_t n = strlen(path);
    if (n == 0 || n >= DROP_PATH) {
        port_log("platform: a dropped file's path of %zu bytes: left out\n", n);
        return;
    }
    if (drop_count == PLATFORM_DROPPED_MAX) {
        port_log("platform: %d dropped files wait already: %s left out\n", PLATFORM_DROPPED_MAX, path);
        return;
    }
    memcpy(dropped[(drop_head + drop_count++) % PLATFORM_DROPPED_MAX], path, n + 1);
}

bool platform_dropped_file(char *out, size_t out_size)
{
    if (input_script_active()) input_script_apply_drops();
    if (drop_count == 0) return false;
    const char *const path = dropped[drop_head];
    drop_head = (drop_head + 1) % PLATFORM_DROPPED_MAX, drop_count--;
    const size_t n = strlen(path);
    if (out == NULL || n >= out_size) {
        port_log("platform: the dropped file %s does not fit %zu bytes: left out\n", path, out_size);
        return false;
    }
    memcpy(out, path, n + 1);
    return true;
}
