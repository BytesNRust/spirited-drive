/* dep56, Restomod's variants as switches inside one track: Control Central's arrows step the tracks alone and keep the switches
 * (race_settings_track_step), where Stock and Restored step base -> mirror -> rally (race_settings_track_forward, unchanged); and
 * Restomod's track card's words (card_words_of), dep64. */
#include "game/frontend/card_marks.h"
#include "game/frontend/race_settings.h"
#include "test.h"

#include <stdlib.h>
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
    CardWords w = card_words_of(3, 1, 1, 1, 2, 3, 8);   /* Rusty Springs, every variant, rain, night */
    CHECK_EQ_INT(w.left_count, 3);
    CHECK(strcmp(w.left[0], "MIRROR") == 0 && strcmp(w.left[1], "REVERSE") == 0 && strcmp(w.left[2], "RALLY") == 0);
    CHECK_EQ_INT(w.right_count, 3);
    CHECK(strcmp(w.right[0], "8 LAPS") == 0 && strcmp(w.right[1], "NIGHT") == 0 && strcmp(w.right[2], "RAIN") == 0);
    w = card_words_of(4, 0, 0, 0, 0, 0, 4);   /* none, clear, midday: the laps alone */
    CHECK_EQ_INT(w.left_count, 0);
    CHECK_EQ_INT(w.right_count, 1);
    CHECK(strcmp(w.right[0], "4 LAPS") == 0);
    w = card_words_of(5, 0, 1, 0, 1, 1, 1);
    CHECK(w.left_count == 1 && strcmp(w.left[0], "REVERSE") == 0);
    CHECK(w.right_count == 3 && strcmp(w.right[0], "1 LAP") == 0 && strcmp(w.right[1], "MORNING") == 0 && strcmp(w.right[2], "FOG") == 0);
    w = card_words_of(7, 0, 0, 1, 3, 2, 99);
    CHECK(w.left_count == 1 && strcmp(w.left[0], "RALLY") == 0);
    CHECK(w.right_count == 3 && strcmp(w.right[0], "99 LAPS") == 0 && strcmp(w.right[1], "EVENING") == 0 && strcmp(w.right[2], "STORM") == 0);
    w = card_words_of(1, 1, 0, 0, 3, -1, 0);   /* a stage, no light effects: no laps, no time */
    CHECK(w.left_count == 1 && strcmp(w.left[0], "MIRROR") == 0);
    CHECK(w.right_count == 1 && strcmp(w.right[0], "STORM") == 0);
    w = card_words_of(0, 0, 0, 0, 0, -1, 0);   /* nothing to say */
    CHECK_EQ_INT(w.left_count + w.right_count, 0);
}

/* dep64: a card says what its track will run, not what was picked: what the track does not take is left out */
static void test_the_card_words_are_the_tracks(void)
{
    CardWords w = card_words_of(8, 1, 1, 1, 0, 1, 8);   /* Lost Vegas with the mirror, reverse, rally and the morning picked */
    CHECK(w.left_count == 1 && strcmp(w.left[0], "REVERSE") == 0);
    CHECK(w.right_count == 2 && strcmp(w.right[0], "8 LAPS") == 0 && strcmp(w.right[1], "NIGHT") == 0);
    w = card_words_of(8, 0, 0, 0, 2, -1, 3);   /* no light effects: still its night */
    CHECK(w.right_count == 3 && strcmp(w.right[1], "NIGHT") == 0 && strcmp(w.right[2], "RAIN") == 0);
    w = card_words_of(9, 1, 0, 1, 0, 0, 2);   /* the hidden track: no variants */
    CHECK_EQ_INT(w.left_count, 0);
    w = card_words_of(2, 1, 1, 1, 0, 1, 8);   /* a stage: no reverse, no laps */
    CHECK(w.left_count == 2 && strcmp(w.left[0], "MIRROR") == 0 && strcmp(w.left[1], "RALLY") == 0);
    CHECK(w.right_count == 1 && strcmp(w.right[0], "MORNING") == 0);
}

enum { PIC_W = 2 * CARD_WORD_W + 4, PIC_H = CARD_WORD_Y1 + 3 };

