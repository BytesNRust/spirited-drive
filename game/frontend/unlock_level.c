/* port: see unlock_level.h. */
#include "game/frontend/unlock_level.h"

#include "game/frontend/edition_rules.h"
#include "game/sys/edition_file.h"

#include "game/sys/port_extras.h"
#include "game/sys/port_log.h"
#include "game/sys/port_profile.h"
#include "game/sys/port_settings.h"

int unlock_level_own(void)
{
    return port_settings_profile() == PROFILE_RESTOMOD;
}

/* port (dep59 step 4): the edition of the player's own in use, NULL for the standard one */
static const Edition *own_edition(Edition *buf) { return unlock_level_own() ? edition_in_use(buf) : NULL; }

int unlock_level_get(const ConfigDat *c)
{
    Edition buf;
    if (own_edition(&buf) != NULL) return UNLOCK_LEVEL_EDITION;
    if (unlock_level_own()) return port_extras_load().unlock_level;
    return config_dat_unlock_level(c);
}

PortExtras unlock_level_gate(PortExtras e, int level, const Edition *edition)
{
    if (edition != NULL) e.lunar_springs = edition_lunar_springs(edition);   /* dep59 Lunar: its tracks=, whatever the switch */
    else if (level < 2) e.lunar_springs = 0;
    return e;
}

PortExtras unlock_level_extras(const ConfigDat *c)
{
    Edition buf;
    return unlock_level_gate(port_extras_load(), unlock_level_get(c), own_edition(&buf));
}

void unlock_level_put(ConfigDat *c, int level)
{
    Edition buf;
    if (own_edition(&buf) != NULL) return;   /* an edition's level is its own, 2, and is never written */
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
