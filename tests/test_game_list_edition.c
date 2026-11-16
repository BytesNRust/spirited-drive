/* dep59 step 4, T10: a saved game is a tournament (game_list.c's load_game), so in an edition that closes the tournament (edition_rules.h)
 * picking one says why (GameListOps.load_refused) and loads nothing: the game state, config.dat's image and the tournament word stay as
 * they were, and "Load game ?" is not asked. With no reason (the standard edition, a stock one), it loads as the exe does. */
#include "game/frontend/file_list.h"
#include "game/frontend/game_list.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

enum { KEY_ENTER = 0xd, KEY_ESCAPE = 0x1b };

typedef struct {
    const char *why;            /* what load_refused answers */
    int keys_asked, loads, asks;
    char banner[96];
} Fake;

static void v(void *c) { (void)c; }
static int32_t zero(void *c) { (void)c; return 0; }
static int32_t one(void *c) { (void)c; return 1; }
static int32_t pic_new(void *c, int32_t w, int32_t h, int32_t f) { (void)c, (void)w, (void)h, (void)f; return 1; }
static int32_t pic_shape(void *c, int32_t p) { (void)c; return p; }
static void sel(void *c, int32_t p) { (void)c, (void)p; }
static void grab(void *c, int32_t s, int32_t x, int32_t y) { (void)c, (void)s, (void)x, (void)y; }
static int32_t art(void *c, const char *n) { (void)c, (void)n; return 1; }
static void cwd(void *c, char *out) { (void)c; out[0] = '\0'; }
static void cd(void *c, const char *p) { (void)c, (void)p; }
static int listed;
static const char *first(void *c, const char *p) { (void)c, (void)p; listed = 1; return "SLOT1.SAV"; }
static const char *next(void *c) { (void)c; return NULL; }
static void clip(void *c, int32_t a, int32_t b, int32_t d, int32_t e) { (void)c, (void)a, (void)b, (void)d, (void)e; }
static void move(void *c, int32_t x, int32_t y) { (void)c, (void)x, (void)y; }
static int32_t in_rect(void *c, int32_t a, int32_t b, int32_t d, int32_t e) { (void)c, (void)a, (void)b, (void)d, (void)e; return 0; }
static void sound(void *c, int32_t s) { (void)c, (void)s; }
static const uint8_t *shape(void *c, int32_t b, const char *t)
{
    static uint8_t s[0x20];
    (void)c, (void)b, (void)t;
    return s;
}
static void blit_shape(void *c, const uint8_t *s, int32_t x, int32_t y) { (void)c, (void)s, (void)x, (void)y; }
static void fit(void *c, const char *t, int32_t x, int32_t y, int32_t w) { (void)c, (void)t, (void)x, (void)y, (void)w; }
static void blit_pic(void *c, int32_t s) { (void)c, (void)s; }
static void blit_at(void *c, int32_t s, int32_t x, int32_t y) { (void)c, (void)s, (void)x, (void)y; }
static void blit_masked(void *c, const uint8_t *s) { (void)c, (void)s; }
static void free_pic(void *c, int32_t p) { (void)c, (void)p; }
static void rm(void *c, const char *p) { (void)c, (void)p; }

/* Enter on the list's first row, a turn with no key, then Escape */
static int32_t key(void *c)
{
    Fake *f = c;
    static const int32_t KEYS[] = { 0, KEY_ENTER, 0, KEY_ESCAPE };
    const int at = f->keys_asked++;
    return at < 4 ? KEYS[at] : KEY_ESCAPE;
}
static int32_t yes_no(void *c, const char *t) { (void)t; ((Fake *)c)->asks++; return 1; }
static void banner(void *c, const char *t, int32_t n)
{
    Fake *f = c;
    (void)n;
    snprintf(f->banner, sizeof f->banner, "%s", t);
}
static void game_load(void *c, const char *n) { (void)n; ((Fake *)c)->loads++; }
static void fade(void *c, int32_t v) { (void)c, (void)v; }
static int32_t cars(void *c, const char *n, char *id) { (void)c, (void)n, (void)id; return 0; }
static const char *refused(void *c) { return ((Fake *)c)->why; }

static void run(Fake *f, uint8_t *game, uint8_t *config, int16_t *tournament)
{
    static uint8_t names[(GAME_LIST_NAMES + 1) * FILE_LIST_NAME], kept[GAME_LIST_STATE], controls[0x1a];
    static char replay[16];
    int16_t pointer = 0, buttons = 0;
    int32_t vegas = 0;
    GameListState st = { 0, 0, 0, 1, "", "", names, &pointer, &buttons, config, replay, game, kept, controls, &vegas, tournament, 0 };
    const GameListOps ops = {
        .text_styles = v, .picture_new = pic_new, .picture_shape = pic_shape, .select = sel, .grab = grab, .load_art = art, .music_feed = v,
        .getcwd = cwd, .chdir = cd, .find_first = first, .find_next = next, .clip = clip, .pointer_move_to = move, .ticks = one,
        .pointer_in_rect = in_rect, .sound = sound, .deadline_set = sound, .shape = shape, .blit_shape_at = blit_shape, .text_select = sound,
        .text_fit = fit, .blit_masked_at = blit_shape, .select_default = v, .lock = v, .blit_own = blit_pic, .blit_at = blit_at, .unlock = v,
        .flip = v, .pointer_show = v, .deadline_passed = one, .wait = v, .pointer_buttons = zero, .pointer_activate_if_moved = v, .music = v,
        .key = key, .pointer_click = v, .yes_no = yes_no, .remove_file = rm, .banner_wait = banner, .game_load = game_load,
        .music_fade_to = fade, .pointer_redraw = v, .pointer_off = v, .blit_masked = blit_masked, .picture_free = free_pic, .free = free_pic,
        .replay_cars = cars, .ctx = f, .load_refused = refused };
    (void)game_list_screen(GAME_LIST_LOAD_GAME, &st, &ops);
}

static void test_refused_in_an_edition(void)
{
    static uint8_t game[GAME_LIST_STATE], config[0xac], before_game[GAME_LIST_STATE], before_config[0xac];
    memset(game, 0x5a, sizeof game), memset(config, 0x21, sizeof config);
    config[8] = 0;   /* the image begins with the player's name, ended (the save's file name is made from it) */
    memcpy(before_game, game, sizeof game), memcpy(before_config, config, sizeof config);
    int16_t tournament = 0;
    Fake f;
    memset(&f, 0, sizeof f);
    f.why = "Tournament needs the stock cars and tracks";
    run(&f, game, config, &tournament);
    CHECK(listed);
    CHECK_EQ_INT(f.loads, 0);
    CHECK_EQ_INT(f.asks, 0);
    CHECK(strcmp(f.banner, f.why) == 0);
    CHECK_EQ_INT(tournament, 0);
    CHECK(memcmp(game, before_game, sizeof game) == 0 && memcmp(config, before_config, sizeof config) == 0);
}

static void test_loaded_otherwise(void)
{
    static uint8_t game[GAME_LIST_STATE], config[0xac];
    int16_t tournament = 0;
    Fake f;
    memset(&f, 0, sizeof f);
    run(&f, game, config, &tournament);
    CHECK_EQ_INT(f.asks, 1);
    CHECK_EQ_INT(f.loads, 1);
    CHECK_EQ_INT(tournament, 1);
}

int main(void)
{
    test_refused_in_an_edition();
    test_loaded_otherwise();
    return TEST_REPORT();
}
