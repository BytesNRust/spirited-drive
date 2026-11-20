/* A long path in one line of a panel: the start and the end kept, "..." in the middle (the full path goes in the line's tooltip). Pure C, no window. */
#ifndef EDITOR_PATH_ELIDE_H
#define EDITOR_PATH_ELIDE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { PATH_ELIDE_LEAST = 7 };   /* "/...mac": the fewest characters an elided path keeps */

/* `path` into `out` (cap bytes, always ended) in at most `max` characters (PATH_ELIDE_LEAST at least when it has to be cut): as it is when it fits, else
 * its first part, "...", and its last part, the last a little longer (the end names the folder) */
void path_elide(const char *path, int max, char *out, size_t cap);

#ifdef __cplusplus
}
#endif

#endif
