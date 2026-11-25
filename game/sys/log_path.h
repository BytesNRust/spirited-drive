/* port: a path as the bug-report log says it: the player's name is not in it. */
#ifndef TNFS_LOG_PATH_H
#define TNFS_LOG_PATH_H

#include <stddef.h>

/* `path` with the player's folder at its start written "<user>" and, failing that, the home folder written "~" (either may be NULL or ""; a folder matches only whole names, so "/Users/ann"
 * does not match "/Users/annie"); any other path as it is. Both separators count. Cut to `cap` bytes. */
void log_path_for_report(const char *path, const char *home, const char *user_dir, char *out, size_t cap);

#endif
