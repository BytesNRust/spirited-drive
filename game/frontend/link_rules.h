/* port (online): the host's rules of the session (session_rules.h) from this machine's own settings, and a guest's from the trailer the host sent. */
#ifndef TNFS_LINK_RULES_H
#define TNFS_LINK_RULES_H

#include "game/race/session_rules.h"

#include <stdint.h>

/* The host's rules: its mode, difficulty, weather, reverse, the moon, gravel, cheats, rain grip, colours, time of day and each human's paint (`records`: the setup's players' records,
 * RACE_NET_INFO bytes each with the entry at +2; `humans` of them) */
void link_rules_compose(const uint8_t *records, int humans, SessionRules *out);

/* The host's own pick of paint for the car it drives (the entry's paint_pick): this machine's CUSTOMIZE choice for it + 1 (0: none), whether or not its own colours switch is on */
uint8_t link_rules_paint_pick(int car);

#endif
