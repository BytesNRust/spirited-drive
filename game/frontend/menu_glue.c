/* The checked menus over game/sys and ea_lib (menu_glue.h) */
#include "game/frontend/menu_glue.h"

#include "game/frontend/ea_lib.h"
#include "game/frontend/frontend.h"
#include "game/frontend/game_mode.h"
#include "game/frontend/message_banner.h"
#include "game/frontend/name_box.h"
#include "game/frontend/palette_put.h"
#include "game/frontend/text_box.h"
#include "game/frontend/text_entry.h"
#include "game/frontend/title.h"
#include "game/frontend/yes_no_box.h"
#include "game/sys/input.h"
#include "game/sys/pointer.h"
#include "game/sys/resource.h"
#include "game/sys/text.h"
#include "game/sys/timer.h"
#include "game/sys/video.h"

#include <stdio.h>

MenuWords menu_words;
MenuCheats menu_cheats;

static uint8_t screen_palette[PALETTE_PUT_BYTES];  /* 0052ced8: what 00463e08 leaves */

/* TODO(004648dc, 0046492c, 004649c8, 004640d0, 0046417c): the menu sounds and music */
static void nothing(void *ctx) { (void)ctx; }
static void no_sound(void *ctx, int32_t sound) { (void)ctx, (void)sound; }

static void deadline_set(void *ctx, int32_t time) { (void)ctx; timer_set_deadline(time); }
static int32_t deadline_passed(void *ctx) { (void)ctx; return timer_deadline_passed(); }
static int32_t ticks(void *ctx) { (void)ctx; return timer_ticks_since_last(); }
static int32_t key(void *ctx) { (void)ctx; return input_get_key(); }
/* 004770ac: the keys typed so far (00476f94's queue) dropped */
static void drain(void *ctx) { (void)ctx; while (input_next_char() != 0) {} }
static void screen_show(void *ctx, const char *file, int32_t mode, int32_t one) { (void)ctx; show_screen(file, mode, one); }
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
static int32_t picture_new(void *ctx, int32_t w, int32_t h, int32_t flags) { (void)ctx, (void)flags; return ea_picture_new(w, h); }
static void picture_free(void *ctx, int32_t picture) { (void)ctx; ea_picture_free(picture); }
static void grab(void *ctx, int32_t s, int32_t x, int32_t y) { (void)ctx; ea_save(s, x, y); }
static void blit_at(void *ctx, int32_t s, int32_t x, int32_t y) { (void)ctx; ea_restore(s, x, y); }
static void blit_picture(void *ctx, int32_t s) { (void)ctx; ea_blit_own((const uint8_t *)(intptr_t)s); }
static void shade_table(void *ctx, int32_t r, int32_t g, int32_t b, const uint8_t *pal, uint8_t *table)
{
    (void)ctx, (void)g, (void)b;
    ea_shade_table(r, pal, table);
}
static void shade_set(void *ctx, uint8_t *table) { (void)ctx; ea_shade_set(table); }
static void shade_polygon(void *ctx, int32_t count, const int32_t *xy) { (void)ctx; ea_shade_polygon(count, xy); }
static int32_t colour(void *ctx, int32_t rgb) { (void)ctx; return ea_colour(rgb); }
static void frame(void *ctx, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t c) { (void)ctx; ea_frame(x0, y0, x1, y1, c); }

static int32_t width(void *ctx, const char *text) { (void)ctx; return text_width(text); }
static void styles(void *ctx) { (void)ctx; text_build_styles(); }
static void style(void *ctx, int32_t s) { (void)ctx; text_select((TextStyle)s); }
static void centred(void *ctx, const char *text, int32_t cx, int32_t y) { (void)ctx; text_draw_centered(text, cx, y); }
static void shadowed(void *ctx, const char *text, int32_t x, int32_t y) { (void)ctx; text_draw_shadowed(text, x, y); }

static const MessageBannerOps BANNER = { width, picture_new, picture_shape, shade_table, select_picture,
                                         select_default, grab, shade_set, shade_polygon, colour, frame, styles,
                                         style, centred, blit_at, blit_picture, flip, picture_free, NULL };
