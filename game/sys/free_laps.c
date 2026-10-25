#include "game/sys/free_laps.h"

#include "game/sys/port_settings.h"

#include <stdlib.h>

static int running;

int free_laps_is_exe(int laps)
{
    return laps == 2 || laps == 4 || laps == 6 || laps == 8 || laps == 12 || laps == 16;
}

int free_laps_choice(int track, int laps)
{
    static const int SHORT_TRACKS[3] = { 2, 6, 12 }, LONG_TRACKS[3] = { 4, 8, 16 };
    enum { FIRST_CIRCUIT = 3, FIRST_SHORT = 4, LAST_SHORT = 7, LAST_TRACK = 8 };
    if (track < FIRST_CIRCUIT || track > LAST_TRACK) return -1;
    const int *const counts = track >= FIRST_SHORT && track <= LAST_SHORT ? SHORT_TRACKS : LONG_TRACKS;
    for (int choice = 0; choice < 3; choice++)
        if (counts[choice] == laps) return choice;
    return -1;
}

int port_laps(void)
{
    const char *v = getenv("TNFS_LAPS");
    char *end;
    if (v != NULL && v[0] != '\0') {
        const long n = strtol(v, &end, 10);
        if (*end == '\0' && n >= 0 && n <= FREE_LAPS_MAX) return (int)n;
    }
    return port_settings_load().laps;
}

void free_laps_set(int laps) { running = laps > 0 && laps <= FREE_LAPS_MAX ? laps : 0; }
int free_laps_count(void) { return running; }
int free_laps_or(int exe_laps) { return running != 0 ? running : exe_laps; }
