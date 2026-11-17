/* port (dep59 section 4, the owner's carousel): the showcase in the carousel (ShowcaseState.leave_step). In the carousel the music is the
 * carousel's: a page leaves it playing (Next, Previous or Done) and the carousel ends it once; the original's screen (no carousel) ends
 * it when it is left, as the game's. A page entered by a step starts on that step's item, Next or Previous, so the player keeps paging
 * without moving the pointer (owner 2026-11-17); the first entry starts on the video as the game's. Stubbed ops only. */
#include "game/frontend/showcase.h"
#include "test.h"

#include <string.h>

enum { ENTER = 0xd, ESCAPE = 0x1b };

static int music_ends, music_fades, music_services, fades_out, movies, deadline_asks;
static int32_t px = -1, py = -1;
static const uint16_t *keys_in;   /* the keys in turn, 0 between (a key seen up) */
static int key_at;

static void none(void *ctx) { (void)ctx; }
static void move_to(void *ctx, int32_t x, int32_t y) { (void)ctx, px = x, py = y; }
static int32_t load_bank(void *ctx, const char *path, int32_t flags) { (void)ctx, (void)path, (void)flags; return 1; }
static const uint8_t *shape(void *ctx, int32_t bank, const char *tag)
{
    (void)ctx, (void)bank, (void)tag;
    static const uint8_t s[0x400];
    return s;
}
static void palette_set(void *ctx, uint8_t *rgb, int32_t first, int32_t count, const uint8_t *src, int32_t mode)
{
    (void)ctx, (void)rgb, (void)first, (void)count, (void)src, (void)mode;
}
static void palette_write(void *ctx, int32_t first, int32_t count, const uint8_t *rgb) { (void)ctx, (void)first, (void)count, (void)rgb; }
static int32_t in_rect(void *ctx, int32_t x0, int32_t x1, int32_t y0, int32_t y1)   /* the pointer where it was last moved to */
{
    (void)ctx;
    return px >= x0 && px <= x1 && py >= y0 && py <= y1;
}
static void select_picture(void *ctx, int32_t p) { (void)ctx, (void)p; }
static void blit(void *ctx, const uint8_t *s) { (void)ctx, (void)s; }
static int32_t picture_shape(void *ctx, int32_t p) { (void)ctx, (void)p; return 0; }
static void blit_word(void *ctx, int32_t s) { (void)ctx, (void)s; }
static int32_t zero(void *ctx) { (void)ctx; return 0; }
static int32_t deadline_passed(void *ctx) { (void)ctx; return ++deadline_asks > 200; }   /* idle long enough: the screen leaves (no hang) */
static void fade_in(void *ctx, int32_t first, int32_t count, const uint8_t *rgb) { (void)ctx, (void)first, (void)count, (void)rgb; }
static void deadline_set(void *ctx, int32_t t) { (void)ctx, (void)t; }
static void sound(void *ctx, int32_t s) { (void)ctx, (void)s; }
static int32_t key(void *ctx)
{
    (void)ctx;
    const uint16_t k = keys_in[key_at];
    if (k != 0 || keys_in[key_at + 1] != 0) key_at++;   /* the last 0 stays: no key */
    return k;
}
static void bank_free(void *ctx, int32_t b) { (void)ctx, (void)b; }
static void fade_out(void *ctx, int32_t first, int32_t count) { (void)ctx, (void)first, (void)count, fades_out++; }
static void words(void *ctx, const char *w) { (void)ctx, (void)w; }
static void music_fade_out(void *ctx) { (void)ctx, music_fades++; }
static void music_end(void *ctx) { (void)ctx, music_ends++; }
static void music_service(void *ctx) { (void)ctx, music_services++; }
static void movie(void *ctx, const char *n) { (void)ctx, (void)n, movies++; }
static void music_start(void *ctx, const char *n) { (void)ctx, (void)n; }

