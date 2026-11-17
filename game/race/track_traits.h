/* port (dep59 step 5, T1): what the race reads from the track's number, as traits of the track. The factory tracks' traits are
 * computed from their number and variant and give exactly what the code reads today (tests/test_track_traits.c copies each
 * expression); an added track's come from its mod.ini (kind, road, edge) and have none of a factory track's tricks. T2 makes the
 * sites read the traits instead of comparing the number.
 *
 * The menu's pick and the race's number are not the same thing (as with the cars' CarPick): 0..9 are the factory tracks, 10..15
 * the port's own entries (10 Lunar Springs: Rusty Springs in rally, on the moon), 16 on the added tracks. track_pick_race is the
 * one place a pick becomes the race's number, variant and traits. */
#ifndef TNFS_TRACK_TRAITS_H
#define TNFS_TRACK_TRAITS_H

#include "core/formats/track_piece.h"   /* TrackKind, TrackRoad, TrackEdge: the piece's card (T3) */

enum {
    TRACK_PICK_LUNAR = 10,          /* the Lunar Springs entry (dep59-lunar-entry-plan.md's TRACK_LUNAR) */
    TRACK_PICK_ADDED_FIRST = 16,    /* the first added track (as car_registry.h's CAR_ADDED_FIRST) */
    TRACK_PICK_ADDED_MAX = 64,
    TRACK_RUSTY_SPRINGS = 3
};

/* the three surfaces' set race_surfaces_init takes (race_state.c): the road is the same in all of them, the sets differ in the verges */
typedef enum {
    TRACK_GROUND_VERGE = 0,   /* the stage tracks' (City, Coastal, Alpine but its third stage): gravel and snow verges */
    TRACK_GROUND_DIRT,        /* Alpine's third stage: the verges dirt */
    TRACK_GROUND_SAND,        /* Rusty Springs: the verges sand (rolling drag) */
    TRACK_GROUND_GRASS        /* the circuits from Autumn Woods on: grass */
} TrackGround;

typedef struct {
    TrackKind kind;          /* car_engine.c:87, ai_setup.c:99, race_splits.c:179, race_results.c:121 (the index above 2) */
    TrackGround ground;      /* race_state.c:38-53 */
    TrackRoad road;          /* an added track's road (the original has one road surface: it changes nothing yet) */
    int traffic;             /* race_sim.c:120: police and traffic in a head-to-head (the stage tracks) */
    int drag_strip;          /* Rusty Springs' strip and its time trial: car_physics.c:269,489, car_load.c:51, race_sim.c:498, race_run.c:62, hud_keys.c:12 */
    int vertigo_drag;        /* car_physics.c:210: Vertigo Ridge's drag rule */
    int city_half_side;      /* model_draw.h:34: City's third stage's half-side models */
    int mirror_low_sky;      /* mirror_sky.c:76: Burnt Sienna's mirror sky */
    int sound_gravel;        /* race_sound_engine.c:68,75: the gravel and rumble of Burnt Sienna */
    int sound_water;         /* race_sound_engine.c:19,188: Transtropolis' water and its crash bank */
    int rally_dust;          /* rally_gravel.c:15: the rally's dust (regolith on the moon) rather than compact gravel */
    int own_rally_tri;       /* track_fam.c:12,16,37: the rally is another TRI, not an _R FAM; no time of day in rally */
    int no_time_of_day;      /* track_fam.c:35: Lost Vegas offers no time of day */
} TrackTraits;

/* an added track's own fields (mod.ini: kind=, road=, edge=; TrackPieceSpec's) */
typedef struct {
    TrackKind kind;
    TrackRoad road;
    TrackEdge edge;
} TrackPieceFields;

/* what the menu's pick races as */
typedef struct {
    int track;               /* the race's number: the factory's 0..9, an added track's pick (16 on) */
    int variant_r;           /* 1: the rally forced (Lunar Springs) */
    int lunar;               /* 1: on the moon (Lunar Springs) */
    TrackTraits traits;
} TrackRace;

/* a factory track's traits, from its number and variant (any number: what today's expressions give for it) */
TrackTraits track_traits_stock(int track, int variant);
/* an added track's traits: its kind, road and edge; no traffic, none of a factory track's tricks */
TrackTraits track_traits_piece(const TrackPieceFields *fields);
/* the race for the menu's pick (VARIANT the stage, 0..2): 1 and *out, or 0 for a pick that is none (11..15, past the added ones,
 * an added one without its PIECE, a factory one with one) */
int track_pick_race(int pick, int variant, const TrackPieceFields *piece, TrackRace *out);

/* the race's added track, as car_registry's race_set: STEM races as the added PICK with FIELDS' traits (T4, before the race on it);
 * a NULL stem clears it. Stems are compared without case. */
void track_traits_race_set(const char *stem, int pick, const TrackPieceFields *fields);
/* the pick STEM races as when it is the race's added track; else 0 */
int track_traits_race_added(const char *stem);
/* what the race's sites read: the race's added track's traits for its pick, else a factory number's (track_traits_stock) */
TrackTraits track_traits_of(int track, int variant);
/* the same by the stem: the race's added track's, else the factory track's the stem names (race_sim_track_index and
 * race_sim_track_variant; an unknown stem as today's -1) */
TrackTraits track_traits_of_stem(const char *stem);

#endif
