#include "game/viewer/spray_v4_switch.h"

#include "game/race/race_sim.h"
#include "game/sys/port_follow.h"
#include "game/sys/port_settings.h"

static PortFollow on;

int spray_v4_switch_on(void) { return port_follow(&on, port_spray_v4) != 0; }

void spray_v4_switch_hook(void (*tick)(const RaceState *race))
{
    race_sim_set_tick_hook(spray_v4_switch_on() ? tick : NULL);
}
