/* 004373c4: the front-end loop (frontend_loop.c, checked) over the screens the build has so far. */
#include "game/frontend/frontend.h"

#include "game/frontend/config.h"
#include "game/frontend/frontend_loop.h"
#include "game/frontend/title.h"
#include "game/sys/input.h"
#include "game/sys/pointer.h"
#include "game/sys/resource.h"
#include "game/sys/timer.h"
#include "game/sys/video.h"

#include <stdio.h>
#include <stdlib.h>

char frontend_player_name[42];

static int16_t no_link;          /* 0052b312: no link game yet */
static int32_t network_raced;    /* 004fb7d4 */

/* TODO(004644f0, 00464568, 0045638c, 004640d0, 0046417c): the menu music; TODO(00466ae4): the tournament and session
 * reset; 00446220 only runs with a link game, which the build has not got yet */
static void glue_nothing(void *ctx) { (void)ctx; }

static int32_t glue_config_load(void *ctx) { (void)ctx; return config_load(); }
static void sys_deadline_set(void *ctx, int32_t time) { (void)ctx; timer_set_deadline(time); }
static int32_t sys_deadline_passed(void *ctx) { (void)ctx; return timer_deadline_passed(); }
static void sys_screen_show(void *ctx, const char *file, int32_t mode, int32_t one) { (void)ctx; show_screen(file, mode, one); }
/* TODO(00467214): the message banner; only a lost link game reaches it, and the build has none yet */
static void sys_banner_wait(void *ctx, const char *text, int32_t time) { (void)ctx, (void)time; fprintf(stderr, "%s\n", text); }
static int32_t sys_pointer_buttons(void *ctx) { (void)ctx; return pointer_read_buttons(); }
static int32_t sys_key(void *ctx) { (void)ctx; return input_get_key(); }
static void sys_fade_out(void *ctx, int32_t first, int32_t count) { (void)ctx; video_fade_out(first, count); }

/* 004372e4 (title.c, checked) */
static void glue_title(void *ctx, int32_t state)
{
    (void)ctx;
    TitleState s = { 0, resource_german(), no_link, 0 };
    DialogPointer p = { pointer_active(), 0, 0, 0, 0, 0 };
    const TitleOps ops = { sys_deadline_set, sys_screen_show, sys_banner_wait, sys_pointer_buttons, glue_nothing,
                           sys_deadline_passed, sys_key, sys_fade_out, NULL };
    title_screen_show(state, &s, &p, &ops);
}
static int32_t glue_main_menu(void *ctx) { (void)ctx; return main_menu(); }
static int32_t glue_control_central(void *ctx) { (void)ctx; return control_central(); }
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
                                  glue_nothing, glue_nothing, glue_intro, glue_nothing, glue_nothing, glue_main_menu,
                                  NULL };
    const FrontendLoopState st = { 0, 0, &no_link, &network_raced };
    frontend_loop(&st, &ops);
}
