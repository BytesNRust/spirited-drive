/* See track_list.h. */
#include "game/frontend/track_list.h"
#include "formats/stock_content.h"
#include "game/sys/port_log.h"
#include "game/sys/track_registry.h"

enum { STANDARD_TRACKS = 8, RUSTY = 3, LOST_VEGAS = 8, HIDDEN = 9 };

void track_list_build(TrackList *out, const Edition *e, int lost_vegas, int lunar)
{
    out->count = 0;
    if (e == NULL) {
        for (int16_t track = 0; track < STANDARD_TRACKS; track++) {
            out->track[out->count++] = track;
            if (track == RUSTY && lunar == TRACK_LIST_LUNAR_ON) out->track[out->count++] = STOCK_TRACK_LUNAR;
        }
        if (lost_vegas) out->track[out->count++] = LOST_VEGAS;
        return;
    }
    for (int i = 0; i < e->tracks && out->count < TRACK_LIST_MAX; i++) {
        const int track = stock_track_of_id(e->track[i]);
        if (track < 0) {   /* port (dep59 step 5, T7b): a registered added track, by its id (16..79) */
            const int added = track_registry_find(e->track[i]);
            if (added >= 0) out->track[out->count++] = (int16_t)added;
        } else if (track == HIDDEN) port_log("port (dep59): edition: track hidden left out: the cut moon track has no geometry on the CD\n");
        else if (!(track == STOCK_TRACK_LUNAR && lunar == TRACK_LIST_LUNAR_NEVER)) out->track[out->count++] = (int16_t)track;
    }
}

int16_t track_list_step(const TrackList *l, int16_t track, int step)
{
    if (l->count == 0) return 0;
    for (int i = 0; i < l->count; i++)
        if (l->track[i] == track) return l->track[(i + (step < 0 ? l->count - 1 : 1)) % l->count];
    return l->track[0];
}

int track_list_has(const TrackList *l, int16_t track)
{
    for (int i = 0; i < l->count; i++)
        if (l->track[i] == track) return 1;
    return 0;
}
