/* Mirror and Rally at the same time (port): with both switches on, the race runs the mirror's geometry and the
 * rally's art and track — the FAM "_R01", TR5 in place of TR1 — while the exe keeps Mirror winning whenever the
 * two are combined. Each rule is behind its own port_* flag (0 = the exe, 1 = the port), set per call-site, so the
 * original's answers stay reachable: the cheats in central_keys.c still exclude one from the other, and the
 * records and the track select's entries still follow the original. */
#include "game/frontend/variant_rule.h"
#include "game/race/track_fam.h"
#include "game/sys/port_extras.h"
#include "test.h"

#include "game/platform/platform.h"

#include <string.h>

/* the platform's files: none here (every case passes the combine flag explicitly) */
uint8_t *platform_read_user_file(const char *name, size_t *size) { (void)name, (void)size; return NULL; }
bool platform_write_user_file(const char *name, const uint8_t *data, size_t size)
{
    (void)name, (void)data, (void)size;
    return false;
}
void platform_free_file(uint8_t *data) { (void)data; }

/* ----- track_fam.c: 004510e8's name ----- */

/* 004510e8 as the exe: midday with "m" is the mirror's FAM, whichever else is set. */
static void fam_mirror_wins_as_the_exe(void)
{
    const TrackFamInputs in = { .time_of_day = 1, .variant_m = 1, .variant_r = 1, .track = 1,
                                .stem = "cl", .dir = "n", .dir_midday = "e" };
    char name[TRACK_FAM_NAME];
    track_fam_name(&in, 1, name, sizeof name);
    CHECK(strstr(name, "_M01.fam") != NULL);
}

/* the port: both on, midday — the rally's art, not the mirror's. */
static void fam_rally_wins_when_the_port_says_so(void)
{
    const TrackFamInputs in = { .time_of_day = 1, .variant_m = 1, .variant_r = 1, .track = 1, .port_both_variants = 1,
                                .stem = "cl", .dir = "n", .dir_midday = "e" };
    char name[TRACK_FAM_NAME];
    track_fam_name(&in, 1, name, sizeof name);
    CHECK(strstr(name, "_R01.fam") != NULL);
}

/* The flag only speaks when the two are combined: alone, each keeps the exe's own answer. */
static void fam_alone_is_the_exe_with_the_flag_on(void)
{
    const TrackFamInputs m = { .time_of_day = 1, .variant_m = 1, .variant_r = 0, .track = 1, .port_both_variants = 1,
                               .stem = "cl", .dir = "n", .dir_midday = "e" };
    const TrackFamInputs r = { .time_of_day = 1, .variant_m = 0, .variant_r = 1, .track = 1, .port_both_variants = 1,
                               .stem = "cl", .dir = "n", .dir_midday = "e" };
    char name[TRACK_FAM_NAME];
    track_fam_name(&m, 1, name, sizeof name);
    CHECK(strstr(name, "_M01.fam") != NULL);
    track_fam_name(&r, 1, name, sizeof name);
    CHECK(strstr(name, "_R01.fam") != NULL);
}

/* A time of day other than midday is the dawn and dusk art, whatever the variants say. */
static void fam_other_time_keeps_the_t_art(void)
{
    const TrackFamInputs in = { .time_of_day = 0, .variant_m = 1, .variant_r = 1, .track = 1, .port_both_variants = 1,
                                .stem = "cl", .dir = "n", .dir_midday = "e" };
    char name[TRACK_FAM_NAME];
    track_fam_name(&in, 1, name, sizeof name);
    CHECK(strstr(name, "_T01.fam") != NULL);
}

/* Track 3 (Rusty Springs) holds no "r" art, so the rally's branch is skipped for it (004510e8). With the mirror on
 * the name is its "_M" — the exe's answer — and with the rally alone the midday plain: the port's rule keeps both,
 * because there is no TR1_R01 to reach for. */
static void fam_track_three_has_no_rally_art(void)
{
    char name[TRACK_FAM_NAME];
    const TrackFamInputs both = { .time_of_day = 1, .variant_m = 1, .variant_r = 1, .track = 3,
                                  .port_both_variants = 1, .stem = "tr1", .dir = "n", .dir_midday = "e" };
    track_fam_name(&both, 1, name, sizeof name);
    CHECK(strstr(name, "_M01.fam") != NULL);

    TrackFamInputs rally = both;
    rally.variant_m = 0, rally.port_both_variants = 0;
    track_fam_name(&rally, 1, name, sizeof name);
    CHECK(strstr(name, "_001.fam") != NULL);
}

/* ----- central_draw.c / central_glue.c: the thumbnail and the TR1 -> TR5 remap ----- */

/* Which of the two variants' thumbnail banks the track art takes: the exe gives "m" 10 and "r" 19, and 10 when
 * both are set. The port's rule gives rally (19) when the two are combined. */
static void thumbnail_base_prefers_rally_when_combined(void)
{
    CHECK_EQ_INT(central_track_art_base(1, 0, 0), 10);    /* mirror */
    CHECK_EQ_INT(central_track_art_base(0, 1, 0), 19);    /* rally */
    CHECK_EQ_INT(central_track_art_base(0, 0, 0), 0);     /* neither */
    CHECK_EQ_INT(central_track_art_base(1, 1, 0), 10);    /* the exe: mirror wins */
    CHECK_EQ_INT(central_track_art_base(1, 1, 1), 19);    /* the port: rally */
}

/* The stem a rally race runs on Rusty Springs (track 3): "tr5" in place of "tr1". */
static void rally_on_tr1_runs_tr5(void)
{
    const char *const tr1 = "TR1";
    CHECK(strcmp(rally_track_stem(tr1, 0, 0, 0), tr1) == 0);
    CHECK(strcmp(rally_track_stem(tr1, 1, 0, 0), tr1) == 0);      /* mirror alone: no remap */
    CHECK(strcmp(rally_track_stem(tr1, 0, 1, 0), "TR5") == 0);    /* rally alone */
    CHECK(strcmp(rally_track_stem(tr1, 1, 1, 0), "TR1") == 0);    /* the exe: mirror wins, no remap */
    CHECK(strcmp(rally_track_stem(tr1, 1, 1, 1), "TR5") == 0);    /* the port: rally's own track */
}

/* The CD's files are not read by this test (the tables start as the built-in defaults). */
uint8_t *platform_read_file(const char *path, size_t *size) { (void)path; (void)size; return NULL; }

int main(void)
{
    /* ----- track_fam.c ----- */
    fam_mirror_wins_as_the_exe();
    fam_rally_wins_when_the_port_says_so();
    fam_alone_is_the_exe_with_the_flag_on();
    fam_other_time_keeps_the_t_art();
    fam_track_three_has_no_rally_art();
    /* ----- central_draw.c / central_glue.c ----- */
    thumbnail_base_prefers_rally_when_combined();
    rally_on_tr1_runs_tr5();
    return TEST_REPORT();
}
