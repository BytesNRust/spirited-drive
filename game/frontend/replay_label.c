/* port (dep59 step 3): what the replay list says of a Replay.rpx (replay_label.h) */
#include "game/frontend/replay_label.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

void replay_label(const char *name, ReplayCars status, const char *id, char *out, size_t cap)
{
    if (cap == 0) return;
    const char *const dot = strchr(name, '.');
    const int n = (int)(dot != NULL ? (size_t)(dot - name) : strlen(name));
    switch (status) {
    case REPLAY_CARS_OK: snprintf(out, cap, "%.*s", n, name); break;
    case REPLAY_CARS_MISSING: snprintf(out, cap, "%.*s - needs %s", n, name, id); break;
    case REPLAY_CARS_CHANGED: snprintf(out, cap, "%.*s - %s changed", n, name, id); break;
    case REPLAY_CARS_BAD: snprintf(out, cap, "%.*s - not a replay", n, name); break;
    }
}

int replay_label_plays(ReplayCars status) { return status == REPLAY_CARS_OK; }

int replay_label_added(const char *name)
{
    const size_t n = strlen(name);
    return n > 4 && strcasecmp(name + n - 4, ".RPX") == 0;
}
