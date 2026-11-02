/* port: the line at the foot of the Game Mode menu: the version, and "Update available: vX" once the update check
 * (platform/update.h) found a newer release. */
#ifndef TNFS_VERSION_FOOTER_H
#define TNFS_VERSION_FOOTER_H

#include "game/platform/update.h"

#include <stddef.h>

/* Writes the line into out (cut to cap - 1 characters). 1 when there is a line; 0 for no version or no room */
int version_footer_line(char *out, size_t cap, const char *version, UpdateState state, const char *latest);

#endif
