/* 004373c4: the front-end loop (frontend_loop.c, checked) over the screens the build has so far. */
#include "game/frontend/frontend.h"

#include "game/frontend/config.h"
#include "game/frontend/frontend_loop.h"
#include "game/frontend/intro_glue.h"
#include "game/frontend/link_game.h"
#include "game/frontend/menu_audio.h"
#include "game/frontend/menu_glue.h"
#include "game/sys/input.h"
#include "game/sys/port_extras.h"

#include <stdio.h>
#include <stdlib.h>

char frontend_player_name[42];

static void glue_reset(void *ctx) { (void)ctx; glue_game_reset(); }
static void glue_music_fade_out(void *ctx) { (void)ctx; menu_audio_music_fade_out(); }
static void glue_music_end(void *ctx) { (void)ctx; menu_audio_music_end(); }
static void glue_music_by_idle(void *ctx) { (void)ctx; menu_audio_music_by_idle(); }
static void glue_music_feed(void *ctx) { (void)ctx; menu_audio_music_feed(); }

/* A window closed after config.dat was read: it saved as on leaving Control Central (004373c4 saves it only there;
 * the original has no window to close) */
static void save_on_close(void)
{
    glue_settings_to_config();
    config_save();
}
static int32_t glue_config_load(void *ctx)
{
    (void)ctx;
    const int32_t defaults = config_load();
    input_on_close(save_on_close);
    input_link_words(&menu_words.link_game, &menu_words.link_lost);
    return defaults;
}
static void glue_title(void *ctx, int32_t state) { (void)ctx; glue_title_screen(state); }
/* port: the first time, extras.cfg's start screen instead of Game Mode (menu_glue.c) */
static int32_t glue_main_menu(void *ctx)
{
    static int started;
    (void)ctx;
    if (!started) {
        started = 1;
        const int32_t state = glue_start_screen(port_extras_load().start);
        if (state >= 0) return state;
    }
    return glue_game_mode();
}
static int32_t glue_control_central(void *ctx) { (void)ctx; return glue_central(); }
static void glue_config_save(void *ctx) { (void)ctx; config_save(); }
static void glue_intro(void *ctx) { (void)ctx; glue_intro_show(); }

/* 00446220 when a link game ends (link_game.c) */
static void glue_link_close(void *ctx) { (void)ctx; link_close(); }
static void glue_fatal(void *ctx, const char *message)
{
    (void)ctx;
    fprintf(stderr, "%s\n", message);
    exit(1);
}
static int16_t glue_no_name(void *ctx) { (void)ctx; return 0; }
static int16_t glue_yes(void *ctx, const char *text) { (void)ctx, (void)text; return 1; }

void frontend_run(void)
{
    const FrontendLoopOps ops = { glue_config_load, glue_title, glue_network_found, glue_fatal, glue_no_name, glue_yes,
                                  glue_network_game, glue_reset, glue_control_central, glue_config_save, glue_link_close,
                                  glue_music_fade_out, glue_music_end, glue_intro, glue_music_by_idle, glue_music_feed,
                                  glue_main_menu, NULL };
    const FrontendLoopState st = { 0, 0, &menu_words.link_game, &menu_words.network_raced };
    menu_audio_open();
    frontend_loop(&st, &ops);
    menu_audio_close();
}
