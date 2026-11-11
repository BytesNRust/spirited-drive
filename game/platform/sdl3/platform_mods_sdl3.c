/* The SDL3 platform's mods overlay and the player's folder moves (platform_sdl3.c split by responsibility; the shared names are in platform_sdl3_private.h). */
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

char sdl3_user_override[1024];   /* --user DIR */
static char mods_data_root[1024];
static char mods_dir_path[1024];
static char mods_real_path[1100];
static char mods_error[1024];

/* ---- mods overlay: the rules are in formats/modpath.c, the validation in formats/validate.c */


bool platform_set_mods(const char *data_root, const char *mods_dir)
{
    mods_error[0] = '\0';
    char fallback[1200];
    if (mods_dir == NULL || mods_dir[0] == '\0') {   /* the default: <user folder>/mods/, when it is there; no folder, no mods */
        mods_dir_path[0] = '\0';
        if (sdl3_user_dir(fallback, sizeof fallback) == NULL || strlen(fallback) + sizeof "mods" >= sizeof fallback) return true;
        strcat(fallback, "mods");
        SDL_PathInfo found;
        if (!SDL_GetPathInfo(fallback, &found) || found.type != SDL_PATHTYPE_DIRECTORY) return true;
        mods_dir = fallback;
    }
    SDL_PathInfo info;
    if (!SDL_GetPathInfo(mods_dir, &info) || info.type != SDL_PATHTYPE_DIRECTORY) {
        SDL_snprintf(mods_error, sizeof mods_error, "mods directory %s does not exist", mods_dir);
        port_fatal("platform: %s\n", mods_error);
        return false;
    }
    SDL_snprintf(mods_data_root, sizeof mods_data_root, "%s", data_root);
    SDL_snprintf(mods_dir_path, sizeof mods_dir_path, "%s", mods_dir);
#ifdef TNFS_HAVE_REALPATH
    char real[PATH_MAX];
    if (realpath(mods_dir, real) == NULL) { port_fatal("platform: cannot resolve %s\n", mods_dir); mods_dir_path[0] = '\0'; return false; }
    SDL_snprintf(mods_real_path, sizeof mods_real_path, "%s", real);
#else
    SDL_snprintf(mods_real_path, sizeof mods_real_path, "%s", mods_dir);
#endif
    return true;
}

const char *platform_mods_dir(void)
{
    return mods_dir_path[0] != '\0' ? mods_dir_path : NULL;
}

/* ---- the player's folder moves: an item of `from` the destination lacks is moved there (a folder item by item); one the destination has stays where
 * it is (the destination wins, the old copy is left intact). */
static int move_item(const char *from, const char *to)
{
    if (SDL_RenamePath(from, to)) return 1;
    SDL_PathInfo info;
    if (SDL_GetPathInfo(from, &info) && info.type == SDL_PATHTYPE_FILE && SDL_CopyFile(from, to)) {   /* another volume */
        if (!SDL_RemovePath(from)) port_log("user: %s was copied but could not be removed\n", from);
        return 1;
    }
    port_log("user: %s could not be moved to %s: %s\n", from, to, SDL_GetError());
    return 0;
}

typedef struct { const char *to; int moved; } MoveJob;

static int move_tree(const char *from, const char *to);

static SDL_EnumerationResult move_entry(void *userdata, const char *dirname, const char *name)
{
    MoveJob *job = userdata;
    char a[1600], b[1600];
    SDL_snprintf(a, sizeof a, "%s%s", dirname, name);
    SDL_snprintf(b, sizeof b, "%s%s", job->to, name);
    SDL_PathInfo have, there;
    if (!SDL_GetPathInfo(a, &have)) return SDL_ENUM_CONTINUE;
    if (!SDL_GetPathInfo(b, &there)) {
        job->moved += move_item(a, b);
    } else if (have.type == SDL_PATHTYPE_DIRECTORY && there.type == SDL_PATHTYPE_DIRECTORY) {
        char fa[1604], fb[1604];
        SDL_snprintf(fa, sizeof fa, "%s/", a);
        SDL_snprintf(fb, sizeof fb, "%s/", b);
        job->moved += move_tree(fa, fb);
    }
    return SDL_ENUM_CONTINUE;
}

static int move_tree(const char *from, const char *to)
{
    MoveJob job = { to, 0 };
    SDL_CreateDirectory(to);
    SDL_EnumerateDirectory(from, move_entry, &job);
    return job.moved;
}

int platform_move_user_files(const char *from, const char *to)
{
    char a[1200], b[1200];
    if (from == NULL || to == NULL || from[0] == '\0' || to[0] == '\0') return 0;
    SDL_snprintf(a, sizeof a, "%s%s", from, from[strlen(from) - 1] == '/' ? "" : "/");
    SDL_snprintf(b, sizeof b, "%s%s", to, to[strlen(to) - 1] == '/' ? "" : "/");
    SDL_PathInfo info;
    if (!SDL_GetPathInfo(a, &info) || info.type != SDL_PATHTYPE_DIRECTORY) return 0;
    return move_tree(a, b);
}

void platform_set_user_dir(const char *dir)
{
    if (dir == NULL || dir[0] == '\0') { sdl3_user_override[0] = '\0'; return; }
    SDL_snprintf(sdl3_user_override, sizeof sdl3_user_override, "%s%s", dir, dir[strlen(dir) - 1] == '/' ? "" : "/");
}

