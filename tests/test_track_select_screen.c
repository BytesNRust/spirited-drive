/* The track screen's weather row and PS1 time, driven through track_select_screen with ops that only feed keys:
 * Up from Done reaches the weather, Enter cycles it, Escape puts it back, Done keeps it. */
#include "game/frontend/track_select.h"
#include "test.h"

#include <string.h>

enum { UP = TRACK_SELECT_KEY_UP, DOWN = TRACK_SELECT_KEY_DOWN, LEFT = TRACK_SELECT_KEY_LEFT, RIGHT = TRACK_SELECT_KEY_RIGHT, ENTER = 0xd, ESCAPE = 0x1b };

typedef struct Script {
    const uint16_t *keys;
    int next, count;
} Script;

static void nothing(void *ctx) { (void)ctx; }
static int32_t pointer_x, pointer_y;   /* where the screen last put the pointer: the keys' moves are seen through it */
static void move_to(void *ctx, int32_t x, int32_t y) { (void)ctx; pointer_x = x, pointer_y = y; }
static int32_t load_bank(void *ctx, const char *path, int32_t flags) { (void)ctx, (void)path, (void)flags; return 1; }
static int32_t load_inverse(void *ctx, const char *name) { (void)ctx, (void)name; return 1; }
static uint8_t picture[0x400];   /* the bank's !pal and every shape: black */
static const uint8_t *shape(void *ctx, int32_t bank, const char *tag) { (void)ctx, (void)bank, (void)tag; return picture; }
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
static void blit_word(void *ctx, int32_t s) { (void)ctx, (void)s; }
static void style(void *ctx, int32_t s) { (void)ctx, (void)s; }
static void text(void *ctx, const char *t, int32_t x, int32_t y) { (void)ctx, (void)t, (void)x, (void)y; }
static void mark(void *ctx, DialogPointer *p) { (void)ctx, (void)p; }
static int32_t zero(void *ctx) { (void)ctx; return 0; }
static void music_start(void *ctx, const char *name) { (void)ctx, (void)name; }
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

/* the screen on track 3 (not a tournament), the keys, and what the weather and the PS1 time are afterwards */
static void run_on(const uint16_t *keys, int count, int *weather, int *psx_time, int use_psx_time, int *reverse, int track)
{
    uint8_t config[TRACK_SELECT_CONFIG] = { 0 }, palette[0x300] = { 0 };
    int32_t time_of_day[9] = { 1, 1, 1, 1, 1, 1, 1, 1, 1 }, variant_m = 0, variant_r = 0;
    config[TRACK_SELECT_TRACK] = (uint8_t)track;
    config[TRACK_SELECT_LAPS] = 8;
    const TrackSelectInputs in = { "", 0, 0, 0, 0, 1, 1 };
    TrackSelectPlus plus = { 0, 0, &variant_m, &variant_r, 0, 1, weather, use_psx_time ? psx_time : NULL, reverse };
    TrackSelectState st = { config, time_of_day, 0, palette, &plus };
    DialogPointer p = { 0, 0, 0, 0, 0, 0 };
    Script script = { keys, 0, count };
    const TrackSelectOps ops = {
        .flush = nothing, .pointer_move_to = move_to, .load_bank = load_bank, .load_inverse = load_inverse,
        .music_feed = nothing, .shape = shape, .palette_set = palette_set, .inverse_build = nothing,
        .text_styles = nothing, .pointer_poll = nothing, .pointer_hide = nothing, .pointer_in_rect = in_rect,
        .select = select_picture, .select_default = nothing, .blit = blit, .blit_masked = blit, .blit_word = blit_word,
        .text_select = style, .text_shadowed = text, .lock = nothing, .unlock = nothing, .flip = nothing,
        .pointer_show = nothing, .pointer_mark = mark, .pointer_buttons = zero, .music_start = music_start,
        .fade_in = fade_in, .music_play = nothing, .delay = delay, .sound = sound, .ticks = ticks,
        .pointer_activate_if_moved = nothing, .key = key, .pointer_click = nothing, .music_stop = nothing,
        .music_free = nothing, .fade_out = fade_out, .free = free_bank, .free_inverse = nothing, .best_times = zero,
        .car_flags = nothing, .pointer_off = nothing, .music_service = nothing, .deadline_passed = zero,
        .music_fade_out = nothing, .music_end = nothing, .ctx = &script };
    track_select_screen(&in, &st, &p, &ops);
    CHECK_EQ_INT(time_of_day[3], 1);   /* the art stays at midday under the PS1 look */
}

static void run(const uint16_t *keys, int count, int *weather, int *psx_time, int use_psx_time)
{
    run_on(keys, count, weather, psx_time, use_psx_time, NULL, 3);
}

