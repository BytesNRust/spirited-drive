/* port: see track_traits.h. */
#include "game/race/track_traits.h"

#include <stddef.h>

enum { VERTIGO_RIDGE = 6, BURNT_SIENNA = 5, TRANSTROPOLIS = 7, LOST_VEGAS = 8, LAST_STAGE = 2, ALPINE = 2, CITY = 0, THIRD_STAGE = 2 };

static TrackGround stock_ground(int track, int variant)
{
    if (track == ALPINE && variant == THIRD_STAGE) return TRACK_GROUND_DIRT;
    if (track >= TRACK_RUSTY_SPRINGS + 1) return TRACK_GROUND_GRASS;
    if (track == TRACK_RUSTY_SPRINGS) return TRACK_GROUND_SAND;
    return TRACK_GROUND_VERGE;
}

TrackTraits track_traits_stock(int track, int variant)
{
    TrackTraits t = { 0 };
    t.kind = track > LAST_STAGE ? TRACK_KIND_CIRCUIT : TRACK_KIND_STAGE;
    t.ground = stock_ground(track, variant);
    t.road = TRACK_ROAD_ASPHALT;
    t.traffic = track <= LAST_STAGE;
    t.drag_strip = t.rally_dust = t.own_rally_tri = track == TRACK_RUSTY_SPRINGS;
    t.vertigo_drag = track == VERTIGO_RIDGE;
    t.city_half_side = track == CITY && variant == THIRD_STAGE;
    t.mirror_low_sky = t.sound_gravel = track == BURNT_SIENNA;
    t.sound_water = track == TRANSTROPOLIS;
    t.no_time_of_day = track == LOST_VEGAS;
    return t;
}

/* an edge's verges and the sounds and dust that go with them, among the values the factory tracks use */
static TrackGround edge_ground(TrackEdge edge)
{
    switch (edge) {
    case TRACK_EDGE_GRAVEL:
    case TRACK_EDGE_SNOW: return TRACK_GROUND_VERGE;
    case TRACK_EDGE_SAND: return TRACK_GROUND_SAND;
    case TRACK_EDGE_DIRT: return TRACK_GROUND_DIRT;
    case TRACK_EDGE_GRASS:
    case TRACK_EDGE_WATER:
    default: return TRACK_GROUND_GRASS;
    }
}

TrackTraits track_traits_piece(const TrackPieceFields *fields)
{
    TrackTraits t = { 0 };
    t.kind = fields->kind;
    t.road = fields->road;
    t.ground = edge_ground(fields->edge);
    t.sound_gravel = fields->edge == TRACK_EDGE_GRAVEL;
    t.sound_water = fields->edge == TRACK_EDGE_WATER;
    t.rally_dust = fields->edge == TRACK_EDGE_SAND;
    return t;
}

int track_pick_race(int pick, int variant, const TrackPieceFields *piece, TrackRace *out)
{
    TrackRace r = { 0 };
    if (pick >= 0 && pick <= TRACK_PICK_LUNAR - 1 && piece == NULL) {
        r.track = pick, r.traits = track_traits_stock(pick, variant);
    } else if (pick == TRACK_PICK_LUNAR && piece == NULL) {
        r.track = TRACK_RUSTY_SPRINGS, r.variant_r = 1, r.lunar = 1, r.traits = track_traits_stock(TRACK_RUSTY_SPRINGS, variant);
    } else if (pick >= TRACK_PICK_ADDED_FIRST && pick < TRACK_PICK_ADDED_FIRST + TRACK_PICK_ADDED_MAX && piece != NULL) {
        r.track = pick, r.traits = track_traits_piece(piece);
    } else {
        return 0;
    }
    *out = r;
    return 1;
}
