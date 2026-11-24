/* The lost-link banner's text (link_central.h): the original's "Connection Terminated" ("Verbindung abgebrochen"), except in Restomod when the game was left for the host's track
 * (link_sync refused it): then it says so, once. Stock and Restored keep the original's banner (owner 2026-11-23). Kept apart from link_central.c so a test can link it alone. */
#include "game/frontend/link_central.h"

#include "game/sys/port_profile.h"
#include "game/sys/port_settings.h"

#include <string.h>

static int left_for_track;  /* the game was left for it, not yet told */

const char *link_central_lost_text(const char *text, int german)
{
    if (!left_for_track || (strcmp(text, "Connection Terminated") != 0 && strcmp(text, "Verbindung abgebrochen") != 0))
        return text;
    left_for_track = 0;
    if (port_settings_profile() != PROFILE_RESTOMOD) return text;
    return german ? "Strecke des Hosts unbekannt" : "The host's track is not in this version";
}

void link_central_left_forget(void) { left_for_track = 0; }
void link_central_host_track_refused(void) { left_for_track = 1; }
