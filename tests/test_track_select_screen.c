/* The track screen's weather row and console time, driven through track_select_screen with ops that only feed keys:
 * Up from Done reaches the weather, Enter cycles it, Escape puts it back, Done keeps it. */
#include "game/frontend/track_select.h"
#include "game/sys/palette.h"
#include "game/sys/track_registry.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

enum { UP = TRACK_SELECT_KEY_UP, DOWN = TRACK_SELECT_KEY_DOWN, LEFT = TRACK_SELECT_KEY_LEFT, RIGHT = TRACK_SELECT_KEY_RIGHT, ENTER = 0xd, ESCAPE = 0x1b };

typedef struct Script {
    const uint16_t *keys;
    int next, count;
} Script;

static void nothing(void *ctx) { (void)ctx; }
static int32_t pointer_x, pointer_y;   /* where the screen last put the pointer: the keys' moves are seen through it */
static void move_to(void *ctx, int32_t x, int32_t y) { (void)ctx; pointer_x = x, pointer_y = y; }
static char bank_path[64], music_name[64];   /* dep59 Lunar L5: the last bank and line the screen asked for */
static int32_t load_bank(void *ctx, const char *path, int32_t flags) { (void)ctx, (void)flags, snprintf(bank_path, sizeof bank_path, "%s", path); return 1; }
static int32_t load_inverse(void *ctx, const char *name) { (void)ctx, (void)name; return 1; }
static uint8_t picture[0x400];   /* the bank's !pal and every shape: black */
/* the evening art of the time-of-day item: 4 x 1 texels, the last masked blit of a shape of that size is kept */
static uint8_t evening[0x10 + 4] = { 0x40, 0, 0, 0, 4, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 20, 0xff, 30 };
static uint8_t evening_seen[4];
static int evening_blits;
static const uint8_t *shape(void *ctx, int32_t bank, const char *tag)
{
    (void)ctx, (void)bank;
    return strcmp(tag, "evel") == 0 || strcmp(tag, "eved") == 0 ? evening : picture;
}
static void palette_set(void *ctx, int32_t first, int32_t count, const uint8_t *rgb)
{
    (void)ctx, (void)first, (void)count, (void)rgb;
}
static int32_t in_rect(void *ctx, int32_t x0, int32_t x1, int32_t y0, int32_t y1)
{
    (void)ctx;
    return pointer_x >= x0 && pointer_x < x1 && pointer_y >= y0 && pointer_y < y1;
}
static void select_picture(void *ctx, int32_t picture) { (void)ctx, (void)picture; }
static void blit(void *ctx, const uint8_t *s) { (void)ctx, (void)s; }
static void blit_masked(void *ctx, const uint8_t *s)
{
    (void)ctx;
    if (s[4] != 4 || s[6] != 1) return;   /* not the evening art */
    memcpy(evening_seen, s + 0x10, 4);
    evening_blits++;
}
static void blit_word(void *ctx, int32_t s) { (void)ctx, (void)s; }
static void style(void *ctx, int32_t s) { (void)ctx, (void)s; }
static int night_words;   /* the times the word Night was written */
static void text(void *ctx, const char *t, int32_t x, int32_t y)
{
    (void)ctx, (void)x, (void)y;
    if (strcmp(t, "Night") == 0) night_words++;
}
static void mark(void *ctx, DialogPointer *p) { (void)ctx, (void)p; }
static int32_t zero(void *ctx) { (void)ctx; return 0; }
static void music_start(void *ctx, const char *name) { (void)ctx, snprintf(music_name, sizeof music_name, "%s", name); }
static void fade_in(void *ctx, int32_t first, int32_t count, const uint8_t *rgb)
{
    (void)ctx, (void)first, (void)count, (void)rgb;
}
static void delay(void *ctx, int32_t time) { (void)ctx, (void)time; }
static int32_t ticks(void *ctx) { (void)ctx; return 0x30; }   /* a key is armed again at once */
static void sound(void *ctx, int32_t s) { (void)ctx, (void)s; }
static void fade_out(void *ctx, int32_t first, int32_t count) { (void)ctx, (void)first, (void)count; }
static void free_bank(void *ctx, int32_t bank) { (void)ctx, (void)bank; }

