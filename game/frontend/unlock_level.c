/* port: see unlock_level.h. */
#include "game/frontend/unlock_level.h"

#include "game/sys/port_extras.h"
#include "game/sys/port_log.h"
#include "game/sys/port_profile.h"
#include "game/sys/port_settings.h"

int unlock_level_own(void)
{
    return port_settings_profile() == PROFILE_RESTOMOD;
}

int unlock_level_get(const ConfigDat *c)
{
    if (unlock_level_own()) return port_extras_load().unlock_level;
    return config_dat_unlock_level(c);
}

PortExtras unlock_level_gate(PortExtras e, int level)
{
    if (level < 2) e.lunar_springs = 0;
    return e;
}

PortExtras unlock_level_extras(const ConfigDat *c)
{
    return unlock_level_gate(port_extras_load(), unlock_level_get(c));
}

void unlock_level_put(ConfigDat *c, int level)
{
    if (!unlock_level_own()) {
        config_dat_set_unlock_level(c, level);
        return;
    }
    PortExtras e = port_extras_load();
    const int clamped = level < 0 ? 0 : level > 2 ? 2 : level;
    if (e.unlock_level == clamped) return;
    e.unlock_level = clamped;
    if (!port_extras_save(&e)) port_log("unlock_level: Restomod's level %d was not saved\n", clamped);
}
