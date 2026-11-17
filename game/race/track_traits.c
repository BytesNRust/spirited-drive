/* port: see track_traits.h. */
#include "game/race/track_traits.h"

#include "game/race/race_sim.h"

#include <ctype.h>
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

enum { STEM_CAP = 16 };

static struct {
    char stem[STEM_CAP];   /* lower case; empty: none */
    int pick;
    TrackTraits traits;
} race_added;

static int same_stem(const char *a, const char *b)
{
    for (; *a != '\0' && *b != '\0'; a++, b++)
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
    return *a == '\0' && *b == '\0';
}

void track_traits_race_set(const char *stem, int pick, const TrackPieceFields *fields)
{
    race_added.stem[0] = '\0', race_added.pick = 0;
    if (stem == NULL || fields == NULL || pick < TRACK_PICK_ADDED_FIRST || pick >= TRACK_PICK_ADDED_FIRST + TRACK_PICK_ADDED_MAX) return;
    size_t i = 0;
    for (; stem[i] != '\0' && i + 1 < STEM_CAP; i++) race_added.stem[i] = (char)tolower((unsigned char)stem[i]);
    race_added.stem[i] = '\0';
    race_added.pick = pick, race_added.traits = track_traits_piece(fields);
}

int track_traits_race_added(const char *stem)
{
    return race_added.stem[0] != '\0' && stem != NULL && same_stem(stem, race_added.stem) ? race_added.pick : 0;
}

TrackTraits track_traits_of(int track, int variant)
{
    if (race_added.stem[0] != '\0' && track == race_added.pick) return race_added.traits;
    return track_traits_stock(track, variant);
}

TrackTraits track_traits_of_stem(const char *stem)
{
    if (track_traits_race_added(stem)) return race_added.traits;
    if (stem == NULL || stem[0] == '\0' || stem[1] == '\0') return track_traits_stock(-1, 0);   /* race_sim_track_index reads two letters */
    return track_traits_stock(race_sim_track_index(stem), race_sim_track_variant(stem));
}