/* the keys in turn, each followed by a no-key poll (the screen takes one key per release); Escape when they run out */
static int32_t key(void *ctx)
{
    Script *s = ctx;
    if (s->next >= 2 * s->count) return ESCAPE;
    const int i = s->next++;
    return i % 2 ? 0 : s->keys[i / 2];
}

static int *custom_laps;   /* dep55: Restomod's laps row (TrackSelectPlus.laps); NULL the exe's */
/* dep56: Restomod's switch rows (TrackSelectPlus.variant_switches) with both variants unlocked, and the variants after the run */
static int switches, locked;   /* locked: a tournament past its first race (TrackSelectPlus.variants_locked) */
static int32_t variant_m_in, variant_r_in, variant_m_out, variant_r_out;
static int kept_m_in, kept_r_in, kept_m_out, kept_r_out;   /* dep65: the choice kept through Lost Vegas (TrackSelectPlus.kept_m) */
static int track_out;
static int *lunar_in, lunar_list_in;   /* dep59 Lunar L5: TrackSelectPlus.lunar and lunar_list; NULL, none */
static const Edition *edition_in;   /* port (dep59 step 4): the edition the screen walks, NULL the standard one */

/* the screen on track 3 (not a tournament), the keys, and what the weather and the console version time are afterwards */
static void run_on(const uint16_t *keys, int count, int *weather, int *light_time, int use_light_time, int *reverse, int track)
{
    uint8_t config[TRACK_SELECT_CONFIG] = { 0 }, palette[0x300] = { 0 };
    int32_t time_of_day[9] = { 1, 1, 1, 1, 1, 1, 1, 1, 1 }, variant_m = variant_m_in, variant_r = variant_r_in;
    config[TRACK_SELECT_TRACK] = (uint8_t)track;
    config[TRACK_SELECT_LAPS] = 8;
    const TrackSelectInputs in = { "", 0, 0, 0, 1, 1, 1, edition_in };   /* Lost Vegas on (the standard edition's; an edition lists its own) */
    TrackSelectPlus plus = { switches, switches, &variant_m, &variant_r, 0, 1, weather, use_light_time ? light_time : NULL, reverse, custom_laps,
                             switches, locked, kept_m_in, kept_r_in, lunar_in, lunar_list_in };
    TrackSelectState st = { config, time_of_day, 0, palette, &plus };
    DialogPointer p = { 0, 0, 0, 0, 0, 0 };
    Script script = { keys, 0, count };
    const TrackSelectOps ops = {
        .flush = nothing, .pointer_move_to = move_to, .load_bank = load_bank, .load_inverse = load_inverse,
        .music_feed = nothing, .shape = shape, .palette_set = palette_set, .inverse_build = nothing,
        .text_styles = nothing, .pointer_poll = nothing, .pointer_hide = nothing, .pointer_in_rect = in_rect,
        .select = select_picture, .select_default = nothing, .blit = blit, .blit_masked = blit_masked, .blit_word = blit_word,
        .text_select = style, .text_shadowed = text, .lock = nothing, .unlock = nothing, .flip = nothing,
        .pointer_show = nothing, .pointer_mark = mark, .pointer_buttons = zero, .music_start = music_start,
        .fade_in = fade_in, .music_play = nothing, .delay = delay, .sound = sound, .ticks = ticks,
        .pointer_activate_if_moved = nothing, .key = key, .pointer_click = nothing, .music_stop = nothing,
        .music_free = nothing, .fade_out = fade_out, .free = free_bank, .free_inverse = nothing, .best_times = zero,
        .car_flags = nothing, .pointer_off = nothing, .music_service = nothing, .deadline_passed = zero,
        .music_fade_out = nothing, .music_end = nothing, .ctx = &script };
    track_select_screen(&in, &st, &p, &ops);
    CHECK_EQ_INT(time_of_day[3], 1);   /* the art stays at midday under the light effects */
    variant_m_out = variant_m, variant_r_out = variant_r, track_out = config[TRACK_SELECT_TRACK];
    kept_m_out = plus.kept_m, kept_r_out = plus.kept_r;
}

static void run(const uint16_t *keys, int count, int *weather, int *light_time, int use_light_time)
{
    run_on(keys, count, weather, light_time, use_light_time, NULL, 3);
}

