#include "game/sys/reverse_track.h"

#include "game/race/session_rules.h"
#include "game/sys/port_extras.h"

#include <stdlib.h>

static int running, picked;

static int forced, forced_value;

void reverse_track_force(int on, int reverse) { forced = on != 0, forced_value = reverse != 0; }

int reverse_track_wanted(void)
{
    const char *v = getenv("TNFS_REVERSE");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] == '1';
    if (forced) return forced_value;   /* port (replay conditions): a replay's own direction */
    const SessionRules *host = session_rules_active();   /* port (online): a link race is the host's way round */
    if (host != NULL) return host->reverse != 0;
    return port_extras_load().reverse != 0 && picked;
}

void reverse_track_set(int on) { running = on != 0; }
int reverse_track_running(void) { return running; }

void reverse_track_pick(int on) { picked = on != 0; }
int reverse_track_picked(void) { return picked; }
