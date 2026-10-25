#include "game/sys/free_laps.h"

#include "game/sys/port_settings.h"

#include <stdlib.h>

static int running;

int free_laps_is_exe(int laps)
{
    return laps == 2 || laps == 4 || laps == 6 || laps == 8 || laps == 12 || laps == 16;
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