/* Where the player's folder was before it was named USER_FOLDER_NAME, newest first, each beside it in the OS's data folder ('/' between parts) */
static const char *const LEGACY_USER_FOLDERS[] = { "TNFS Restomod", "tnfs/tnfs" };

/* The old folder `rel` beside the player's folder `pref`, found without making it. */
static bool legacy_pref_dir(const char *pref, const char *rel, char *out, size_t out_size)
{
    char base[1024];
    SDL_snprintf(base, sizeof base, "%s", pref);
    size_t n = strlen(base);
    while (n > 0 && (base[n - 1] == '/' || base[n - 1] == '\\')) base[--n] = '\0';   /* ...<base>/<USER_FOLDER_NAME> */
    while (n > 0 && base[n - 1] != '/' && base[n - 1] != '\\') base[--n] = '\0';     /* ...<base>/ */
    if (n == 0) return false;
    const char sep = base[n - 1];
    const int len = SDL_snprintf(out, out_size, "%s%s%c", base, rel, sep);
    if (len < 0 || (size_t)len >= out_size) return false;
    for (size_t i = n; i < (size_t)len; i++)
        if (out[i] == '/') out[i] = sep;
    return true;
}

int platform_migrate_user_folders(const char *pref)
{
    int total = 0;
    for (size_t i = 0; i < SDL_arraysize(LEGACY_USER_FOLDERS); i++) {
        char legacy[1200];
        if (!legacy_pref_dir(pref, LEGACY_USER_FOLDERS[i], legacy, sizeof legacy)) continue;
        const int moved = platform_move_user_files(legacy, pref);
        if (moved > 0) port_log("user: %d item(s) moved from %s to %s\n", moved, legacy, pref);
        total += moved;
    }
    return total;
}

static void migrate_user_files(const char *mods_dir)
{
    char dir[1200], from[1300];
    if (sdl3_user_dir(dir, sizeof dir) == NULL) return;
    if (sdl3_user_override[0] == '\0' && SDL_getenv("TNFS_PREF_DIR") == NULL) platform_migrate_user_folders(dir);   /* the OS's own folder: what the old names held comes along */
    if (mods_dir != NULL && mods_dir[0] != '\0') {   /* --mods no longer holds the player's files: those it did (<mods>/user/) move to the player's folder */
        SDL_snprintf(from, sizeof from, "%s/user/", mods_dir);
        const int moved = platform_move_user_files(from, dir);
        if (moved > 0) port_log("user: %d item(s) moved from %s to %s (--mods no longer keeps the player's files; --user DIR says where they go)\n", moved, from, dir);
    }
}

void platform_open_log(const char *mods_dir)
{
    data_path_set_base(SDL_GetBasePath());
    sdl3_open_log();
    migrate_user_files(mods_dir);
}

const char *platform_mods_error(void)
{
    return mods_error;
}

/* The mod's copy of `path`, validated; *refused is set when there is a copy the game must not get. */
static uint8_t *read_mod_file(const char *path, size_t *size, bool *refused)
{
    char rel[512], candidate[1600], upper[512];
    *refused = false;
    if (!modpath_relative(mods_data_root, path, rel, sizeof rel)) return NULL;
    if (rel[0] == '\0') return NULL;
    const char *names[2] = { rel, upper };
    if (!modpath_upper(rel, upper, sizeof upper)) return NULL;
    for (int k = 0; k < 2; ++k) {
        if (!modpath_join(mods_dir_path, names[k], candidate, sizeof candidate)) continue;
        SDL_PathInfo info;
        if (!SDL_GetPathInfo(candidate, &info) || info.type != SDL_PATHTYPE_FILE) continue;
#ifdef TNFS_HAVE_REALPATH
        char real[PATH_MAX];
        if (realpath(candidate, real) == NULL || !modpath_inside(mods_real_path, real)) {
            SDL_snprintf(mods_error, sizeof mods_error, "mod file %s leaves the mods directory (symbolic link)", names[k]);
            port_fatal("platform: %s\n", mods_error);
            *refused = true;
            return NULL;
        }
#endif
        uint8_t *data = SDL_LoadFile(candidate, size);
        if (data == NULL) {
            SDL_snprintf(mods_error, sizeof mods_error, "mod file %s cannot be read: %s", names[k], SDL_GetError());
            port_fatal("platform: %s\n", mods_error);
            *refused = true;
            return NULL;
        }
        Report report;
        report_init(&report);
        validate_file(data, *size, names[k], &report);
        if (report_fatal_count(&report) > 0) {
            char text[700];
            report_text(&report, VALIDATE_COMPATIBLE, text, sizeof text);
            SDL_snprintf(mods_error, sizeof mods_error, "mod file %s is not valid for the original game: %s", names[k], text);
            port_fatal("platform: %s\n", mods_error);
            SDL_free(data);
            report_free(&report);
            *refused = true;
            return NULL;
        }
        report_free(&report);
        return data;
    }
    return NULL;
}

uint8_t *platform_read_file(const char *path, size_t *size)
{
    if (mods_dir_path[0] != '\0') {
        bool refused;
        uint8_t *mod = read_mod_file(path, size, &refused);
        if (mod != NULL || refused) return mod;
    }
    uint8_t *data = SDL_LoadFile(path, size);
    if (data == NULL) port_log("platform: cannot read %s: %s\n", path, SDL_GetError());
    return data;
}

void platform_free_file(uint8_t *data)
{
    SDL_free(data);
}
