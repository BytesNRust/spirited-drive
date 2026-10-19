#include "game/frontend/intro_glue.h"

#include "game/frontend/config.h"
#include "game/frontend/config_dat.h"
#include "game/frontend/ea_lib.h"
#include "game/frontend/frontend.h"
#include "game/frontend/intro.h"
#include "game/frontend/menu_audio.h"
#include "game/frontend/menu_glue.h"
#include "game/frontend/random_pick.h"
#include "game/frontend/save_files.h"
#include "game/frontend/tgv_player.h"
#include "game/sys/input.h"
#include "game/sys/resource.h"
#include "game/sys/video.h"
#include "game/viewer/track_viewer.h"
#include "platform/platform.h"

#include <string.h>
#include <time.h>

enum { REPLAY_FLAG = 0xaf, REPLAY_NAME = 0x3b };

static void pointer_hide(void *ctx) { (void)ctx; video_set_overlay(NULL, 0, 0); }
static void drain(void *ctx) { (void)ctx; while (input_next_char() != 0) {} }
static void window(void *ctx, int32_t x, int32_t y, int32_t w, int32_t h) { (void)ctx, (void)x, (void)y, (void)w, (void)h; }
/* "gamedata\\replay\\REPLAYn.RPL": the player's file of that name, else the CD's */
static uint8_t *replay_read(const char *path, size_t *size)
{
    const char *const name = strrchr(path, '\\');
    return save_files_read_replay(name != NULL ? name + 1 : path, size);
}
/* 004886e0 */
static int32_t file_exists(void *ctx, const char *path)
{
    (void)ctx;
    size_t size = 0;
    uint8_t *file = replay_read(path, &size);
    platform_free_file(file);
    return file != NULL;
}
static int16_t image_short(int at) { int16_t v; memcpy(&v, config_current()->image + at, sizeof v); return v; }
/* 004275f8 for the intro's replay (eax 2, 00524e9f 3: request 8, 005137e4 set) in the menus' window with their
 * volumes; 1 when its viewer left it (00509d70 0x40). 0047723c's window is the game window's place: kept. */
static int32_t replay(void *ctx, int32_t mode, const char *path)
{
    enum { DEMO = 8, LEFT = 0x40 };
    (void)ctx, (void)mode;
    size_t size = 0;
    uint8_t *file = replay_read(path, &size);
    if (file == NULL) return 0;
    const TrackViewerMenus menus = { 1, 1, 1, 0, 0, image_short(0x73), image_short(0x75), image_short(0x79),
                                     image_short(0x71), NULL, 0, 0, 0, 0, 0, 0, NULL, -1 };  /* the time of day and the flags: the replay's setup */
    menu_audio_close();
    track_viewer_drive_menus(&menus);
    const int ok = track_viewer_drive_replay_data(resource_root(), file, size, DEMO);
    menu_audio_open();
    platform_free_file(file);
    return ok && track_viewer_drive_results().end_state == LEFT;
}
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
