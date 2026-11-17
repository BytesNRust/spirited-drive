#include "game/frontend/link_rules.h"

#include "game/race/race_net.h"
#include "game/race/race_state.h"
#include "game/sys/extras_marks.h"
#include "game/sys/port_extras.h"
#include "game/sys/port_settings.h"
#include "game/sys/reverse_track.h"
#include "game/viewer/drive_session.h"
#include "game/frontend/config.h"
#include "game/frontend/track_pick.h"
#include "game/frontend/unlock_level.h"

#include <string.h>

uint8_t link_rules_paint_pick(int car)
{
    const PortExtras e = port_extras_load();
    if (car < 0 || car >= CAR_PAINT_SLOTS) return 0;   /* sent whatever this machine's own switch says: the host decides whether the session has colours */
    return (uint8_t)(e.car_paint[car] + 1);
}

static uint8_t clamp(int v, int top) { return (uint8_t)(v < 0 ? 0 : v > top ? top : v); }

void link_rules_compose(const uint8_t *records, int humans, SessionRules *out)
{
    const PortExtras e = unlock_level_extras(config_current());   /* the host's level gates its Lunar Springs Track */
    memset(out, 0xff, sizeof *out);   /* the paints not asked for: the race's own draw */
    out->mode = clamp(port_settings_profile(), SESSION_RESTOMOD);
    out->difficulty = clamp(e.difficulty, 2);
    out->weather = clamp(port_weather(), 3);
    out->reverse = reverse_track_wanted() != 0;
    out->lunar_springs = track_pick_lunar() != 0,   /* dep59 Lunar: the host's entry, Lunar or Rusty's rally */ out->lunar_gravity = clamp(e.lunar_gravity, LUNAR_GRAVITY_ALL), out->rally_gravel = e.rally_gravel != 0;
    out->cheats = (uint8_t)(port_extras_cheat_marks(&e) & 7);
    out->rain_grip = port_rain_grip() != 0;
    out->snow_weather = port_snow_weather() != 0;
    out->liveries = e.extra_liveries != 0;
    out->time_of_day = clamp(drive_menus_on ? drive_menus.time_of_day : 1, 2);
    out->light_time = clamp(port_light_time(), 2);
    out->variants = 1;   /* port (dep56): the host's variants, as its race runs them */
    out->mirror = drive_menus_on && drive_menus.variant_m != 0, out->rally = drive_menus_on && drive_menus.variant_r != 0;
    for (int i = 0; i < humans && i < SESSION_SLOTS; i++) {
        const uint8_t pick = records[i * RACE_NET_INFO + 2 + offsetof(RaceEntry, paint_pick)];
        out->paint[i] = out->liveries && pick != 0 ? (uint8_t)(pick - 1) : 0xff;
    }
}
