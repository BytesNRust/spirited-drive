/* port: test harness only. TNFS_AUTOPILOT=1 drives the human cars for
 * headless runs (a race to its finish with no one at the keys): not a game
 * option, in no menu and not in --help. Unset, nothing changes. */
#ifndef TNFS_DRIVE_AUTOPILOT_H
#define TNFS_DRIVE_AUTOPILOT_H

#include "game/race/car.h"
#include "game/race/race_state.h"

#include <stdint.h>

/* port: test harness only. TNFS_AUTOPILOT is 1 (read once) */
int drive_autopilot_on(void);

/* port: test harness only. The keyboard word a player at the keys would
 * give: steer towards the road's aim (00401010, the controller's own) once
 * it is past what the keyboard's assist takes, the brake into a sharp aim
 * at speed, else full throttle; in neutral, gear up (A) pressed and let go
 * until the automatic takes drive. */
uint32_t drive_autopilot_word(const Car *car, const RaceState *race);

#endif