static void test_reverse_row(void)
{
    int weather = 0, light_time = 0, reverse = 0;
    /* Up: Weather; Right: Reverse; Enter: on; Down: Done; Done ends */
    const uint16_t on[] = { UP, RIGHT, ENTER, DOWN, ENTER };
    run_on(on, 5, &weather, &light_time, 0, &reverse, 3);
    CHECK_EQ_INT(reverse, 1);
    CHECK_EQ_INT(weather, 0);
    const uint16_t off[] = { UP, RIGHT, ENTER, ENTER, DOWN, ENTER };   /* on, then off again */
    reverse = 0;
    run_on(off, 6, &weather, &light_time, 0, &reverse, 3);
    CHECK_EQ_INT(reverse, 0);
    const uint16_t back[] = { UP, RIGHT, LEFT, ENTER, DOWN, ENTER };   /* Left from Reverse is the weather: clear -> fog */
    run_on(back, 6, &weather, &light_time, 0, &reverse, 3);
    CHECK_EQ_INT(weather, 1);
    CHECK_EQ_INT(reverse, 0);
    reverse = 0, weather = 0;
    const uint16_t esc[] = { UP, RIGHT, ENTER, ESCAPE };   /* Escape puts the pick back */
    run_on(esc, 4, &weather, &light_time, 0, &reverse, 3);
    CHECK_EQ_INT(reverse, 0);
    reverse = 0;
    const uint16_t stage[] = { UP, RIGHT, ENTER, DOWN, ENTER };   /* a stage (track 1): no row, nothing set */
    run_on(stage, 5, &weather, &light_time, 0, &reverse, 1);
    CHECK_EQ_INT(reverse, 0);
}

static void test_weather_row(void)
{
    int weather = 0, light_time = 0;
    const uint16_t cycle[] = { UP, ENTER, ENTER, DOWN, ENTER };   /* Up: Weather; clear -> fog -> rain; Down: Done; Done ends */
    run(cycle, 5, &weather, &light_time, 0);
    CHECK_EQ_INT(weather, 2);
    weather = 0;
    const uint16_t wrap[] = { UP, ENTER, ENTER, ENTER, ENTER, DOWN, ENTER };
    run(wrap, 7, &weather, &light_time, 0);
    CHECK_EQ_INT(weather, 0);   /* round */
}

static void test_escape_puts_them_back(void)
{
    int weather = 0, light_time = 0;
    const uint16_t keys[] = { UP, ENTER, UP, ENTER, ESCAPE };   /* Weather -> fog; Time -> dawn; Escape */
    run(keys, 5, &weather, &light_time, 1);
    CHECK_EQ_INT(weather, 0);
    CHECK_EQ_INT(light_time, 0);
}

/* dep55: Left and Right on the laps row step Restomod's custom count; Done keeps it, Escape puts it back as it came */
static void test_custom_laps_and_escape(void)
{
    int weather = 0, light_time = 0, laps = 5;
    custom_laps = &laps;
    const uint16_t keep[] = { DOWN, RIGHT, RIGHT, UP, ENTER };   /* Done -> the laps row; 5 -> 7; back to Done */
    run(keep, 5, &weather, &light_time, 0);
    CHECK_EQ_INT(laps, 7);
    const uint16_t esc[] = { DOWN, LEFT, LEFT, LEFT, ESCAPE };
    run(esc, 5, &weather, &light_time, 0);
    CHECK_EQ_INT(laps, 7);
    laps = 1;
    const uint16_t wrap[] = { DOWN, LEFT, LEFT, UP, ENTER };   /* 1 -> the menu's (0) -> 99, round */
    run(wrap, 5, &weather, &light_time, 0);
    CHECK_EQ_INT(laps, 99);
    custom_laps = NULL;
}

static void test_light_time_toggle(void)
{
    int weather = 0, light_time = 0;
    const uint16_t keys[] = { UP, UP, ENTER, ENTER, DOWN, DOWN, ENTER };   /* Weather, Time; day -> dawn -> dusk; to Done */
    run(keys, 7, &weather, &light_time, 1);
    CHECK_EQ_INT(light_time, 2);
    CHECK_EQ_INT(weather, 0);
}

/* dusk -> night -> day stay on the screen: three steps on the Time item leave light_time 3 and a fourth wraps to 0, the screen's own exit is Done */
static void test_light_time_reaches_night_and_wraps(void)
{
    int weather = 0, light_time = 0;
    const uint16_t keys3[] = { UP, UP, ENTER, ENTER, ENTER, DOWN, DOWN, ENTER };
    run(keys3, 8, &weather, &light_time, 1);
    CHECK_EQ_INT(light_time, 3);
    light_time = 0;
    const uint16_t keys4[] = { UP, UP, ENTER, ENTER, ENTER, ENTER, DOWN, DOWN, ENTER };
    run(keys4, 9, &weather, &light_time, 1);
    CHECK_EQ_INT(light_time, 0);
}

