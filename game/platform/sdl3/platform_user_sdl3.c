/* The SDL3 platform's player folder files and the settings' read cache (platform_sdl3.c split by responsibility; the shared names are in platform_sdl3_private.h). */
#include "platform/platform.h"

#include "formats/modpath.h"
#include "formats/validate.h"
#include "game/sys/key_latch.h"
#include "game/sys/data_path.h"
#include "game/sys/file_url.h"
#include "game/sys/game_name.h"
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

/* The player's folder: --user DIR, else $TNFS_PREF_DIR (tests, modders), else one folder named USER_FOLDER_NAME in the OS's per-user data directory
 * (SDL_GetPrefPath without an organisation, so no tnfs/tnfs). NULL when the platform has none. --mods has no say in it. */
const char *sdl3_user_dir(char *out, size_t out_size)
{
    static char *pref;
    if (sdl3_user_override[0] != '\0') {
        SDL_snprintf(out, out_size, "%s", sdl3_user_override);
        SDL_CreateDirectory(out);
        return out;
    }
    const char *const env = SDL_getenv("TNFS_PREF_DIR");
    if (env != NULL && env[0] != '\0') {
        SDL_snprintf(out, out_size, "%s/", env);
        SDL_CreateDirectory(out);
        return out;
    }
    if (pref == NULL) pref = SDL_GetPrefPath(NULL, USER_FOLDER_NAME);
    if (pref == NULL) return NULL;
    SDL_snprintf(out, out_size, "%s", pref);
    return out;
}

bool platform_user_dir(char *out, size_t out_size)
{
    return sdl3_user_dir(out, out_size) != NULL;
}

int platform_open_user_dir(void)
{
    char dir[1200], url[3700];
    if (sdl3_user_dir(dir, sizeof dir) == NULL || file_url_from_path(dir, url, sizeof url) == 0) return 0;
    return SDL_OpenURL(url) ? 1 : 0;
}

int platform_open_repo_page(void)
{
    return SDL_OpenURL(GAME_REPO_URL) ? 1 : 0;
}

const char *sdl3_user_path(const char *name, char *out, size_t out_size)
{
    char dir[1200];
    if (strpbrk(name, "/\\") != NULL || strcmp(name, "..") == 0 || sdl3_user_dir(dir, sizeof dir) == NULL) return NULL;
    SDL_snprintf(out, out_size, "%s%s", dir, name);
    return out;
}

/* port (perf): the settings' .cfg files are read by every port_*() accessor, many times a frame (from the band threads
 * too), each an open, read and mkdir. A copy of the bytes is kept for CFG_CACHE_MS: a write or remove through this layer
 * drops it at once, so a menu's change applies immediately, and a file edited from outside is seen within the interval. */
enum { CFG_CACHE_SLOTS = 8, CFG_CACHE_MS = 50 };   /* port.cfg, the three modes' files, controls.cfg and room */
static struct {
    char path[1024];
    uint8_t *data;
    size_t size;
    Uint64 at;
} cfg_cache[CFG_CACHE_SLOTS];
static atomic_flag cfg_lock = ATOMIC_FLAG_INIT;
static Uint64 cfg_writes;   /* with the lock: a read that spans a write does not keep what it read */

static void cfg_lock_take(void) { while (atomic_flag_test_and_set_explicit(&cfg_lock, memory_order_acquire)) {} }
static void cfg_lock_give(void) { atomic_flag_clear_explicit(&cfg_lock, memory_order_release); }

static int cfg_cacheable(const char *name)
{
    const size_t n = strlen(name);
    return n > 4 && strcmp(name + n - 4, ".cfg") == 0;
}

static void cfg_cache_drop(const char *path)   /* with the lock held */
{
    for (int i = 0; i < CFG_CACHE_SLOTS; i++)
        if (cfg_cache[i].data != NULL && strcmp(cfg_cache[i].path, path) == 0) {
            SDL_free(cfg_cache[i].data);
            cfg_cache[i].data = NULL;
        }
}

static void cfg_cache_forget(const char *path)
{
    cfg_lock_take();
    cfg_writes++;
    cfg_cache_drop(path);
    cfg_lock_give();
}

