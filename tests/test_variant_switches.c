/* dep56, Restomod's variants as switches inside one track: Control Central's arrows step the tracks alone and keep the switches
 * (race_settings_track_step), where Stock and Restored step base -> mirror -> rally (race_settings_track_forward, unchanged); and
 * the words along the top of the track card (card_marks_text). */
#include "game/frontend/card_marks.h"
#include "game/frontend/race_settings.h"
#include "test.h"

#include <string.h>

static RaceSettings at(int track, int m, int r)
{
    RaceSettings s;
    memset(&s, 0, sizeof s);
    s.track = track, s.variant_m = m, s.variant_r = r;
    return s;
}

static void test_the_arrows_step_the_tracks_alone(void)
{
    const Unlocks u = { .variant_m = 1, .variant_r = 1, .warrior = 1, .lost_vegas = 1 };
    RaceSettings s = at(3, 1, 1);
    race_settings_track_step(&s, &u, 1);
    CHECK_EQ_INT(s.track, 4);
    CHECK_EQ_INT(s.variant_m, 1);
    CHECK_EQ_INT(s.variant_r, 1);
    race_settings_track_step(&s, &u, -1);
    race_settings_track_step(&s, &u, -1);
    CHECK_EQ_INT(s.track, 2);
    CHECK_EQ_INT(s.variant_m + s.variant_r, 2);
    s = at(7, 1, 0);
    race_settings_track_step(&s, &u, 1);   /* onto Lost Vegas: it has neither */
    CHECK_EQ_INT(s.track, TRACK_LOST_VEGAS);
    CHECK_EQ_INT(s.variant_m + s.variant_r, 0);
    race_settings_track_step(&s, &u, 1);   /* the wrap, as 00432cd4 */
    CHECK_EQ_INT(s.track, 0);
    race_settings_track_step(&s, &u, -1);
    CHECK_EQ_INT(s.track, TRACK_LOST_VEGAS);
    const Unlocks level1 = { .variant_m = 1, .warrior = 1, .lost_vegas = 1 };
    s = at(3, 1, 1);   /* a rally left on with no unlock (a lower level): not kept */
    race_settings_track_step(&s, &level1, 1);
    CHECK_EQ_INT(s.variant_m, 1);
    CHECK_EQ_INT(s.variant_r, 0);
    const Unlocks none = { 0 };
    s = at(7, 0, 0);
    race_settings_track_step(&s, &none, 1);   /* no Lost Vegas: 7 wraps to 0 */
    CHECK_EQ_INT(s.track, 0);
}

/* Stock and Restored keep the exe's order: base, mirror, rally, the next track */
static void test_the_exe_order_is_kept(void)
{
    const Unlocks u = { .variant_m = 1, .variant_r = 1, .warrior = 1, .lost_vegas = 1 };
    RaceSettings s = at(3, 0, 0);
    race_settings_track_forward(&s, &u);
    CHECK(s.track == 3 && s.variant_m == 1 && s.variant_r == 0);
    race_settings_track_forward(&s, &u);
    CHECK(s.track == 3 && s.variant_m == 0 && s.variant_r == 1);
    race_settings_track_forward(&s, &u);
    CHECK(s.track == 4 && s.variant_m == 0 && s.variant_r == 0);
}

static void test_the_card_words(void)
{
    char out[48];
    card_marks_text(1, 1, 1, 2, 2, 8, out, sizeof out);
    CHECK(strcmp(out, "MIR  RAIN DUSK 8 LAPS") == 0);
    card_marks_text(0, 0, 0, 0, 0, 4, out, sizeof out);
    CHECK(strcmp(out, "CLR DAY 4 LAPS") == 0);
    card_marks_text(1, 0, 0, 3, -1, 0, out, sizeof out);   /* a stage, no light effects */
    CHECK(strcmp(out, "M  STRM") == 0);
    card_marks_text(0, 1, 0, 1, 3, 1, out, sizeof out);
    CHECK(strcmp(out, "I  FOG NGT 1 LAP") == 0);
    card_marks_text(1, 1, 1, 2, 2, 99, out, 6);   /* cut to its room, NUL-ended */
    CHECK_EQ_INT((int)strlen(out), 5);
}

int main(void)
{
    test_the_arrows_step_the_tracks_alone();
    test_the_exe_order_is_kept();
    test_the_card_words();
    return TEST_REPORT();
}
