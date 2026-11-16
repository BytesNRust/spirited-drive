/* port (dep59 step 4, T7): what an edition of the player's own (formats/edition_ini.h) opens and closes (notes/dep59-step4-editions-plan.md,
 * section 3). The standard edition (NULL) is today's game: everything as the unlock level and the cheat codes leave it. An edition of the
 * player's own unlocks the mirror, the rally and Lost Vegas; the Warrior only when cars= lists it and the hidden track only when tracks=
 * does; its unlock level is 2 and is never written (unlock_level.h), and the unlock codes do nothing in it (central_keys.c). The
 * tournament needs the stock cars and tracks; the network, the standard edition. Pure: the edition in use is edition_file.h's. */
#ifndef TNFS_EDITION_RULES_H
#define TNFS_EDITION_RULES_H

#include "formats/edition_ini.h"
#include "game/frontend/extras_apply.h"
#include "game/frontend/race_settings.h"
#include "game/sys/port_extras.h"

typedef struct {
    int tournament, network;                     /* 1 open */
    const char *tournament_why, *network_why;    /* the reason a closed one is, for the player; NULL when open */
} EditionGate;

/* `e` NULL: all open */
EditionGate edition_gate(const Edition *e);

/* 1 when `e` lists every stock car (rx7..diablo) and every stock track (city..transtropolis), in any order, and nothing but stock
 * ids (the Warrior, vegas and hidden may be there too) */
int edition_is_stock(const Edition *e);

/* what `e` unlocks: the mirror, the rally and Lost Vegas; the Warrior when cars= lists it, the hidden track when tracks= does */
Unlocks edition_unlocks(const Edition *e);

/* 1 when `e` lists rusty: Lunar Springs (the switch on Rusty's rally, and Lunar Gravity's "Lunar Springs") can be on */
int edition_lunar_springs(const Edition *e);

/* the mode's switches as `e` lets them be: the Warrior (as the player's car and as the rival) on when listed and off when not; Lunar
 * Springs off without rusty */
PortExtras edition_extras(const Edition *e, PortExtras x);

/* what extras_settle_to expects on in `e` (edition_unlocks) */
ExtrasExpect edition_expect(const Edition *e);

#endif