uint8_t *platform_read_user_file(const char *name, size_t *size)
{
    char path[1024];
    if (sdl3_user_path(name, path, sizeof path) == NULL) return NULL;
    if (!cfg_cacheable(name)) return SDL_LoadFile(path, size);
    const Uint64 now = SDL_GetTicks();
    cfg_lock_take();
    for (int i = 0; i < CFG_CACHE_SLOTS; i++) {
        if (cfg_cache[i].data == NULL || strcmp(cfg_cache[i].path, path) != 0 || now - cfg_cache[i].at >= CFG_CACHE_MS) continue;
        uint8_t *copy = SDL_malloc(cfg_cache[i].size + 1);
        if (copy != NULL) {
            memcpy(copy, cfg_cache[i].data, cfg_cache[i].size + 1);
            *size = cfg_cache[i].size;
        }
        cfg_lock_give();
        return copy;
    }
    const Uint64 writes_before = cfg_writes;
    cfg_lock_give();
    uint8_t *data = SDL_LoadFile(path, size);
    cfg_lock_take();
    if (cfg_writes != writes_before) {
        cfg_lock_give();
        return data;
    }
    cfg_cache_drop(path);
    uint8_t *keep = data != NULL ? SDL_malloc(*size + 1) : NULL;
    if (keep != NULL) {
        int slot = 0;
        for (int i = 0; i < CFG_CACHE_SLOTS; i++)
            if (cfg_cache[i].data == NULL) { slot = i; break; }
        memcpy(keep, data, *size + 1);   /* SDL_LoadFile's buffer has a terminating zero past the size */
        SDL_free(cfg_cache[slot].data);
        SDL_snprintf(cfg_cache[slot].path, sizeof cfg_cache[slot].path, "%s", path);
        cfg_cache[slot].data = keep, cfg_cache[slot].size = *size, cfg_cache[slot].at = now;
    }
    cfg_lock_give();
    return data;
}

/* the bytes in `path`, on the disk and not only in the system's buffers when it returns true */
static bool write_to_disk(const char *path, const uint8_t *data, size_t size)
{
    SDL_IOStream *io = SDL_IOFromFile(path, "wb");
    if (io == NULL) return false;
    bool ok = SDL_WriteIO(io, data, size) == size && SDL_FlushIO(io);
#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
    FILE *file = SDL_GetPointerProperty(SDL_GetIOProperties(io), SDL_PROP_IOSTREAM_STDIO_FILE_POINTER, NULL);
    if (ok && file != NULL) ok = fsync(fileno(file)) == 0;
#endif
    return SDL_CloseIO(io) && ok;
}

bool platform_write_user_file(const char *name, const uint8_t *data, size_t size)
{
    char path[1024], temp[1100];
    if (sdl3_user_path(name, path, sizeof path) == NULL) return false;
    cfg_cache_forget(path);
    /* Write beside the target and rename, so a crash cannot truncate a good file. The name is this write's own: two running games do not mix their bytes. */
    SDL_snprintf(temp, sizeof temp, "%s.%llx.%llx.tmp", path, (unsigned long long)SDL_GetCurrentThreadID(), (unsigned long long)SDL_GetPerformanceCounter());
    if (!write_to_disk(temp, data, size)) {
        port_log("platform: cannot write %s: %s\n", temp, SDL_GetError());
        SDL_RemovePath(temp);
        return false;
    }
    if (!SDL_RenamePath(temp, path)) {
        port_log("platform: cannot rename %s: %s\n", temp, SDL_GetError());
        SDL_RemovePath(temp);
        return false;
    }
    cfg_cache_forget(path);   /* a read that began before the rename may have kept the old bytes */
    return true;
}

bool platform_remove_user_file(const char *name)
{
    char path[1024];
    if (sdl3_user_path(name, path, sizeof path) == NULL) return false;
    cfg_cache_forget(path);
    return SDL_RemovePath(path);
}

bool platform_user_file_exists(const char *name)
{
    char path[1024];
    SDL_PathInfo info;
    return sdl3_user_path(name, path, sizeof path) != NULL && SDL_GetPathInfo(path, &info)
           && info.type == SDL_PATHTYPE_FILE;
}

static int compare_names(const void *a, const void *b)
{
    return SDL_strcasecmp(*(char *const *)a, *(char *const *)b);
}

char **platform_list_user_files(const char *pattern, int *count)
{
    char dir[1200];
    *count = 0;
    if (sdl3_user_dir(dir, sizeof dir) == NULL) return NULL;
    return platform_list_files(dir, pattern, count);
}

char **platform_list_files(const char *dir, const char *pattern, int *count)
{
    *count = 0;
    char **names = SDL_GlobDirectory(dir, pattern, SDL_GLOB_CASEINSENSITIVE, count);
    if (names == NULL || *count == 0) {
        SDL_free(names);
        *count = 0;
        return NULL;
    }
    SDL_qsort(names, (size_t)*count, sizeof *names, compare_names);
    return names;
}

void platform_free_names(char **names)
{
    SDL_free(names);
}
