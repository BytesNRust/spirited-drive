/* See mod_set.h: the mods' generation alone, apart from mod_set.c, so what follows it (a cache) needs neither the platform nor the
 * settings to be linked, and its test can move it on. */
#include "game/sys/mod_set.h"

static uint32_t generation = 1;

uint32_t mod_set_generation(void) { return generation; }

void mod_set_generation_move(void) { generation = generation == UINT32_MAX ? 1 : generation + 1; }
