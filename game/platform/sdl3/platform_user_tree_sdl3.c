/* The SDL3 platform's folders below the player's folder (platform.h, dep59 step 2): the installed mods' tree, and (step 4b) the editions'. Every path is checked to be
 * a clean relative one, and every part of it that exists not to be a symbolic link, before anything is done; a tree is removed without
 * following the links in it. On Windows and the web there is no link check (lstat), only the path's. */
#include "platform/platform.h"

#include "formats/modpath.h"
#include "game/sys/port_log.h"
#include "platform/sdl3/platform_sdl3_private.h"

#include <SDL3/SDL.h>
#include <string.h>
#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
#include <sys/stat.h>
#define TREE_HAS_LSTAT 1
#endif

enum { TREE_PATH = 1400 };

/* 1 when `path` is a symbolic link */
static bool is_link(const char *path)
{
#ifdef TREE_HAS_LSTAT
    struct stat st;
    return lstat(path, &st) == 0 && S_ISLNK(st.st_mode);
#else
    (void)path;
    return false;
#endif
}

/* the folders of the player's folder that may change, and what is in them: the installed mods, their backups, the editions (dep59 4b) */
static const char *const CHANGED[] = { "mods", "mods_backup", "editions" };

/* 1 when the first part of the clean path `clean` is one of CHANGED */
static bool may_change(const char *clean)
{
    const size_t first = strcspn(clean, "/");
    for (size_t i = 0; i < sizeof CHANGED / sizeof CHANGED[0]; i++)
        if (strlen(CHANGED[i]) == first && strncmp(clean, CHANGED[i], first) == 0) return true;
    return false;
}

/* `rel` below the player's folder into `out`: a clean relative path, none of its parts that exist a link; one that is changed (`change`)
 * only in one of CHANGED (or one of them itself), whatever its caller asks */
static bool tree_path_for(const char *rel, char out[TREE_PATH], bool change)
{
    char clean[1024], dir[1200];
    if (rel == NULL || !modpath_clean(rel, clean, sizeof clean) || strcmp(clean, rel) != 0) return false;
    if (change && !may_change(clean)) {
        port_log("platform: %s is not below mods/, mods_backup/ or editions/: not changed\n", clean);
        return false;
    }
    if (sdl3_user_dir(dir, sizeof dir) == NULL) return false;
    if (SDL_snprintf(out, TREE_PATH, "%s%s", dir, clean) >= TREE_PATH) return false;
    const size_t base = strlen(dir);
    for (size_t i = base; ; i++) {   /* each part from the player's folder down */
        if (out[i] != '/' && out[i] != '\0') continue;
        const char keep = out[i];
        out[i] = '\0';
        const bool link = is_link(out);
        out[i] = keep;
        if (link) {
            port_log("platform: %s is a symbolic link: not followed\n", out);
            return false;
        }
        if (keep == '\0') break;
    }
    return true;
}

static bool tree_path(const char *rel, char out[TREE_PATH]) { return tree_path_for(rel, out, false); }
static bool tree_change_path(const char *rel, char out[TREE_PATH]) { return tree_path_for(rel, out, true); }

static bool path_is_dir(const char *path)
{
    SDL_PathInfo info;
    return !is_link(path) && SDL_GetPathInfo(path, &info) && info.type == SDL_PATHTYPE_DIRECTORY;
}

static bool path_exists(const char *path)
{
#ifdef TREE_HAS_LSTAT
    struct stat st;
    if (lstat(path, &st) == 0) return true;
#endif
    return SDL_GetPathInfo(path, NULL);
}

bool platform_user_is_dir(const char *rel)
{
    char path[TREE_PATH];
    return tree_path(rel, path) && path_is_dir(path);
}

bool platform_user_exists(const char *rel)
{
    char path[TREE_PATH];
    return tree_path(rel, path) && path_exists(path);
}

bool platform_user_mkdirs(const char *rel)
{
    char path[TREE_PATH], dir[1200];
    if (!tree_change_path(rel, path) || sdl3_user_dir(dir, sizeof dir) == NULL) return false;
    for (size_t i = strlen(dir); ; i++) {
        if (path[i] != '/' && path[i] != '\0') continue;
        const char keep = path[i];
        path[i] = '\0';
        const bool ok = path_is_dir(path) || (!path_exists(path) && SDL_CreateDirectory(path) && path_is_dir(path));
        if (!ok) port_log("platform: cannot make the folder %s\n", path);
        path[i] = keep;
        if (!ok) return false;
        if (keep == '\0') return true;
    }
}

bool platform_user_rename(const char *from_rel, const char *to_rel)
{
    char from[TREE_PATH], to[TREE_PATH];
    if (!tree_change_path(from_rel, from) || !tree_change_path(to_rel, to) || !path_exists(from) || path_exists(to)) return false;
    if (SDL_RenamePath(from, to)) return true;
    port_log("platform: cannot move %s to %s: %s\n", from, to, SDL_GetError());
    return false;
}

