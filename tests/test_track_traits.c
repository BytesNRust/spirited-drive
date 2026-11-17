/* dep59 step 5, T1: what the race reads from the track index, as traits (game/race/track_traits.h). The factory tracks' traits must
 * give, for every index and variant, exactly what today's expressions give: each check below copies one of them from its site. */
#include "game/race/track_traits.h"
#include "tests/test.h"

/* today's expressions, copied from their sites (T2 replaces each with the trait) */
static int ground_today(int track, int variant)   /* race_state.c:38-53 race_surfaces_init */
{
    if (track == 2 && variant == 2) return TRACK_GROUND_DIRT;
    if (track >= 4) return TRACK_GROUND_GRASS;
    if (track == 3) return TRACK_GROUND_SAND;
    return TRACK_GROUND_VERGE;
}
static int circuit_today(int track) { return track > 2; }      /* car_engine.c:87, ai_setup.c:99, race_splits.c:179, race_results.c:121 */
static int traffic_today(int track) { return track < 3; }      /* race_sim.c:120 race_field_counts */
static int strip_today(int track) { return track == 3; }       /* car_physics.c:269,489, car_load.c:51, race_sim.c:498, race_run.c:62, hud_keys.c:12 */
static int vertigo_today(int track) { return track == 6; }     /* car_physics.c:210 */
static int half_today(int track, int variant) { return track == 0 && variant == 2; }  /* model_draw.h:34 */
static int low_sky_today(int track) { return track == 5; }     /* mirror_sky.c:76 */
static int gravel_today(int track) { return track == 5; }      /* race_sound_engine.c:68,75 */
static int water_today(int track) { return track == 7; }       /* race_sound_engine.c:19,188 */
static int dust_today(int track) { return track == 3; }        /* rally_gravel.c:15 */
static int own_rally_tri_today(int track) { return track == 3; }   /* track_fam.c:12,16,37 */
static int no_time_today(int track) { return track == 8; }     /* track_fam.c:35 */

static void stock_matches_today(void)
{
    for (int track = -1; track <= 10; track++)
        for (int variant = 0; variant < 3; variant++) {
            const TrackTraits t = track_traits_stock(track, variant);
            CHECK_EQ_INT(t.ground, ground_today(track, variant));
            CHECK_EQ_INT(t.kind == TRACK_KIND_CIRCUIT, circuit_today(track));
            CHECK_EQ_INT(t.traffic, traffic_today(track));
            CHECK_EQ_INT(t.drag_strip, strip_today(track));
            CHECK_EQ_INT(t.vertigo_drag, vertigo_today(track));
            CHECK_EQ_INT(t.city_half_side, half_today(track, variant));
            CHECK_EQ_INT(t.mirror_low_sky, low_sky_today(track));
            CHECK_EQ_INT(t.sound_gravel, gravel_today(track));
            CHECK_EQ_INT(t.sound_water, water_today(track));
            CHECK_EQ_INT(t.rally_dust, dust_today(track));
            CHECK_EQ_INT(t.own_rally_tri, own_rally_tri_today(track));
            CHECK_EQ_INT(t.no_time_of_day, no_time_today(track));
        }
}