static const ShowcaseOps OPS = {
    .pointer_move_to = move_to, .load_bank = load_bank, .music_feed = none, .flush = none, .shape = shape, .palette_set = palette_set,
    .palette_write = palette_write, .inverse_build = none, .text_styles = none, .pointer_poll = none, .pointer_mark = none,
    .pointer_in_rect = in_rect, .select = select_picture, .blit_own = blit, .blit_masked = blit, .palette_show = palette_write,
    .select_default = none, .picture_shape = picture_shape, .blit_word = blit_word, .flip = none, .pointer_show = none,
    .pointer_buttons = zero, .fade_in = fade_in, .deadline_set = deadline_set, .sound = sound, .pointer_hide = none, .lock = none,
    .ticks = zero, .pointer_activate_if_moved = none, .key = key, .pointer_click = none, .free = bank_free, .fade_out = fade_out,
    .pages = words, .figures = words, .music_fade_out = music_fade_out, .music_end = music_end, .movie = movie,
    .music_start = music_start, .car_flags = none, .pointer_off = none, .music = music_service, .deadline_passed = deadline_passed, .ctx = NULL,
};

/* one showcase run, the keyboard's (the pointer not active), the pointer put at (x, y) first; returns the step it leaves on
 * (leave_step), or 99 without a carousel */
static int16_t run(int carousel, int16_t entry, int32_t x, int32_t y, const uint16_t *keys)
{
    static int16_t car = 6, race = 1, track = 0, offered = 1, active = 0, buttons = 0;
    static const int32_t screen = 0;
    int16_t step = entry;
    ShowcaseState st = { &car, &race, &track, &offered, "", &active, &buttons, &screen, NULL, carousel ? &step : NULL };
    music_ends = music_fades = music_services = fades_out = movies = deadline_asks = 0, keys_in = keys, key_at = 0;
    px = x, py = y;
    showcase(&st, &OPS);
    return carousel ? step : 99;
}

enum { NEXT_X = 382, NEXT_Y = 450, DONE_X = 35, DONE_Y = 445 };   /* showcase.c POINTER_X / POINTER_Y */

static void test_the_music_is_the_carousels(void)
{
    static const uint16_t ESCAPE_NOW[] = { ESCAPE, 0, 0 };
    CHECK_EQ_INT(run(1, 0, NEXT_X, NEXT_Y, ESCAPE_NOW), 0);   /* leaving the carousel's page: the carousel ends the music, not the page */
    CHECK_EQ_INT(music_ends, 0);
    CHECK_EQ_INT(music_fades, 0);
    CHECK_EQ_INT(fades_out, 1);           /* the picture fades, the music plays on */
    CHECK(music_services > 0);            /* and is serviced while the page runs */
    run(0, 0, DONE_X, DONE_Y, ESCAPE_NOW);   /* the original's screen: as the game's */
    CHECK_EQ_INT(music_ends, 1);
    CHECK_EQ_INT(music_fades, 1);
}

static void test_entry_keeps_the_step(void)
{
    static const uint16_t ENTER_NOW[] = { ENTER, 0, 0 };
    CHECK_EQ_INT(run(1, 1, -1, -1, ENTER_NOW), 1);   /* entered by Next: Enter takes Next again */
    CHECK(px == 382 && py == 450);                   /* the pointer on Next (the game's place for it) */
    CHECK_EQ_INT(movies, 0);
    CHECK_EQ_INT(run(1, -1, -1, -1, ENTER_NOW), -1);   /* by Previous: Previous again */
    CHECK(px == 230 && py == 450);
    CHECK_EQ_INT(movies, 0);
    static const uint16_t ESCAPE_NOW[] = { ESCAPE, 0, 0 };
    CHECK_EQ_INT(run(1, 0, -1, -1, ESCAPE_NOW), 0);   /* the first entry: on the video as the game's; Escape is no step */
    CHECK(px == 376 && py == 265);
}

int main(void)
{
    test_entry_keeps_the_step();
    test_the_music_is_the_carousels();
    return TEST_REPORT();
}