/* a bitmap entry PIC_W x PIC_H at (341, y), its texels from `texel` */
static void picture(uint8_t *shape, int y, uint8_t (*texel)(int i, int j))
{
    memset(shape, 0, 16);
    shape[0] = 0x7b, shape[4] = PIC_W, shape[6] = PIC_H, shape[12] = 341 & 0xff, shape[13] = 341 >> 8, shape[14] = (uint8_t)y;
    for (int j = 0; j < PIC_H; j++)
        for (int i = 0; i < PIC_W; i++) shape[16 + j * PIC_W + i] = texel(i, j);
}

static uint8_t mirror_texel(int i, int j) { return (uint8_t)(i < CARD_WORD_W ? 0xfe : j); }   /* the word's white; a sky per row */
static uint8_t normal_texel(int i, int j) { return (uint8_t)(i * 3 + j * 7); }

static int in_box(int i, int j) { return i <= CARD_WORD_X1 && j >= CARD_WORD_Y0 && j <= CARD_WORD_Y1; }

/* outside the box the mirror picture as it was */
static int changed_outside(const uint8_t *out, const uint8_t *mirror)
{
    int n = memcmp(out, mirror, 16) != 0;
    for (int j = 0; j < PIC_H; j++)
        for (int i = 0; i < PIC_W; i++) n += !in_box(i, j) && out[16 + j * PIC_W + i] != mirror[16 + j * PIC_W + i];
    return n;
}

/* the word takes the normal picture's texels, turned and lined up by the pictures' places on the screen */
static void test_the_mirror_word_takes_the_normal_picture(void)
{
    static uint8_t mirror[16 + PIC_W * PIC_H], normal[16 + PIC_W * PIC_H];
    picture(mirror, 254, mirror_texel);
    picture(normal, 255, normal_texel);   /* a row lower on the screen, as Vertigo Ridge's */
    uint8_t *out = card_word_wiped(mirror, normal);
    CHECK(out != NULL);
    if (out != NULL) {
        int wrong = 0;
        for (int j = CARD_WORD_Y0; j <= CARD_WORD_Y1; j++)
            for (int i = 0; i <= CARD_WORD_X1; i++) wrong += out[16 + j * PIC_W + i] != normal_texel(PIC_W - 1 - i, j - 1);
        CHECK_EQ_INT(wrong, 0);
        CHECK_EQ_INT(changed_outside(out, mirror), 0);
        free(out);
    }
    CHECK_EQ_INT(mirror[16 + CARD_WORD_Y0 * PIC_W], 0xfe);   /* the bank's own entry is left as it was */
}

/* no normal picture that lines up (none, another width): the sky beside the word, as the Night label */
static void test_the_mirror_word_takes_the_sky_without_it(void)
{
    static uint8_t mirror[16 + PIC_W * PIC_H], normal[16 + PIC_W * PIC_H];
    picture(mirror, 255, mirror_texel);
    picture(normal, 255, normal_texel);
    normal[4] = PIC_W - 1;
    const uint8_t *const others[2] = { NULL, normal };
    for (int n = 0; n < 2; n++) {
        uint8_t *out = card_word_wiped(mirror, others[n]);
        CHECK(out != NULL);
        if (out == NULL) continue;
        int white_left = 0;
        for (int j = CARD_WORD_Y0; j <= CARD_WORD_Y1; j++)
            for (int i = 0; i <= CARD_WORD_X1; i++) white_left += out[16 + j * PIC_W + i] != j;
        CHECK_EQ_INT(white_left, 0);
        CHECK_EQ_INT(changed_outside(out, mirror), 0);
        free(out);
    }
    mirror[0] = 0x7d;
    CHECK(card_word_wiped(mirror, NULL) == NULL);
    mirror[0] = 0x7b, mirror[4] = CARD_WORD_W;   /* too narrow to have sky beside the word */
    CHECK(card_word_wiped(mirror, NULL) == NULL);
    CHECK(card_word_wiped(NULL, normal) == NULL);
}

int main(void)
{
    test_the_arrows_step_the_tracks_alone();
    test_the_exe_order_is_kept();
    test_the_card_words();
    test_the_card_words_are_the_tracks();
    test_the_mirror_word_takes_the_normal_picture();
    test_the_mirror_word_takes_the_sky_without_it();
    return TEST_REPORT();
}