static void test_without_the_row_up_is_the_originals(void)
{
    int weather = 0, light_time = 0;
    const uint16_t keys[] = { UP, ENTER, ESCAPE };   /* with no weather pointer: Up leads to the time of day, not the row */
    uint8_t config[TRACK_SELECT_CONFIG] = { 0 }, palette[0x300] = { 0 };
    int32_t time_of_day[9] = { 1, 1, 1, 1, 1, 1, 1, 1, 1 }, vm = 0, vr = 0;
    config[TRACK_SELECT_TRACK] = 3;
    TrackSelectPlus plus = { 0, 0, &vm, &vr, 0, 0, &weather, NULL, NULL, NULL, 0, 0, 0, 0, NULL, 0 };   /* port_track_weather 0 */
    TrackSelectState st = { config, time_of_day, 0, palette, &plus };
    (void)light_time;
    const TrackSelectInputs in = { "", 0, 0, 0, 0, 1, 1, NULL };
    DialogPointer p = { 0, 0, 0, 0, 0, 0 };
    Script script = { keys, 0, 3 };
    const TrackSelectOps ops = {
        .flush = nothing, .pointer_move_to = move_to, .load_bank = load_bank, .load_inverse = load_inverse,
        .music_feed = nothing, .shape = shape, .palette_set = palette_set, .inverse_build = nothing,
        .text_styles = nothing, .pointer_poll = nothing, .pointer_hide = nothing, .pointer_in_rect = in_rect,
        .select = select_picture, .select_default = nothing, .blit = blit, .blit_masked = blit_masked, .blit_word = blit_word,
        .text_select = style, .text_shadowed = text, .lock = nothing, .unlock = nothing, .flip = nothing,
        .pointer_show = nothing, .pointer_mark = mark, .pointer_buttons = zero, .music_start = music_start,
        .fade_in = fade_in, .music_play = nothing, .delay = delay, .sound = sound, .ticks = ticks,
        .pointer_activate_if_moved = nothing, .key = key, .pointer_click = nothing, .music_stop = nothing,
        .music_free = nothing, .fade_out = fade_out, .free = free_bank, .free_inverse = nothing, .best_times = zero,
        .car_flags = nothing, .pointer_off = nothing, .music_service = nothing, .deadline_passed = zero,
        .music_fade_out = nothing, .music_end = nothing, .ctx = &script };
    track_select_screen(&in, &st, &p, &ops);
    CHECK_EQ_INT(weather, 0);          /* Enter on the time of day toggled the exe's art, not the weather */
    CHECK_EQ_INT(time_of_day[3], 2);   /* the toggle is the exe's: midday -> evening */
}

