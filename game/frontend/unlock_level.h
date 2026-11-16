/* port: the unlock level of the mode in use (0 nothing, 1 mirror + Warrior + Lost Vegas, 2 the rally too; config_dat_unlocks).
 * Stock and Restored keep config.dat's, as the exe; Restomod keeps its own in its mode's file (port_extras.h's unlock_level, 2
 * until the player changes it) and never reads or writes config.dat's. Both are read at the call, from the mode in use then:
 * nothing is kept across a change of mode. */
#ifndef TNFS_UNLOCK_LEVEL_H
#define TNFS_UNLOCK_LEVEL_H

#include "game/frontend/config_dat.h"
#include "formats/edition_ini.h"
#include "game/sys/port_extras.h"

enum { UNLOCK_LEVEL_EDITION = 2 };   /* port (dep59 step 4): an edition of the player's own unlocks everything (edition_rules.h) */

/* 1 when the mode in use keeps its own level (Restomod) */
int unlock_level_own(void);
/* the level of the mode in use, 0..2 for Restomod's, config.dat's word as it is for the others; an edition's (edition_file.h's
 * edition_active), UNLOCK_LEVEL_EDITION */
int unlock_level_get(const ConfigDat *c);
/* the level written where the mode in use keeps it: Restomod's mode file (0..2, clamped), else config.dat's word; in an edition,
 * nowhere */
void unlock_level_put(ConfigDat *c, int level);

/* the mode's extras (port_extras_load) with what the level does not give yet taken off: the Lunar Springs Track switch is
 * level 2's (the rally's), so below it the rally on Rusty Springs is the desert's whatever the file says; in an edition it needs rusty
 * in tracks= instead. What a race,
 * Control Central's card and a host's session rules read. */
PortExtras unlock_level_extras(const ConfigDat *c);
/* the same gate over `e` for a level already read (Control Central's own word); in `edition` (not NULL) Lunar Springs needs rusty
 * listed instead (edition_rules.h), whatever the level */
PortExtras unlock_level_gate(PortExtras e, int level, const Edition *edition);

#endif
