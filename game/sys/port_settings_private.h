/* The port settings files' shared names (port_settings.c, port_settings_store.c, port_settings_scene.c, port_settings_effects.c), split from port_settings.c
 * with no change in what they do. */
#ifndef TNFS_PORT_SETTINGS_PRIVATE_H
#define TNFS_PORT_SETTINGS_PRIVATE_H

#include "game/sys/port_settings.h"

#define PORT_CFG_NAME "port.cfg"
#define FOV_MIN (-10)
#define FOV_MAX 20

/* the PSX_TIME_* a name stands for ("dawn" or "a", "day" or "b", "dusk" or "c", "night", any case), or -1 */
int port_settings_psx_time_named(const char *name);

#endif