static uint8_t banner_palette[PALETTE_PUT_BYTES];  /* 00520dd8 */

/* 0046667c (message_banner.c) */
static void banner(void *ctx, const char *text)
{
    (void)ctx;
    video_get_palette(banner_palette);
    const MessageBannerInputs in = { ea_screen(), banner_palette };
    message_banner_show(text, &in, &BANNER);
}

/* 00467214 (message_banner.c) */
static void banner_wait(void *ctx, const char *text, int32_t time)
{
    (void)ctx;
    const MessageBannerWaitOps ops = { nothing, picture_new, picture_shape, select_picture, grab, banner,
                                       deadline_set, blit_at, picture_free, show, deadline_passed, key, buttons,
                                       select_default, blit_picture, flip, NULL };
    int active = pointer_active(), held = 0;
    message_banner_wait(text, time, ea_screen(), &active, &held, &ops);
}

static void dialog_poll(void *ctx, DialogPointer *p)
{
    (void)ctx;
    int x, y;
    pointer_sync_from_os();
    pointer_position(&x, &y);
    p->x = x, p->y = y;
}
static int32_t dialog_buttons(void *ctx, DialogPointer *p)
{
    dialog_poll(ctx, p);
    return pointer_read_buttons();
}
static void dialog_mark(void *ctx, DialogPointer *p)
{
    (void)ctx;
    p->mark_x = (int16_t)p->x, p->mark_y = (int16_t)p->y;
    pointer_mark();
}
static void ramp(void *ctx, int use_shadow) { (void)ctx; text_ramp(use_shadow); }
static void draw_text(void *ctx, const char *text, int32_t x, int32_t y) { (void)ctx; text_draw(text, x, y); }

/* 00463934 (text_box.c) */
static void boxed(void *ctx, const char *text, int32_t x, int32_t y)
{
    (void)ctx;
    const TextBoxOps ops = { width, picture_new, picture_shape, select_picture, grab, ramp, draw_text, nothing,
                             blit_at, select_default, blit_picture, flip, picture_free, NULL };
    text_box_show(text, x, y, ea_screen(), &ops);
}

/* 00465f4c (yes_no_box.c) */
static int32_t yes_no(void *ctx, const char *text)
{
    (void)ctx;
    video_get_palette(banner_palette);
    const MessageBannerInputs in = { ea_screen(), banner_palette };
    const YesNoOps ops = { &BANNER, shadowed, boxed, nothing, dialog_poll, show, dialog_buttons, click, move_to,
                           dialog_mark, key, nothing, no_sound, drain, NULL };
    int x, y;
    pointer_position(&x, &y);
    DialogPointer p = { pointer_active(), 0, x, y, x, y };
    return yes_no_box(text, resource_german(), &p, &in, &ops);
}
static int32_t typed(void *ctx) { (void)ctx; return input_next_char(); }
static int32_t pointer_poll(void *ctx) { (void)ctx; pointer_sync_from_os(); return pointer_read_buttons(); }

/* 004626d0 (text_entry.c); no joystick, no German keyboard layout switch (004770a0 / 0047706c) */
static int32_t text_field(void *ctx, char *text, int32_t length, int32_t x, int32_t y, int32_t one)
{
    (void)ctx;
    const TextEntryOps ops = { width, picture_new, picture_shape, select_picture, select_default, nothing, grab, flip,
                               blit_at, blit_picture, blit_at, style, shadowed, key, nothing, drain, pointer_poll,
                               nothing, typed, ticks, picture_free, nothing, nothing, NULL };
    TextEntryState st = { ea_screen(), (int16_t)pointer_active(), 0, 0 };
    return text_entry_edit(text, length, x, y, one, resource_german(), &st, &ops);
}

/* 004667fc (name_box.c) */
static int32_t name_box(void *ctx, char *name)
{
    (void)ctx;
    video_get_palette(banner_palette);
    const MessageBannerInputs in = { ea_screen(), banner_palette };
    const NameBoxOps ops = { &BANNER, nothing, text_field, no_sound, NULL };
    NameBoxState st = { &menu_cheats.keys_506, &menu_cheats.keys_50a };
    return name_box_show(name, resource_german(), &in, &st, &ops);
}
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
