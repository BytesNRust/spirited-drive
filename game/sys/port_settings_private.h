/* The port settings files' shared names (port_settings.c, port_settings_store.c, port_settings_scene.c, port_settings_effects.c), split from port_settings.c
 * with no change in what they do. */
#ifndef TNFS_PORT_SETTINGS_PRIVATE_H
#define TNFS_PORT_SETTINGS_PRIVATE_H

#include <stdlib.h>
#include "game/sys/port_settings.h"

#define PORT_CFG_NAME "port.cfg"
#define FOV_MIN (-10)
#define FOV_MAX 20

/* the LIGHT_TIME_* a name stands for ("dawn" or "a", "day" or "b", "dusk" or "c", "night", any case), or -1 */
int port_settings_light_time_named(const char *name);

/* The switch's environment variable under its name, or under the name it had before the effects' names were made neutral (TNFS_PSX_*): both are read, the new one first. */
static inline const char *port_env_alias(const char *name, const char *old_name)
{
    const char *v = getenv(name);
    return v != NULL ? v : getenv(old_name);
}

#endif
