/* port (live settings): a value read from a setting and kept between reads follows every change of the settings: a change of mode in Game Mode or Options, a row of
 * Options, Graphics, CUSTOMIZE or EXTRAS saved, a mode's Restore. Every write of a settings file goes through port_settings_forget, which moves the settings' generation on;
 * a kept value made under an older generation is read again. Anything held in memory that depends on a setting keeps its value in a PortFollow (or compares
 * port_settings_generation itself) instead of a static that is read once. */
#ifndef TNFS_PORT_FOLLOW_H
#define TNFS_PORT_FOLLOW_H

#include <stdint.h>

/* the settings' generation: never 0, moved on by every write of a settings file (port_settings_forget) */
uint32_t port_settings_generation(void);

typedef struct {
    uint32_t seen;   /* the generation `value` was read under; 0: never read */
    int value;
} PortFollow;

/* `read`'s value, read again only when the settings changed since the last call with `f` (a zeroed PortFollow reads at the first call) */
int port_follow(PortFollow *f, int (*read)(void));

#endif
