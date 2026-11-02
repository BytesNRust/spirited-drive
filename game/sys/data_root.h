/* port (the first-run wizard): where the CD's data is. In order: --data (explicit: taken as it is), the player's own copy (<user dir>/gamedata, made by the wizard),
 * for a development build the repository's original/ when it is there; none of them: the wizard has to make one. */
#ifndef TNFS_DATA_ROOT_H
#define TNFS_DATA_ROOT_H

#include <stddef.h>

typedef enum { DATA_ROOT_EXPLICIT, DATA_ROOT_USER_COPY, DATA_ROOT_DEV, DATA_ROOT_NEEDS_SETUP } DataRootSource;

/* `explicit_root` NULL when --data was not given; `user_copy` the wizard's folder (NULL: none); `dev_root` a development build's own folder (NULL in a release build).
 * Writes the folder to `out` (empty for DATA_ROOT_NEEDS_SETUP) and says which it is. */
DataRootSource data_root_resolve(const char *explicit_root, const char *user_copy, const char *dev_root, char *out, size_t out_size);

#endif