/* an added track: no factory track's trick, no traffic; its kind, ground and sounds from its mod.ini */
static void piece(void)
{
    const TrackPieceFields stage_grass = { TRACK_KIND_STAGE, TRACK_ROAD_ASPHALT, TRACK_EDGE_GRASS };
    TrackTraits t = track_traits_piece(&stage_grass);
    CHECK_EQ_INT(t.kind, TRACK_KIND_STAGE);
    CHECK_EQ_INT(t.ground, TRACK_GROUND_GRASS);
    CHECK_EQ_INT(t.traffic, 0);
    CHECK_EQ_INT(t.drag_strip + t.vertigo_drag + t.city_half_side + t.mirror_low_sky + t.own_rally_tri + t.no_time_of_day, 0);
    CHECK_EQ_INT(t.sound_gravel + t.sound_water + t.rally_dust, 0);
    static const struct { int edge, ground, gravel, water, dust; } EDGES[] = {
        { TRACK_EDGE_GRASS, TRACK_GROUND_GRASS, 0, 0, 0 },  { TRACK_EDGE_GRAVEL, TRACK_GROUND_VERGE, 1, 0, 0 },
        { TRACK_EDGE_SAND, TRACK_GROUND_SAND, 0, 0, 1 },    { TRACK_EDGE_SNOW, TRACK_GROUND_VERGE, 0, 0, 0 },
        { TRACK_EDGE_DIRT, TRACK_GROUND_DIRT, 0, 0, 0 },    { TRACK_EDGE_WATER, TRACK_GROUND_GRASS, 0, 1, 0 },
    };
    for (size_t i = 0; i < sizeof EDGES / sizeof EDGES[0]; i++) {
        const TrackPieceFields f = { TRACK_KIND_STAGE, TRACK_ROAD_ASPHALT, EDGES[i].edge };
        t = track_traits_piece(&f);
        CHECK_EQ_INT(t.ground, EDGES[i].ground);
        CHECK_EQ_INT(t.sound_gravel, EDGES[i].gravel);
        CHECK_EQ_INT(t.sound_water, EDGES[i].water);
        CHECK_EQ_INT(t.rally_dust, EDGES[i].dust);
    }
    const TrackPieceFields circuit = { TRACK_KIND_CIRCUIT, TRACK_ROAD_DIRT, TRACK_EDGE_GRASS };
    t = track_traits_piece(&circuit);
    CHECK_EQ_INT(t.kind, TRACK_KIND_CIRCUIT);
    CHECK_EQ_INT(t.road, TRACK_ROAD_DIRT);
}

/* the menu's pick to the race: factory 0..9 as they are, Lunar (10) as Rusty Springs in rally on the moon, added ones (16 on) as their own
 * number with their piece's traits; anything else none */
static void picks(void)
{
    TrackRace r;
    for (int pick = 0; pick <= 9; pick++) {
        CHECK(track_pick_race(pick, 1, NULL, &r));
        CHECK_EQ_INT(r.track, pick);
        CHECK_EQ_INT(r.variant_r, 0);
        CHECK_EQ_INT(r.lunar, 0);
        CHECK_EQ_INT(r.traits.ground, ground_today(pick, 1));
    }
    CHECK(track_pick_race(TRACK_PICK_LUNAR, 0, NULL, &r));
    CHECK_EQ_INT(r.track, 3);
    CHECK_EQ_INT(r.variant_r, 1);
    CHECK_EQ_INT(r.lunar, 1);
    CHECK_EQ_INT(r.traits.ground, TRACK_GROUND_SAND);
    CHECK_EQ_INT(r.traits.rally_dust, 1);
    for (int pick = 11; pick < TRACK_PICK_ADDED_FIRST; pick++) CHECK(!track_pick_race(pick, 0, NULL, &r));
    CHECK(!track_pick_race(-1, 0, NULL, &r));
    CHECK(!track_pick_race(TRACK_PICK_ADDED_FIRST, 0, NULL, &r));   /* added, but no piece given */
    const TrackPieceFields sand = { TRACK_KIND_STAGE, TRACK_ROAD_ASPHALT, TRACK_EDGE_SAND };
    CHECK(track_pick_race(TRACK_PICK_ADDED_FIRST + 2, 0, &sand, &r));
    CHECK_EQ_INT(r.track, TRACK_PICK_ADDED_FIRST + 2);
    CHECK_EQ_INT(r.traits.ground, TRACK_GROUND_SAND);
    CHECK_EQ_INT(r.traits.drag_strip, 0);   /* sand like Rusty Springs', none of its tricks */
    CHECK(!track_pick_race(TRACK_PICK_ADDED_FIRST + TRACK_PICK_ADDED_MAX, 0, &sand, &r));
    CHECK(!track_pick_race(5, 0, &sand, &r));   /* a factory pick takes no piece */
}

int main(void)
{
    stock_matches_today();
    piece();
    picks();
    return TEST_REPORT();
}
