/* See mod_set.h: the run's mark that --mods served a file that changes the race (dep59, temporary until step 5), on its own so that the
 * records and the replay can read it without the rest of mod_set. */
#include "game/sys/mod_set.h"

static bool modified;

bool mod_set_session_modified(void) { return modified; }
void mod_set_session_mark_modified(void) { modified = true; }
void mod_set_session_clear(void) { modified = false; }
