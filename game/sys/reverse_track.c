#include "game/sys/reverse_track.h"

#include "game/sys/port_extras.h"

#include <stdlib.h>

static int running;

int reverse_track_wanted(void)
{
    const char *v = getenv("TNFS_REVERSE");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] == '1';
    return port_extras_load().reverse != 0;
}

void reverse_track_set(int on) { running = on != 0; }
int reverse_track_running(void) { return running; }
