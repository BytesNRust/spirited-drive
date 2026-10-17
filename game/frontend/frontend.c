/* 004373c4: the front-end loop (frontend_loop.c, checked) over the screens the build has so far. */
#include "game/frontend/frontend.h"

#include "game/frontend/config.h"
#include "game/frontend/frontend_loop.h"
#include "game/frontend/menu_audio.h"
#include "game/frontend/menu_glue.h"

#include <stdio.h>
#include <stdlib.h>

char frontend_player_name[42];

/* TODO(00466ae4): the tournament and session reset; 00446220 only runs with a link game, which the build has not
 * got yet */
static void glue_nothing(void *ctx) { (void)ctx; }
static void glue_music_fade_out(void *ctx) { (void)ctx; menu_audio_music_fade_out(); }
static void glue_music_end(void *ctx) { (void)ctx; menu_audio_music_end(); }
static void glue_music_by_idle(void *ctx) { (void)ctx; menu_audio_music_by_idle(); }
static void glue_music_feed(void *ctx) { (void)ctx; menu_audio_music_feed(); }

static int32_t glue_config_load(void *ctx) { (void)ctx; return config_load(); }
static void glue_title(void *ctx, int32_t state) { (void)ctx; glue_title_screen(state); }
static int32_t glue_main_menu(void *ctx) { (void)ctx; return glue_game_mode(); }
static int32_t glue_control_central(void *ctx) { (void)ctx; return glue_central(); }
static void glue_config_save(void *ctx) { (void)ctx; config_save(); }
static void glue_intro(void *ctx) { (void)ctx; intro_sequence(); }

/* the network branch (0052b31e) is not reachable yet: these stand in until the network screens are wired */
static int32_t glue_no_network(void *ctx) { (void)ctx; return 0; }
static void glue_fatal(void *ctx, const char *message)
{
    (void)ctx;
    fprintf(stderr, "%s\n", message);
    exit(1);
}
static int16_t glue_no_name(void *ctx) { (void)ctx; return 0; }
static int16_t glue_yes(void *ctx, const char *text) { (void)ctx, (void)text; return 1; }
static int32_t glue_leave(void *ctx) { (void)ctx; return 1; }

void frontend_run(void)
{
    const FrontendLoopOps ops = { glue_config_load, glue_title, glue_no_network, glue_fatal, glue_no_name, glue_yes,
                                  glue_leave, glue_nothing, glue_control_central, glue_config_save, glue_nothing,
                                  glue_music_fade_out, glue_music_end, glue_intro, glue_music_by_idle, glue_music_feed,
                                  glue_main_menu, NULL };
    const FrontendLoopState st = { 0, 0, &menu_words.link_game, &menu_words.network_raced };
    menu_audio_open();
    frontend_loop(&st, &ops);
    menu_audio_close();
}
