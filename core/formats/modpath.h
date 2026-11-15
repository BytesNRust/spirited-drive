/* Path rules for the mods overlay (BYTES-0009 phase 6). The platform layer asks, for each file the game reads,
 * whether the mod directory has its own copy. These functions decide, without touching the file system:
 *   - whether a path lies under the data root (the CD directory);
 *   - the relative name below it, refusing anything that could leave the mod directory (".." components,
 *     absolute names, backslashes, empty components);
 *   - the mod directory's candidate names, in one fixed order: as asked, in upper case, in lower case (the game
 *     asks in either case and the CD is upper case, while a mod made on a case-sensitive system may name its files
 *     in either), the file system asked through a callback;
 *   - whether a resolved real path stays inside the real mod directory (symbolic links followed by the caller).
 * Everything is plain strings with '/' separators. */
#ifndef TNFS_MODPATH_H
#define TNFS_MODPATH_H

#include <stddef.h>

/* 1 and `rel` filled when `path` is the data root or below it, with a safe relative remainder; 0 otherwise
 * (not under the root, or unsafe). `data_root` may end with '/'. A leading "./" is dropped from both. */
int modpath_relative(const char *data_root, const char *path, char *rel, size_t cap);

/* 1 and `rel` filled when `name` (a zip entry's, a path inside a mod) is a safe relative path by the same rules: not absolute, no
 * backslash, no ".." component; "." and doubled slashes dropped, a trailing '/' too. 0 for an unsafe name or one that is left empty. */
int modpath_clean(const char *name, char *rel, size_t cap);

/* mods + '/' + rel. 0 when it does not fit. `rel` must have passed modpath_relative. */
int modpath_join(const char *mods, const char *rel, char *out, size_t cap);

/* Whether `path` is a file (the caller's file system, or a test's). */
typedef int (*ModpathIsFile)(const char *path, void *ctx);

/* The mod's copy of `rel` (which passed modpath_relative): the first of `rel` as asked, in upper case, in lower case
 * (a form equal to an earlier one is not asked again) that `is_file` says is a file below `mods`. 1 with its path in `out`
 * and the relative name that found it in `name`; 0 when there is none or a name does not fit. */
int modpath_find(const char *mods, const char *rel, ModpathIsFile is_file, void *ctx, char *out, size_t out_cap, char *name, size_t name_cap);

/* Whether `real_path` equals or lies below `real_root`, compared by whole components. */
int modpath_inside(const char *real_root, const char *real_path);

#endif
