/* The checked menus over game/sys and ea_lib (menu_glue.h) */
#include "game/frontend/menu_glue.h"

#include "game/frontend/dialog.h"
#include "game/frontend/ea_lib.h"
#include "game/frontend/frontend.h"
#include "game/frontend/game_mode.h"
#include "game/frontend/palette_put.h"
#include "game/frontend/title.h"
#include "game/sys/input.h"
#include "game/sys/pointer.h"
#include "game/sys/resource.h"
#include "game/sys/timer.h"
#include "game/sys/video.h"

#include <stdio.h>

MenuWords menu_words;

static uint8_t screen_palette[PALETTE_PUT_BYTES];  /* 0052ced8: what 00463e08 leaves */

/* TODO(004648dc, 0046492c, 004649c8, 004640d0, 0046417c): the menu sounds and music */
static void nothing(void *ctx) { (void)ctx; }
static void no_sound(void *ctx, int32_t sound) { (void)ctx, (void)sound; }

static void deadline_set(void *ctx, int32_t time) { (void)ctx; timer_set_deadline(time); }
static int32_t deadline_passed(void *ctx) { (void)ctx; return timer_deadline_passed(); }
static int32_t ticks(void *ctx) { (void)ctx; return timer_ticks_since_last(); }
static int32_t key(void *ctx) { (void)ctx; return input_get_key(); }
/* 004770ac: the keys typed so far dropped */
static void drain(void *ctx) { (void)ctx; while (input_get_key() != 0) {} }
static void screen_show(void *ctx, const char *file, int32_t mode, int32_t one) { (void)ctx; show_screen(file, mode, one); }
/* TODO(00467214): the message banner; logged until it is wired */
static void banner_wait(void *ctx, const char *text, int32_t time) { (void)ctx, (void)time; fprintf(stderr, "%s\n", text); }
static void fade_in(void *ctx, int32_t first, int32_t count, const uint8_t *rgb) { (void)ctx; video_fade_in(first, count, rgb); }
static void fade_out(void *ctx, int32_t first, int32_t count) { (void)ctx; video_fade_out(first, count); }
static void palette_clear(void *ctx, uint8_t *rgb) { (void)ctx; palette_put_clear(rgb); }
static void palette_set(void *ctx, uint8_t *rgb, int32_t first, int32_t count, const uint8_t *src, int32_t mode)
{
    (void)ctx;
    palette_put(rgb, first, count, src, mode, screen_palette);
}

static void move_to(void *ctx, int32_t x, int32_t y) { (void)ctx; pointer_move_to(x, y); }
static int32_t in_rect(void *ctx, int32_t x0, int32_t x1, int32_t y0, int32_t y1) { (void)ctx; return pointer_in_rect(x0, x1, y0, y1); }
static int32_t buttons(void *ctx) { (void)ctx; return pointer_read_buttons(); }
static void show(void *ctx) { (void)ctx; pointer_show(); }
static void activate(void *ctx) { (void)ctx; pointer_activate_if_moved(); }
static void click(void *ctx) { (void)ctx; pointer_click_feedback(); }
static void off(void *ctx) { (void)ctx; pointer_off(); }
/* 00474558's end of the menu: the pointer taken off the screen, as the draft did */
static void pointer_away(void *ctx) { (void)ctx; video_set_overlay(NULL, 0, 0); }

static int32_t load_art(void *ctx, const char *name) { (void)ctx; return ea_bank_load(name); }
static void bank_free(void *ctx, int32_t bank) { (void)ctx; ea_bank_free(bank); }
static const uint8_t *shape(void *ctx, int32_t bank, const char *tag) { (void)ctx; return ea_shape(bank, tag); }
static void select_picture(void *ctx, int32_t picture) { (void)ctx; ea_select(picture); }
static void select_default(void *ctx) { (void)ctx; ea_select_default(); }
static int32_t picture_shape(void *ctx, int32_t picture) { (void)ctx; return ea_picture_shape(picture); }
static void blit_own(void *ctx, const uint8_t *s) { (void)ctx; ea_blit_own(s); }
static void flip(void *ctx) { (void)ctx; video_flip(); }

/* TODO(00465f4c, 004667fc): the checked boxes (yes_no_box.c, name_box.c); the drafts until they are wired */
static int32_t yes_no(void *ctx, const char *text) { (void)ctx; return dialog_yes_no(text); }
static int32_t name_box(void *ctx, char *name) { (void)ctx; return dialog_enter_name(name); }
/* the modem is out of scope (UDP only); the network branch until it is wired: no service, the menu again */
static int32_t back_to_menu(void *ctx) { (void)ctx; return 1; }
static int32_t no_network(void *ctx) { (void)ctx; return 0; }

/* 004372e4 (title.c) */
void glue_title_screen(int32_t state)
{
    TitleState s = { 0, resource_german(), menu_words.link_game, menu_words.link_lost };
    DialogPointer p = { pointer_active(), 0, 0, 0, 0, 0 };
    const TitleOps ops = { deadline_set, screen_show, banner_wait, buttons, nothing, deadline_passed, key, fade_out,
                           NULL };
    title_screen_show(state, &s, &p, &ops);
    menu_words.link_game = (int16_t)s.link_game, menu_words.link_lost = (int16_t)s.link_lost;
}

/* 004376ac (game_mode.c) */
int32_t glue_game_mode(void)
{
    const GameModeOps ops = { palette_clear, move_to, nothing, load_art, nothing, shape, palette_set, nothing, in_rect,
                              select_picture, blit_own, select_default, nothing, nothing, picture_shape, nothing, flip,
                              show, buttons, fade_in, nothing, banner_wait, deadline_set, no_sound, key, ticks,
                              activate, click, name_box, fade_out, pointer_away, back_to_menu, no_network,
                              back_to_menu, yes_no, drain, off, nothing, deadline_passed, nothing, bank_free, NULL };
    GameModeState st = { ea_screen(), (int16_t)pointer_active(), 0, menu_words.mode_menu, menu_words.intro_seen,
                         menu_words.single, menu_words.network_raced, menu_words.link_game, menu_words.link_lost,
                         menu_words.redraw, frontend_player_name };
    const int32_t result = game_mode_menu(resource_german(), &st, &ops);
    menu_words = (MenuWords){ st.lost, st.lost_shown, st.mode_menu, st.redraw, st.intro_seen, st.single,
                              st.network_raced };
    return result;
}
