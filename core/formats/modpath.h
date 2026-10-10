/* Path rules for the mods overlay (BYTES-0009 phase 6). The platform layer asks, for each file the game reads,
 * whether the mod directory has its own copy. These functions decide, without touching the file system:
 *   - whether a path lies under the data root (the CD directory);
 *   - the relative name below it, refusing anything that could leave the mod directory (".." components,
 *     absolute names, backslashes, empty components);
 *   - the mod directory's candidate names: as asked and in upper case, as the game itself tries both;
 *   - whether a resolved real path stays inside the real mod directory (symbolic links followed by the caller).
 * Everything is plain strings with '/' separators. */
#ifndef TNFS_MODPATH_H
#define TNFS_MODPATH_H

#include <stddef.h>

/* 1 and `rel` filled when `path` is the data root or below it, with a safe relative remainder; 0 otherwise
 * (not under the root, or unsafe). `data_root` may end with '/'. A leading "./" is dropped from both. */
int modpath_relative(const char *data_root, const char *path, char *rel, size_t cap);

/* mods + '/' + rel. 0 when it does not fit. `rel` must have passed modpath_relative. */
int modpath_join(const char *mods, const char *rel, char *out, size_t cap);

/* `rel` with its letters in upper case. */
int modpath_upper(const char *rel, char *out, size_t cap);

/* Whether `real_path` equals or lies below `real_root`, compared by whole components. */
int modpath_inside(const char *real_root, const char *real_path);

#endif
