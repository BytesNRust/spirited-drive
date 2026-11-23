/* A path as a person types it: a leading ~ (alone or before a /), $HOME or ${HOME} stands for the home folder. Nothing else is expanded.
 * `home` is the folder it stands for (NULL or "": the path stays as it was). Returns 1 when something was expanded, else 0; `out` is always the path to use (cut to `cap`). */
#ifndef EDITOR_PATH_EXPAND_H
#define EDITOR_PATH_EXPAND_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
int path_expand(const char *in, const char *home, char *out, size_t cap);
/* the same with the environment's HOME, into one of a few rotating buffers (for the command line's options: the pointer stays valid for the run) */
const char *path_expand_arg(const char *in);
#ifdef __cplusplus
}
#endif
#endif