static bool remove_path(const char *path);

static SDL_EnumerationResult remove_entry(void *userdata, const char *dirname, const char *name)
{
    bool *ok = userdata;
    char path[TREE_PATH];
    if (SDL_snprintf(path, sizeof path, "%s%s", dirname, name) >= (int)sizeof path || !remove_path(path)) *ok = false;
    return *ok ? SDL_ENUM_CONTINUE : SDL_ENUM_FAILURE;
}

/* `path` gone: a link or a file removed as it is, a folder emptied first */
static bool remove_path(const char *path)
{
    if (!is_link(path) && path_is_dir(path)) {
        bool ok = true;
        SDL_EnumerateDirectory(path, remove_entry, &ok);
        if (!ok) return false;
    }
    if (SDL_RemovePath(path)) return true;
    port_log("platform: cannot remove %s: %s\n", path, SDL_GetError());
    return false;
}

bool platform_user_remove_tree(const char *rel)
{
    char path[TREE_PATH];
    if (!tree_change_path(rel, path)) return false;
    return !path_exists(path) || remove_path(path);
}

typedef struct {
    char (*names)[256];
    int count, cap;
    bool failed;
} DirList;

static SDL_EnumerationResult list_entry(void *userdata, const char *dirname, const char *name)
{
    DirList *l = userdata;
    char path[TREE_PATH];
    if (strlen(name) >= sizeof l->names[0] || SDL_snprintf(path, sizeof path, "%s%s", dirname, name) >= (int)sizeof path || !path_is_dir(path))
        return SDL_ENUM_CONTINUE;
    if (l->count == l->cap) {   /* as many as there are: a list cut short would miss the newest backup */
        const int cap = l->cap > 0 ? l->cap * 2 : 64;
        char (*grown)[256] = SDL_realloc(l->names, sizeof *grown * (size_t)cap);
        if (grown == NULL) return l->failed = true, SDL_ENUM_FAILURE;
        l->names = grown, l->cap = cap;
    }
    SDL_snprintf(l->names[l->count++], sizeof l->names[0], "%s", name);
    return SDL_ENUM_CONTINUE;
}

static int compare_names(const void *a, const void *b) { return SDL_strcmp(*(char *const *)a, *(char *const *)b); }

char **platform_user_list_dirs(const char *rel, int *count)
{
    char path[TREE_PATH];
    *count = 0;
    if (!tree_path(rel, path) || !path_is_dir(path)) return NULL;
    DirList *l = SDL_calloc(1, sizeof *l);
    if (l == NULL) return NULL;
    SDL_EnumerateDirectory(path, list_entry, l);
    if (l->failed) port_log("platform: the folders of %s could not all be listed (out of memory)\n", path);
    size_t bytes = sizeof(char *) * (size_t)(l->count + 1);
    for (int i = 0; i < l->count; i++) bytes += strlen(l->names[i]) + 1;
    char **out = l->count > 0 ? SDL_malloc(bytes) : NULL;   /* one block, as SDL_GlobDirectory's: platform_free_names frees it */
    if (out != NULL) {
        char *text = (char *)(out + l->count + 1);
        for (int i = 0; i < l->count; i++) {
            const size_t n = strlen(l->names[i]) + 1;
            memcpy(text, l->names[i], n);
            out[i] = text, text += n;
        }
        out[l->count] = NULL;
        SDL_qsort(out, (size_t)l->count, sizeof *out, compare_names);
        *count = l->count;
    }
    SDL_free(l->names);
    SDL_free(l);
    return out;
}

uint8_t *platform_user_read_file_at(const char *rel, size_t *size)
{
    char path[TREE_PATH];
    *size = 0;
    if (!tree_path(rel, path)) return NULL;
    SDL_PathInfo info;
    if (!SDL_GetPathInfo(path, &info) || info.type != SDL_PATHTYPE_FILE) return NULL;
    return SDL_LoadFile(path, size);
}

bool platform_user_write_file_at(const char *rel, const uint8_t *data, size_t size)
{
    char path[TREE_PATH], folder[TREE_PATH];
    if (!tree_change_path(rel, path)) return false;
    SDL_snprintf(folder, sizeof folder, "%s", path);
    char *slash = strrchr(folder, '/');
    if (slash == NULL || (*slash = '\0', !path_is_dir(folder)) || (path_exists(path) && !SDL_RemovePath(path))) return false;
    SDL_IOStream *io = SDL_IOFromFile(path, "wb");
    if (io == NULL) {
        port_log("platform: cannot write %s: %s\n", path, SDL_GetError());
        return false;
    }
    const bool ok = SDL_WriteIO(io, data, size) == size && SDL_FlushIO(io);
    return SDL_CloseIO(io) && ok;
}
