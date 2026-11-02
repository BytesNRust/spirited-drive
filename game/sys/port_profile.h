/* port (profiles, plan 16.1): Stock '96, Restored '96 and Restomod '26, each a table of the settings it controls. port.cfg stores the profile's NAME (`profile=`), not the
 * values: with a profile chosen the controlled settings are taken from its table whatever the files say, so an update that adds an improvement turns it on by itself in
 * Restomod '26 and leaves Stock alone; touching any controlled setting by hand makes the profile "custom", which keeps the values as they are. No `profile=` line (a file from
 * before the profiles) is custom, so nothing changes for anyone who had set things up. */
#ifndef TNFS_PORT_PROFILE_H
#define TNFS_PORT_PROFILE_H

#include "game/sys/port_extras.h"
#include "game/sys/port_settings.h"

typedef enum { PROFILE_CUSTOM = 0, PROFILE_STOCK = 1, PROFILE_RESTORED = 2, PROFILE_RESTOMOD = 3, PROFILE_COUNT = 4 } Profile;

/* the name in port.cfg ("stock", ...) and the one shown ("Stock '96", ...); the two lines under it in the menu ("" for none) */
const char *port_profile_key(Profile p);
const char *port_profile_title(Profile p);
const char *port_profile_line1(Profile p);
const char *port_profile_line2(Profile p);
/* the profile a port.cfg name stands for (any case), or -1 */
int port_profile_named(const char *name);

/* the widescreen format that fits a display of w x h: 21:9, 16:9 (also 16:10) or 4:3 */
ScreenFormat port_profile_monitor_format(int width, int height);

/* `s` / `e` with the settings profile `p` controls set to its table (the format Restored '96 and Restomod '26 take is `monitor`); custom returns them unchanged */
PortSettings port_profile_settings(Profile p, PortSettings s, ScreenFormat monitor);
PortExtras port_profile_extras(Profile p, PortExtras e);
/* 1 when every setting `p` controls is as its table has it (custom: always) */
int port_profile_settings_match(Profile p, const PortSettings *s, ScreenFormat monitor);
int port_profile_extras_match(Profile p, const PortExtras *e);

/* Undo for the Options screen's Cancel: begin arms it (and forgets an older one); the first change a profile step makes calls note, which keeps port.cfg and extras.cfg as
 * they are; restore writes them back (1 done, 0 nothing was kept, -1 a write failed); end (Done) forgets. Without begin, note keeps nothing (Game Mode's row has no undo). */
void port_profile_undo_begin(void);
void port_profile_undo_note(void);
int port_profile_undo_restore(void);
void port_profile_undo_end(void);

#endif
