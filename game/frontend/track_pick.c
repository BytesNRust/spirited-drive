/* port (dep59 Lunar L2): see track_pick.h. */
#include "game/frontend/track_pick.h"
#include "formats/stock_content.h"

enum { RUSTY = 3 };

static int lunar_now;

int track_pick_of(int track, int variant_r, int lunar)
{
    return track == RUSTY && variant_r && lunar ? STOCK_TRACK_LUNAR : track;
}

void track_pick_set(int pick, int *track, int *variant_r, int *lunar)
{
    *lunar = pick == STOCK_TRACK_LUNAR;
    if (*lunar) *variant_r = 1;
    *track = *lunar ? RUSTY : pick;
}

void track_pick_step(RaceSettings *s, int *lunar, const Unlocks *u, const TrackList *tracks, int dir, int switches)
{
    const int listed = tracks != NULL && track_list_has(tracks, STOCK_TRACK_LUNAR);
    if (listed && track_pick_of(s->track, s->variant_r, *lunar) == STOCK_TRACK_LUNAR) {
        s->track = STOCK_TRACK_LUNAR;
        if (!switches) s->variant_r = 0;   /* the entry itself, its rally implied */
    }
    if (switches) race_settings_track_step(s, u, tracks, dir);
    else if (dir > 0) race_settings_track_forward(s, u, tracks);
    else race_settings_track_backward(s, u, tracks);
    if (!switches && s->track == STOCK_TRACK_LUNAR && s->variant_r) {   /* no Lunar Rally entry: the moon is one */
        s->variant_r = 0;
        if (dir > 0) race_settings_next_track(s, u, tracks);   /* on from Lunar Mirror (or Lunar, the mirror locked) */
        else s->variant_m = u->variant_m != 0;   /* back onto it: its last entry */
    }
    track_pick_set(s->track, &s->track, &s->variant_r, lunar);
}

int track_pick_lunar(void) { return lunar_now; }

void track_pick_lunar_put(int lunar) { lunar_now = lunar != 0; }
