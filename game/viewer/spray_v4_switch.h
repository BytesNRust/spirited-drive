/* port (spray v4): whether this frame draws spray v4's dust and spray, and its emitter on the sim's ticks, following the settings as they are now (port_follow.h):
 * a change of mode or of the row with no restart is the next race's (before, the switch was read once, and a game started in Stock kept the old dust in Restomod). */
#ifndef TNFS_SPRAY_V4_SWITCH_H
#define TNFS_SPRAY_V4_SWITCH_H

#include "game/race/race_state.h"

/* 1 when spray v4 is on: port_spray_v4 (the mode's spray_v4, TNFS_SPRAY_V4 over it), read again after any change of the settings */
int spray_v4_switch_on(void);

/* the sim's tick hook (race_sim_set_tick_hook): `tick` while spray v4 is on, none when it is off */
void spray_v4_switch_hook(void (*tick)(const RaceState *race));

#endif
