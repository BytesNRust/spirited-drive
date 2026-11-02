/* port: the line at the foot of the Game Mode menu: the version, and "Update available: vX" once the update check
 * (platform/update.h) found a newer release. */
#ifndef TNFS_VERSION_FOOTER_H
#define TNFS_VERSION_FOOTER_H

#include "game/platform/update.h"

#include <stddef.h>

/* Line 0: the version. Line 1: "Update available: vX" once the check found a newer release (drawn above line 0).
 * Writes into out (cut to cap - 1 characters). 1 when there is such a line; 0 for none or no room */
int version_footer_line(char *out, size_t cap, int line, const char *version, UpdateState state, const char *latest);

#endif