/* the night (light_time 3): the evening art's texels moved to darker entries, the transparent one left; dusk draws it as it is */
/* the night's label is the word Night in the text font over the evening's strip; dusk keeps the art as it is */
static void test_night_label_is_text(void)
{
    uint8_t config[TRACK_SELECT_CONFIG] = { 0 }, palette[0x300];
    for (int i = 0; i < 256; i++) palette[3 * i] = palette[3 * i + 1] = palette[3 * i + 2] = (uint8_t)i;   /* a grey ramp */
    int32_t time_of_day[9] = { 1, 1, 1, 1, 1, 1, 1, 1, 1 }, vm = 0, vr = 0;
    int weather = 0;
    config[TRACK_SELECT_TRACK] = 3;
    const uint8_t day[4] = { 10, 20, 0xff, 30 };
    uint8_t after_a_race[256] = { 0 };   /* the entries a race leaves usable (palette_usable_set) are the race's, not the menu's: few, and none of the dark ones */
    after_a_race[200] = after_a_race[250] = 1;
    palette_usable_set(after_a_race);
    for (int night = 0; night < 2; night++) {
        int light_time = night ? 3 : 2;
        TrackSelectPlus plus = { 0, 0, &vm, &vr, 0, 1, &weather, &light_time, NULL, NULL, 0, 0, 0, 0, NULL, 0 };
        TrackSelectState st = { config, time_of_day, 0, palette, &plus };
        const TrackSelectInputs in = { "", 0, 0, 0, 0, 1, 1, NULL };
        DialogPointer p = { 0, 0, 0, 0, 0, 0 };
        const uint16_t keys[1] = { ESCAPE };
        Script script = { keys, 0, 0 };
        const TrackSelectOps ops = {
            .flush = nothing, .pointer_move_to = move_to, .load_bank = load_bank, .load_inverse = load_inverse,
            .music_feed = nothing, .shape = shape, .palette_set = palette_set, .inverse_build = nothing,
            .text_styles = nothing, .pointer_poll = nothing, .pointer_hide = nothing, .pointer_in_rect = in_rect,
            .select = select_picture, .select_default = nothing, .blit = blit, .blit_masked = blit_masked, .blit_word = blit_word,
            .text_select = style, .text_shadowed = text, .lock = nothing, .unlock = nothing, .flip = nothing,
            .pointer_show = nothing, .pointer_mark = mark, .pointer_buttons = zero, .music_start = music_start,
            .fade_in = fade_in, .music_play = nothing, .delay = delay, .sound = sound, .ticks = ticks,
            .pointer_activate_if_moved = nothing, .key = key, .pointer_click = nothing, .music_stop = nothing,
            .music_free = nothing, .fade_out = fade_out, .free = free_bank, .free_inverse = nothing, .best_times = zero,
            .car_flags = nothing, .pointer_off = nothing, .music_service = nothing, .deadline_passed = zero,
            .music_fade_out = nothing, .music_end = nothing, .ctx = &script };
        evening_blits = 0;
        night_words = 0;
        memset(evening_seen, 0, sizeof evening_seen);
        track_select_screen(&in, &st, &p, &ops);
        if (!night) {
            CHECK(evening_blits > 0);
            CHECK_MEM(evening_seen, day, 4);   /* dusk: the art as it is */
            CHECK_EQ_INT(night_words, 0);
        } else {
            CHECK(evening_blits > 0);   /* the strip (its word wiped: none here, the test palette is black) still covers the screen's own label */
            CHECK(night_words > 0);
        }
        CHECK_MEM(evening + 0x10, day, 4);   /* the bank's own shape is not touched */
    }
    uint8_t all[256];
    memset(all, 1, sizeof all);
    palette_usable_set(all);
}

/* dep56, Restomod: the variants are switch rows over the length (Mirror, Inverse, Rally on a circuit): Down from Done reaches
 * Mirror, on to Inverse and Rally and then the laps; Enter, Left or Right turns a row; Escape puts them all back; Next steps the
 * track and keeps them, onto Lost Vegas clears them; the Reverse row right of the weather is gone (Up from Done is the weather) */
static void test_switch_rows(void)
{
    int weather = 0, light_time = 0, reverse = 0;
    switches = 1;
    variant_m_in = variant_r_in = 0;
    const uint16_t all[] = { DOWN, ENTER, DOWN, RIGHT, DOWN, LEFT, DOWN, UP, UP, UP, UP, ENTER };   /* each on, back up to Done */
    run_on(all, 12, &weather, &light_time, 0, &reverse, 3);
    CHECK_EQ_INT(variant_m_out, 1);
    CHECK_EQ_INT(reverse, 1);
    CHECK_EQ_INT(variant_r_out, 1);
    CHECK_EQ_INT(track_out, 3);
    reverse = 0;
    const uint16_t esc[] = { DOWN, ENTER, DOWN, ENTER, ESCAPE };
    run_on(esc, 5, &weather, &light_time, 0, &reverse, 3);
    CHECK_EQ_INT(variant_m_out + variant_r_out + reverse, 0);
    const uint16_t up[] = { UP, ENTER, DOWN, ENTER };   /* Up from Done: the weather, not a Reverse row */
    run_on(up, 4, &weather, &light_time, 0, &reverse, 3);
    CHECK_EQ_INT(weather, 1);
    CHECK_EQ_INT(reverse, 0);
    weather = 0;
    /* a stage: no Inverse, Mirror then Rally */
    const uint16_t stage[] = { DOWN, DOWN, ENTER, UP, UP, ENTER };
    run_on(stage, 6, &weather, &light_time, 0, &reverse, 1);
    CHECK_EQ_INT(variant_m_out, 0);
    CHECK_EQ_INT(variant_r_out, 1);
    CHECK_EQ_INT(reverse, 0);
    /* Next keeps them (track 3 to 4), and onto Lost Vegas (7 to 8) clears them */
    variant_m_in = variant_r_in = 1;
    const uint16_t next[] = { RIGHT, RIGHT, ENTER, LEFT, LEFT, ENTER };   /* Done -> Prev -> Next, Next; back to Done */
    run_on(next, 6, &weather, &light_time, 0, &reverse, 3);
    CHECK_EQ_INT(track_out, 4);
    CHECK_EQ_INT(variant_m_out, 1);
    CHECK_EQ_INT(variant_r_out, 1);
    run_on(next, 6, &weather, &light_time, 0, &reverse, 7);
    CHECK_EQ_INT(track_out, 8);
    CHECK_EQ_INT(variant_m_out + variant_r_out, 0);
    variant_m_in = variant_r_in = 0;
    switches = 0;
}

