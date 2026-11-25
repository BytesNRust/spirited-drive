/* port (profiles): what the first start's "Pick your profile" window shows: the order of the three profiles (least change to most, the order of the Game
 * Mode selector), the one Return and Escape take (Restored '96, the middle one), and the window's own text. Game Mode keeps the short lines of
 * port_profile.c; this window says in a few words what each one changes. No SDL: first_run_sdl3.c shows it, a test pins it. */
#ifndef TNFS_FIRST_PROFILE_H
#define TNFS_FIRST_PROFILE_H

#include "game/sys/port_profile.h"

#include <stddef.h>

enum { FIRST_PROFILE_COUNT = 3, FIRST_PROFILE_DEFAULT = 1 };   /* the index in the order of the profile Return and Escape take */

/* the three profiles in the order the window lists them (buttons left to right) */
void first_profile_order(Profile out[FIRST_PROFILE_COUNT]);

/* what one profile changes, in words for a player who never saw the game; NULL for an unknown profile */
const char *first_profile_changes(Profile p);

/* the window's whole text into `out` (NUL ended, cut to `size`); returns its length without the NUL */
int first_profile_text(char *out, size_t size);

#endif
