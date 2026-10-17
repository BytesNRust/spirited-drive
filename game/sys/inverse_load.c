#include "game/sys/inverse_load.h"

#include <stdio.h>

int32_t resource_load_inverse(const char *art_dir, const char *name, const InverseLoadOps *ops)
{
    char path[INVERSE_LOAD_PATH];
    snprintf(path, sizeof path, "%s%s", art_dir, name);
    return ops->load(ops->ctx, path, 0x20);
}