/* dep65: the player's mirror and rally go through Lost Vegas: cleared on it, back on the track after it, and kept for the
 * caller when the screen ends on it */
/* port (dep59 step 4): Prev and Next walk the edition's tracks: with city,vegas the next of City is Lost Vegas, and so is the one before it */
static void test_an_edition_walks_its_tracks(void)
{
    int weather = 0, light_time = 0, reverse = 0;
    Edition e;
    memset(&e, 0, sizeof e);
    e.tracks = 2;
    snprintf(e.track[0], sizeof e.track[0], "city"), snprintf(e.track[1], sizeof e.track[1], "vegas");
    edition_in = &e;
    const uint16_t next[] = { RIGHT, RIGHT, ENTER, LEFT, LEFT, ENTER };   /* Next once, back to Done */
    run_on(next, 6, &weather, &light_time, 0, &reverse, 0);
    CHECK_EQ_INT(track_out, 8);
    const uint16_t prev[] = { RIGHT, ENTER, LEFT, ENTER };   /* Prev once, back to Done */
    run_on(prev, 4, &weather, &light_time, 0, &reverse, 0);
    CHECK_EQ_INT(track_out, 8);
    switches = 1;   /* Restomod's switch rows: the same walk */
    run_on(next, 6, &weather, &light_time, 0, &reverse, 0);
    CHECK_EQ_INT(track_out, 8);
    switches = 0;
    snprintf(e.track[1], sizeof e.track[1], "hidden");   /* the hidden track is never listed: City alone */
    run_on(next, 6, &weather, &light_time, 0, &reverse, 0);
    CHECK_EQ_INT(track_out, 0);
    edition_in = NULL;
}

/* port (dep59 step 5, T7c): an edition that lists an added track between City and Lost Vegas: this screen's tables are the original's nine, so
 * its arrows walk the stock tracks and never land on 16..79 (an added track has its own page) */
static void test_the_arrows_skip_added_tracks(void)
{
    int weather = 0, light_time = 0, reverse = 0;
    Edition e;
    memset(&e, 0, sizeof e);
    e.tracks = 3;
    snprintf(e.track[0], sizeof e.track[0], "city"), snprintf(e.track[1], sizeof e.track[1], "dune"), snprintf(e.track[2], sizeof e.track[2], "vegas");
    TrackPiece piece;
    memset(&piece, 0, sizeof piece);
    snprintf(piece.id, sizeof piece.id, "dune"), snprintf(piece.name, sizeof piece.name, "dune"), snprintf(piece.stem, sizeof piece.stem, "ZZB");
    piece.spec.kind = TRACK_KIND_STAGE, piece.spec.segments = 1, piece.spec.times = TRACK_TIME_DAY, piece.fingerprint = 7;
    track_registry_clear();
    CHECK(track_registry_add(&piece, NULL, 0) >= TRACK_ADDED_FIRST);
    edition_in = &e;
    const uint16_t next[] = { RIGHT, RIGHT, ENTER, LEFT, LEFT, ENTER };   /* Next once, back to Done */
    run_on(next, 6, &weather, &light_time, 0, &reverse, 0);
    CHECK_EQ_INT(track_out, 8);
    const uint16_t prev[] = { RIGHT, ENTER, LEFT, ENTER };   /* Prev once, back to Done */
    run_on(prev, 4, &weather, &light_time, 0, &reverse, 0);
    CHECK_EQ_INT(track_out, 8);
    edition_in = NULL;
    track_registry_clear();
}

