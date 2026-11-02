/* port (the packaged app): where the game's own data files (car_masks/<CAR>/..., car_envmap.png, the light maps) are looked for. An app opened from Finder has / as its
 * working directory, so a relative "data/..." finds nothing; the data lives beside the program instead (on macOS SDL_GetBasePath() is the bundle's Contents/Resources/, our
 * files Contents/Resources/data/). The candidates, in the order they are tried:
 *   1. <base>data/<relative>             the program's own data (the platform sets the base once: data_path_set_base)
 *   2. data/<relative>                   the working directory (a program run from the repository)
 *   3. <source_dir>/data/<relative>      a development build's source tree; the release build has none (TNFS_RELEASE passes NULL) */
#ifndef TNFS_DATA_PATH_H
#define TNFS_DATA_PATH_H

#include <stddef.h>

enum { DATA_PATH_MAX = 1024, DATA_PATH_CANDIDATES = 3 };

/* The program's base directory (with or without its trailing slash), kept for data_path_candidates; NULL or "" for none. */
void data_path_set_base(const char *base);

/* The candidate paths of `relative` for the base, `source_dir` (NULL: none) in that order, into `paths` (the skipped ones are left empty); returns how many there are.
 * A path that would not fit is skipped. */
int data_path_candidates_for(const char *base, const char *source_dir, const char *relative, char paths[DATA_PATH_CANDIDATES][DATA_PATH_MAX]);

/* The same with the base the platform set and the build's own source directory (none in a release build). */
int data_path_candidates(const char *relative, char paths[DATA_PATH_CANDIDATES][DATA_PATH_MAX]);

#endif