static void test_reverse_row(void)
{
    int weather = 0, psx_time = 0, reverse = 0;
    /* Up: Weather; Right: Reverse; Enter: on; Down: Done; Done ends */
    const uint16_t on[] = { UP, RIGHT, ENTER, DOWN, ENTER };
    run_on(on, 5, &weather, &psx_time, 0, &reverse, 3);
    CHECK_EQ_INT(reverse, 1);
    CHECK_EQ_INT(weather, 0);
    const uint16_t off[] = { UP, RIGHT, ENTER, ENTER, DOWN, ENTER };   /* on, then off again */
    reverse = 0;
    run_on(off, 6, &weather, &psx_time, 0, &reverse, 3);
    CHECK_EQ_INT(reverse, 0);
    const uint16_t back[] = { UP, RIGHT, LEFT, ENTER, DOWN, ENTER };   /* Left from Reverse is the weather: clear -> fog */
    run_on(back, 6, &weather, &psx_time, 0, &reverse, 3);
    CHECK_EQ_INT(weather, 1);
    CHECK_EQ_INT(reverse, 0);
    reverse = 0, weather = 0;
    const uint16_t esc[] = { UP, RIGHT, ENTER, ESCAPE };   /* Escape puts the pick back */
    run_on(esc, 4, &weather, &psx_time, 0, &reverse, 3);
    CHECK_EQ_INT(reverse, 0);
    reverse = 0;
    const uint16_t stage[] = { UP, RIGHT, ENTER, DOWN, ENTER };   /* a stage (track 1): no row, nothing set */
    run_on(stage, 5, &weather, &psx_time, 0, &reverse, 1);
    CHECK_EQ_INT(reverse, 0);
}

static void test_weather_row(void)
{
    int weather = 0, psx_time = 0;
    const uint16_t cycle[] = { UP, ENTER, ENTER, DOWN, ENTER };   /* Up: Weather; clear -> fog -> rain; Down: Done; Done ends */
    run(cycle, 5, &weather, &psx_time, 0);
    CHECK_EQ_INT(weather, 2);
    weather = 0;
    const uint16_t wrap[] = { UP, ENTER, ENTER, ENTER, DOWN, ENTER };
    run(wrap, 6, &weather, &psx_time, 0);
    CHECK_EQ_INT(weather, 0);   /* round */
}

static void test_escape_puts_them_back(void)
{
    int weather = 0, psx_time = 0;
    const uint16_t keys[] = { UP, ENTER, UP, ENTER, ESCAPE };   /* Weather -> fog; Time -> dawn; Escape */
    run(keys, 5, &weather, &psx_time, 1);
    CHECK_EQ_INT(weather, 0);
    CHECK_EQ_INT(psx_time, 0);
}

static void test_psx_time_toggle(void)
{
    int weather = 0, psx_time = 0;
    const uint16_t keys[] = { UP, UP, ENTER, ENTER, DOWN, DOWN, ENTER };   /* Weather, Time; day -> dawn -> dusk; to Done */
    run(keys, 7, &weather, &psx_time, 1);
    CHECK_EQ_INT(psx_time, 2);
    CHECK_EQ_INT(weather, 0);
}

static void test_without_the_row_up_is_the_originals(void)
{
    int weather = 0, psx_time = 0;
    const uint16_t keys[] = { UP, ENTER, ESCAPE };   /* with no weather pointer: Up leads to the time of day, not the row */
    uint8_t config[TRACK_SELECT_CONFIG] = { 0 }, palette[0x300] = { 0 };
    int32_t time_of_day[9] = { 1, 1, 1, 1, 1, 1, 1, 1, 1 }, vm = 0, vr = 0;
    config[TRACK_SELECT_TRACK] = 3;
    TrackSelectPlus plus = { 0, 0, &vm, &vr, 0, 0, &weather, NULL, NULL };   /* port_track_weather 0 */
    TrackSelectState st = { config, time_of_day, 0, palette, &plus };
    (void)psx_time;
    const TrackSelectInputs in = { "", 0, 0, 0, 0, 1, 1 };
    DialogPointer p = { 0, 0, 0, 0, 0, 0 };
    Script script = { keys, 0, 3 };
    const TrackSelectOps ops = {
        .flush = nothing, .pointer_move_to = move_to, .load_bank = load_bank, .load_inverse = load_inverse,
        .music_feed = nothing, .shape = shape, .palette_set = palette_set, .inverse_build = nothing,
        .text_styles = nothing, .pointer_poll = nothing, .pointer_hide = nothing, .pointer_in_rect = in_rect,
        .select = select_picture, .select_default = nothing, .blit = blit, .blit_masked = blit, .blit_word = blit_word,
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

int main(void)
{
    test_weather_row();
    test_reverse_row();
    test_escape_puts_them_back();
    test_psx_time_toggle();
    test_without_the_row_up_is_the_originals();
    return TEST_REPORT();
}
