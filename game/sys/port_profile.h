/* port (modes): Stock '96, Restored '96 and Restomod '26, each a table of the values it starts from and a list of what the player may change. port.cfg names the mode in use
 * (`profile=`, Stock without it) and the machine's own psx_dir; stock.cfg, restored.cfg and restomod.cfg keep only what the player changed in that mode, so a better value in a new
 * version reaches whoever did not touch it, each mode remembers its own, and going back to a mode's values is emptying its file. config.dat and controls.cfg are shared. */
#ifndef TNFS_PORT_PROFILE_H
#define TNFS_PORT_PROFILE_H

#include "game/sys/port_extras.h"
#include "game/sys/port_settings.h"

typedef enum { PROFILE_STOCK = 0, PROFILE_RESTORED = 1, PROFILE_RESTOMOD = 2, PROFILE_COUNT = 3 } Profile;

/* the name in port.cfg ("stock", ...), the file of the mode's changes ("stock.cfg", ...), the name shown ("Stock '96", ...); the two lines under it in the menu */
const char *port_profile_key(Profile p);
const char *port_profile_file(Profile p);
const char *port_profile_title(Profile p);
const char *port_profile_line1(Profile p);
const char *port_profile_line2(Profile p);
/* the mode a port.cfg name stands for (any case), or -1 */
int port_profile_named(const char *name);

/* the widescreen format that fits a display of w x h: 21:9, 16:9 (also 16:10) or 4:3 */
ScreenFormat port_profile_monitor_format(int width, int height);

/* `s` / `e` with the values of mode `p`'s table (the format Restored '96 and Restomod '26 take is `monitor`) */
PortSettings port_profile_settings(Profile p, PortSettings s, ScreenFormat monitor);
PortExtras port_profile_extras(Profile p, PortExtras e);

/* 1 when the player may change the setting of file key `key` in mode `p`: then its file's value counts. The machine's keys (profile, psx_dir) are never a mode's. */
int port_profile_key_free(Profile p, const char *key);

/* `key` under today's name: the effects' old psx_* keys read as the new ones (light_effects, car_soft_shadow, light_time, headlight_beams, lamp_flares, extra_liveries); any other key as it is. */
const char *port_profile_key_alias(const char *key);
/* `take` on each "key=value" line of `text` whose key is free in `p` */
void port_profile_read_free(Profile p, const char *text, size_t size, void (*take)(void *to, const char *key, const char *value), void *to);
/* the lines of `now` free in `p` that are not in `base` or are in `keep` (NULL for none), NUL-terminated in `out`: their length, 0 for none, -1 when they do not
 * fit. A line that differs and is not free is logged and counted in `*dropped`. */
long port_profile_differences(Profile p, const char *base, const char *now, const char *keep, char *out, size_t room, int *dropped);
/* the file of `s`'s mode: what `s` and `e` have that differs from its values and is free in it, and what `keep` holds even at the mode's value: a choice stays the
 * player's when a later version changes the table; moved back to the table's value by hand it leaves the file. False when nothing could be written; `*dropped` counts what was not free. */
bool port_mode_write(const PortSettings *s, const PortExtras *e, const char *keep, int *dropped);
/* port_mode_write keeping what the mode's file already has; true when the file was written, what was not free in the mode is in the log */
bool port_mode_save(const PortSettings *s, const PortExtras *e);

#endif