/* dep59 Lunar L5: Lunar Springs' entry right after Rusty's: the moon's art and line, its rally set and the pick on; on to Autumn, both off.
 * With the switches the same walk, and off the moon the rally is the player's choice again (kept_r) */
static void test_lunar_is_an_entry(void)
{
    int weather = 0, light_time = 0, reverse = 0, lunar = 0;
    lunar_in = &lunar, lunar_list_in = TRACK_LIST_LUNAR_ON;
    const uint16_t next[] = { RIGHT, RIGHT, ENTER, LEFT, LEFT, ENTER };   /* Next once, back to Done */
    for (switches = 0; switches <= 1; switches++) {
        lunar = 0, variant_m_in = 0, variant_r_in = 0, kept_r_in = 1;
        run_on(next, 6, &weather, &light_time, 0, &reverse, 3);
        CHECK_EQ_INT(track_out, 3);
        CHECK_EQ_INT(lunar, 1);
        CHECK_EQ_INT(variant_r_out, 1);
        CHECK(strcmp(bank_path, "tracksel/lunarsel.qfs") == 0);
        CHECK(strcmp(music_name, "lunar.eas") == 0);
        variant_r_in = 1;
        run_on(next, 6, &weather, &light_time, 0, &reverse, 3);
        CHECK_EQ_INT(track_out, 4);
        CHECK_EQ_INT(lunar, 0);
        CHECK_EQ_INT(variant_r_out, switches);   /* entries: Autumn plain; switches: the player's rally choice (kept_r) back */
        CHECK(strcmp(bank_path, "tracksel/autsel.qfs") == 0);
    }
    switches = 0;
    lunar = 1, variant_r_in = 1;   /* back from Lunar: Rusty (no variants offered here) */
    const uint16_t prev[] = { RIGHT, ENTER, LEFT, ENTER };
    run_on(prev, 4, &weather, &light_time, 0, &reverse, 3);
    CHECK_EQ_INT(track_out, 3);
    CHECK_EQ_INT(lunar, 0);
    CHECK_EQ_INT(variant_r_out, 0);
    lunar_list_in = TRACK_LIST_LUNAR_NEVER, lunar = 0, variant_r_in = 0;   /* not offered: Rusty steps to Autumn */
    run_on(next, 6, &weather, &light_time, 0, &reverse, 3);
    CHECK_EQ_INT(track_out, 4);
    CHECK_EQ_INT(lunar, 0);
    lunar_in = NULL, lunar_list_in = 0, variant_r_in = 0, kept_r_in = 0;
}

static void test_the_switches_come_back_after_lost_vegas(void)
{
    int weather = 0, light_time = 0, reverse = 0;
    switches = 1;
    variant_m_in = variant_r_in = 1;
    const uint16_t through[] = { RIGHT, RIGHT, ENTER, ENTER, LEFT, LEFT, ENTER };   /* Next twice (7 -> 8 -> 0), back to Done */
    run_on(through, 7, &weather, &light_time, 0, &reverse, 7);
    CHECK_EQ_INT(track_out, 0);
    CHECK_EQ_INT(variant_m_out, 1);
    CHECK_EQ_INT(variant_r_out, 1);
    const uint16_t onto[] = { RIGHT, RIGHT, ENTER, LEFT, LEFT, ENTER };   /* Next once, ending on Lost Vegas */
    run_on(onto, 6, &weather, &light_time, 0, &reverse, 7);
    CHECK_EQ_INT(track_out, 8);
    CHECK_EQ_INT(variant_m_out + variant_r_out, 0);   /* the race on it has neither */
    CHECK_EQ_INT(kept_m_out, 1);   /* the choice, for the caller to keep */
    CHECK_EQ_INT(kept_r_out, 1);
    variant_m_in = variant_r_in = 0, kept_m_in = kept_r_in = 1;   /* opened on Lost Vegas with the choice kept */
    run_on(onto, 6, &weather, &light_time, 0, &reverse, 8);
    CHECK_EQ_INT(track_out, 0);
    CHECK_EQ_INT(variant_m_out, 1);
    CHECK_EQ_INT(variant_r_out, 1);
    kept_m_in = kept_r_in = 0;
    switches = 0;
}

