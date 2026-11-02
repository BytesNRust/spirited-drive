/* port: see data_root.h. */
#include "game/sys/data_root.h"

#include "game/sys/data_check.h"

#include <stdio.h>

DataRootSource data_root_resolve(const char *explicit_root, const char *user_copy, const char *dev_root, char *out, size_t out_size)
{
    if (explicit_root != NULL && explicit_root[0] != '\0') {
        snprintf(out, out_size, "%s", explicit_root);
        return DATA_ROOT_EXPLICIT;
    }
    if (user_copy != NULL && data_looks_installed(user_copy)) {
        snprintf(out, out_size, "%s", user_copy);
        return DATA_ROOT_USER_COPY;
    }
    if (dev_root != NULL && data_looks_installed(dev_root)) {
        snprintf(out, out_size, "%s", dev_root);
        return DATA_ROOT_DEV;
    }
    out[0] = '\0';
    return DATA_ROOT_NEEDS_SETUP;
}
