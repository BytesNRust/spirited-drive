/* port (dep59 step 5, T6): the one flag of mod_set.h's mod_set_places_served, apart from mod_set.c so that what reads it needs nothing else */
#include "game/sys/mod_set.h"

static bool served;

bool mod_set_places_served(void) { return served; }
void mod_set_places_mark(void) { served = true; }
void mod_set_places_clear(void) { served = false; }