/* which rows show: Restomod's only, Mirror and Rally with their unlocks and never on Lost Vegas, Inverse with the reverse value
 * on the circuits (Lost Vegas too); a tournament's the same (they are its variants) */
static void test_which_rows(void)
{
    int reverse = 0, rows[3];
    int32_t vm = 0, vr = 0;
    TrackSelectPlus plus = { 1, 1, &vm, &vr, 0, 1, NULL, NULL, &reverse, NULL, 1, 0, 0, 0, NULL, 0 };
    CHECK_EQ_INT(track_select_switch_rows(&plus, 3, rows), 3);
    CHECK(rows[0] == TRACK_SELECT_MIRROR && rows[1] == TRACK_SELECT_INVERSE && rows[2] == TRACK_SELECT_RALLY);
    CHECK_EQ_INT(track_select_switch_rows(&plus, 1, rows), 2);   /* a stage */
    CHECK_EQ_INT(track_select_switch_rows(&plus, 8, rows), 1);   /* Lost Vegas: Inverse alone */
    CHECK_EQ_INT(rows[0], TRACK_SELECT_INVERSE);
    plus.variants_locked = 1;   /* a tournament past its first race: the same rows, fixed */
    CHECK_EQ_INT(track_select_switch_rows(&plus, 3, rows), 3);
    plus.variants_locked = 0;
    plus.unlock_r = 0, plus.reverse = NULL;   /* level 1: Mirror alone */
    CHECK_EQ_INT(track_select_switch_rows(&plus, 3, rows), 1);
    CHECK_EQ_INT(rows[0], TRACK_SELECT_MIRROR);
    plus.unlock_m = 0;
    CHECK_EQ_INT(track_select_switch_rows(&plus, 3, rows), 0);   /* level 0 */
    plus.unlock_m = plus.unlock_r = 1, plus.reverse = &reverse, plus.variant_switches = 0;   /* Stock, Restored: entries */
    CHECK_EQ_INT(track_select_switch_rows(&plus, 3, rows), 0);
    CHECK_EQ_INT(track_select_switch_rows(NULL, 3, rows), 0);
    int lunar = 1;   /* dep59 Lunar L5: Lunar is the rally: no Rally row on its screen */
    plus.variant_switches = 1, plus.lunar = &lunar;
    CHECK_EQ_INT(track_select_switch_rows(&plus, 3, rows), 2);
    CHECK(rows[0] == TRACK_SELECT_MIRROR && rows[1] == TRACK_SELECT_INVERSE);
    CHECK_EQ_INT(track_select_switch_rows(&plus, 1, rows), 2);   /* the pick holds on Rusty alone */
}

/* a tournament past its first race: the rows show its variants, and neither Enter, Left nor Right turns them */
static void test_locked_rows_do_not_turn(void)
{
    int weather = 0, light_time = 0, reverse = 1;
    switches = 1, locked = 1;
    variant_m_in = 1, variant_r_in = 0;
    const uint16_t keys[] = { DOWN, ENTER, RIGHT, DOWN, ENTER, DOWN, LEFT, UP, UP, UP, ENTER };
    run_on(keys, 11, &weather, &light_time, 0, &reverse, 3);
    CHECK_EQ_INT(variant_m_out, 1);
    CHECK_EQ_INT(reverse, 1);
    CHECK_EQ_INT(variant_r_out, 0);
    CHECK_EQ_INT(track_out, 3);
    locked = 0;   /* the same keys before the first race: Mirror turned twice, Inverse and Rally once */
    run_on(keys, 11, &weather, &light_time, 0, &reverse, 3);
    CHECK_EQ_INT(variant_m_out, 1);
    CHECK_EQ_INT(reverse, 0);
    CHECK_EQ_INT(variant_r_out, 1);
    variant_m_in = variant_r_in = 0;
    switches = 0;
}

int main(void)
{
    test_switch_rows();
    test_the_switches_come_back_after_lost_vegas();
    test_an_edition_walks_its_tracks();
    test_the_arrows_skip_added_tracks();
    test_lunar_is_an_entry();
    test_locked_rows_do_not_turn();
    test_which_rows();
    test_weather_row();
    test_reverse_row();
    test_escape_puts_them_back();
    test_custom_laps_and_escape();
    test_light_time_toggle();
    test_light_time_reaches_night_and_wraps();
    test_night_label_is_text();
    test_without_the_row_up_is_the_originals();
    return TEST_REPORT();
}
