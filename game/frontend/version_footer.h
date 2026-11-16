/* port: the line at the foot of the Game Mode menu: the version, and "Update available: vX" once the update check
 * (platform/update.h) found a newer release. */
#ifndef TNFS_VERSION_FOOTER_H
#define TNFS_VERSION_FOOTER_H

#include "game/platform/update.h"

#include <stddef.h>

/* Line 0: the version. Line 1: "Update available: vX" once the check found a newer release (drawn above line 0).
 * Writes into out (cut to cap - 1 characters). 1 when there is such a line; 0 for none or no room */
int version_footer_line(char *out, size_t cap, int line, const char *version, UpdateState state, const char *latest);

/* port (dep59 step 4): the version line 0 shows: `override` (TNFS_VERSION_TEXT, which tools/menu_walk.sh sets so two builds' Game Mode
 * pictures compare) when it is 1..VERSION_TEXT_MAX printable ASCII characters, else `built` */
enum { VERSION_TEXT_MAX = 31 };
const char *version_footer_text(const char *built, const char *override);

#endif
