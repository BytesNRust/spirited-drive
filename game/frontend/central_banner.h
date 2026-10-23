/* port: Control Central's CHEATS ENABLED banner beside the RACE item: on while a cheat that sends the race's records to
 * the cheats table is on, the records routing's own predicate (records_route.h's records_has_cheats over the kind the
 * flags make). Mirror and rally are variants, not cheats, so they alone show nothing. */
#ifndef TNFS_CENTRAL_BANNER_H
#define TNFS_CENTRAL_BANNER_H

#include "game/frontend/central_run.h"

/* 1 when the flags Control Central holds (explode, nitro, Turbo in track_flags, the Warrior as the player's car) put
 * the race's records on the cheats table */
int central_cheats_on(const ControlCentralState *st);
/* the words, drawn over the frame as central_draw has it (which blanks the frame first, so a banner that goes leaves
 * nothing); sets r->banner_shown */
void central_banner(CentralRun *r);
/* 1 when the banner on the frame is not what the flags say now: the frame is drawn again */
int central_banner_stale(const CentralRun *r);

#endif
