#include "game/frontend/intro_glue.h"

#include "game/frontend/config.h"
#include "game/frontend/config_dat.h"
#include "game/frontend/ea_lib.h"
#include "game/frontend/frontend.h"
#include "game/frontend/intro.h"
#include "game/frontend/menu_glue.h"
#include "game/frontend/random_pick.h"
#include "game/frontend/tgv_player.h"
#include "game/sys/input.h"
#include "game/sys/resource.h"
#include "game/sys/video.h"

#include <string.h>
#include <time.h>

enum { REPLAY_FLAG = 0xaf, REPLAY_NAME = 0x3b };

static void pointer_hide(void *ctx) { (void)ctx; video_set_overlay(NULL, 0, 0); }
static void drain(void *ctx) { (void)ctx; while (input_next_char() != 0) {} }
static void window(void *ctx, int32_t x, int32_t y, int32_t w, int32_t h) { (void)ctx, (void)x, (void)y, (void)w, (void)h; }
/* TODO(004275f8): the demo replay in its window; until then no REPLAY%d.RPL is found */
static int32_t file_exists(void *ctx, const char *path) { (void)ctx, (void)path; return 0; }
static int32_t replay(void *ctx, int32_t mode, const char *path) { (void)ctx, (void)mode, (void)path; return 0; }
static void video_reset(void *ctx) { (void)ctx; video_wait_vblank(); }
static void palette_set(void *ctx, int32_t first, int32_t count, const uint8_t *rgb) { (void)ctx; video_set_palette(first, count, rgb); }
/* 00464a30: both pages black (screen_clear_both) */
static void clear_both(void *ctx)
{
    (void)ctx;
    for (int i = 0; i < 2; i++) ea_select_default(), video_clear_back(), video_flip();
}
/* 00477980: the window keeps 640 x 480; the player draws the 320 x 200 screen twice the size */
static void mode_set(void *ctx, int32_t w, int32_t h, int32_t bits) { (void)ctx, (void)w, (void)h, (void)bits; }
static int32_t movie(void *ctx, const char *path) { (void)ctx; return tgv_player_play(path); }
static int32_t pick(void *ctx)
{
    static uint32_t rand_state;
    (void)ctx;
    return random_pick_of_three((uint32_t)time(NULL), &rand_state);
}
static int32_t screen_show(void *ctx, const char *name, int32_t mode, int32_t music) { (void)ctx; return show_screen(name, mode, music); }

static const IntroOps ops = { pointer_hide, drain, window, file_exists, replay, video_reset, palette_set, clear_both,
                              mode_set, movie, pick, screen_show, NULL };

/* 00441abc's words and config.dat's replay flag and name (copied back: the image is byte-addressed) */
static IntroState state(int16_t *flag, char *movie_path)
{
    static int16_t replay_seen, replay_index, attract_seen, attract;
    char *const name = (char *)config_current()->image + REPLAY_NAME;
    memcpy(flag, config_current()->image + REPLAY_FLAG, sizeof *flag);
    return (IntroState){ glue_movies(), &replay_seen, &replay_index, &attract_seen, &attract, flag, name,
                         resource_dir(PATH_MOVIE), movie_path };
}

void glue_intro_show(void)
{
    int16_t flag;
    char path[INTRO_PATH];
    IntroState s = state(&flag, path);
    intro_show(&s, &ops);
    memcpy(config_current()->image + REPLAY_FLAG, &flag, sizeof flag);
}

void glue_intro_movie(const char *name)
{
    int16_t flag;
    char path[INTRO_PATH];
    IntroState s = state(&flag, path);
    intro_movie(&s, &ops, name);
}
