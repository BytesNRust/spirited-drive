/* port: a path as the bug-report log says it: the player's name is not in it. */
#ifndef TNFS_LOG_PATH_H
#define TNFS_LOG_PATH_H

#include <stddef.h>

/* `path` with the player's folder at its start written "<user>" and, failing that, the home folder written "~" (either may be NULL or ""; a folder matches only whole names, so "/Users/ann"
 * does not match "/Users/annie"); any other path as it is. Both separators count. Cut to `cap` bytes. */
void log_path_for_report(const char *path, const char *home, const char *user_dir, char *out, size_t cap);

/* `text` (a line of the log, or any text) with every place that the player's folder or the home folder is named written the way log_path_for_report writes it: "<user>" and "~". A folder
 * is found anywhere in the text, but only where the name before it is not part of a longer one ("/data/hm/pat" is not "/hm/pat") and a whole name follows. Cut to `cap` bytes
 * (NUL-terminated); returns the length written. */
size_t log_text_for_report(const char *text, const char *home, const char *user_dir, char *out, size_t cap);

#endif
