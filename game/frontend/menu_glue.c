/* The checked menus over game/sys and ea_lib (menu_glue.h) */
#include "game/frontend/menu_glue.h"

#include "game/frontend/config.h"
#include "game/frontend/config_bgnd.h"
#include "game/frontend/config_bgnd.h"
#include "game/frontend/ea_lib.h"
#include "game/frontend/frontend.h"
#include "game/frontend/game_mode.h"
#include "game/frontend/intro_glue.h"
#include "game/frontend/link_game.h"
#include "game/frontend/menu_audio.h"
#include "game/frontend/message_banner.h"
#include "game/frontend/name_box.h"
#include "game/frontend/extras_menu.h"
#include "game/sys/port_extras.h"
#include "game/frontend/net_game.h"
#include "game/frontend/net_host.h"
#include "game/frontend/palette_put.h"
#include "game/frontend/replay_box.h"
#include "game/frontend/save_box.h"
#include "game/frontend/save_files.h"
#include "game/frontend/showcase.h"
#include "game/frontend/showcase_figures.h"
#include "game/frontend/showcase_pages.h"
#include "game/frontend/text_box.h"
#include "game/frontend/text_entry.h"
#include "game/frontend/text_fit.h"
#include "game/frontend/title.h"
#include "game/frontend/yes_no_box.h"
#include "game/sys/input.h"
#include "game/sys/pointer.h"
#include "game/sys/resource.h"
#include "game/sys/text.h"
#include "game/sys/timer.h"
#include "game/sys/video.h"

#include <stdio.h>
#include <stdlib.h>

MenuWords menu_words;
MenuCheats menu_cheats;

static uint8_t screen_palette[PALETTE_PUT_BYTES];  /* 0052ced8: what 00463e08 leaves */

static void nothing(void *ctx) { (void)ctx; }
static void sound(void *ctx, int32_t s) { (void)ctx; menu_audio_sound(s); }
static void sound_load(void *ctx) { (void)ctx; menu_audio_bank_load(); }
static void sound_free(void *ctx) { (void)ctx; menu_audio_bank_free(); }
static void music_feed(void *ctx) { (void)ctx; menu_audio_music_feed(); }
static void music(void *ctx) { (void)ctx; menu_audio_music_service(); }

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
    const MessageBannerWaitOps ops = { music, picture_new, picture_shape, select_picture, grab, banner,
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
/* 00441798 on the box's own short 00524790 (ctx its DialogPointer) */
static void dialog_show(void *ctx) { pointer_show(), ((DialogPointer *)ctx)->active = pointer_active(); }
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
    int x, y;
    pointer_position(&x, &y);
    DialogPointer p = { pointer_active(), 0, x, y, x, y };
    const YesNoOps ops = { &BANNER, shadowed, boxed, nothing, dialog_poll, dialog_show, dialog_buttons, click, move_to,
                           dialog_mark, key, music, sound, drain, &p };
    const int32_t yes = yes_no_box(text, resource_german(), &p, &in, &ops);
    *pointer_active_word() = (int16_t)p.active;  /* 00465f4c sets and clears the one word itself */
    return yes;
}
static int32_t typed(void *ctx) { (void)ctx; return input_next_char(); }
static int32_t pointer_poll(void *ctx) { (void)ctx; pointer_sync_from_os(); return pointer_read_buttons(); }

/* 004626d0 (text_entry.c); no joystick, no German keyboard layout switch (004770a0 / 0047706c) */
static int32_t text_field(void *ctx, char *text, int32_t length, int32_t x, int32_t y, int32_t one)
{
    (void)ctx;
    const TextEntryOps ops = { width, picture_new, picture_shape, select_picture, select_default, nothing, grab, flip,
                               blit_at, blit_picture, blit_at, style, shadowed, key, music, drain, pointer_poll,
                               nothing, typed, ticks, picture_free, nothing, nothing, NULL };
    TextEntryState st = { ea_screen(), (int16_t)pointer_active(), 0, 0 };
    int16_t *const was = pointer_bind_active(&st.pointer_active);
    const int32_t result = text_entry_edit(text, length, x, y, one, resource_german(), &st, &ops);
    pointer_bind_active(was);
    return result;
}

/* 004667fc (name_box.c) on the name at 00524df0, which is config.dat's: the port keeps it in frontend_player_name too,
 * so the image takes it at once (00466ae4 copies it to player 0's record, 0042702c to the race's entry) */
static int32_t name_box(void *ctx, char *name)
{
    (void)ctx;
    video_get_palette(banner_palette);
    const MessageBannerInputs in = { ea_screen(), banner_palette };
    const NameBoxOps ops = { &BANNER, nothing, text_field, sound, NULL };
    NameBoxState st = { &menu_cheats.keys_506, &menu_cheats.keys_50a };
    const int32_t taken = name_box_show(name, resource_german(), &in, &st, &ops);
    config_dat_set_name(config_current(), name);
    return taken;
}
/* 0042a680 / 0042a9f8's calls: ctx the game state; the saves' and replays' directory the player's ("") */
static int16_t ask(void *ctx, const char *text) { return (int16_t)yes_no(ctx, text); }
static int32_t exists(void *ctx, const char *path) { (void)ctx; return save_files_exists(path); }
static void remove_file(void *ctx, const char *path) { (void)ctx; save_files_remove(path); }
static int32_t save_game(void *ctx, const char *path) { return game_file_save(path, ctx, &save_files_ops); }
static int32_t save_replay(void *ctx, const char *path) { (void)ctx; return game_file_copy_replay(path, &save_files_ops); }
static void redraw(void *ctx, int32_t eax, int32_t edx) { (void)ctx, (void)eax, (void)edx; pointer_show(); }

/* 0042a680 (save_box.c) */
void glue_save_box(char *name, uint8_t *config, uint8_t *game, int16_t lost_vegas)
{
    video_get_palette(banner_palette);
    const MessageBannerInputs in = { ea_screen(), banner_palette };
    const SaveBoxOps ops = { &BANNER, text_field, sound, exists, ask, remove_file, save_game, banner_wait, redraw,
                             game };
    SaveBoxState st = { config, game, lost_vegas, "" };
    save_box_show(name, resource_german(), &in, &st, &ops);
}

/* 0042a9f8 (replay_box.c): the last race's Replay.rpl (0042a5e8 reads gamedata/replay/, the player's directory)
 * copied to the name */
void glue_replay_box(char *name, uint8_t *config)
{
    video_get_palette(banner_palette);
    const MessageBannerInputs in = { ea_screen(), banner_palette };
    const SaveBoxOps ops = { &BANNER, text_field, sound, exists, ask, remove_file, save_replay, banner_wait, redraw,
                             NULL };
    replay_box_show(name, resource_german(), &in, config, "", &ops);
}

void glue_banner_wait(void *ctx, const char *text, int32_t time) { banner_wait(ctx, text, time); }

/* 00408724's calls past the boxes': ctx the ShowcaseGlue */
static void clear_both(void *ctx)
{
    (void)ctx;
    for (int i = 0; i < 2; i++) ea_select_default(), video_clear_back(), video_flip();
}
static void sync(void *ctx) { (void)ctx; pointer_sync_from_os(); }
static void mark(void *ctx) { (void)ctx; pointer_mark(); }
static void blit_masked(void *ctx, const uint8_t *s) { (void)ctx; ea_blit_masked(s); }
static void palette_write(void *ctx, int32_t first, int32_t count, const uint8_t *rgb) { (void)ctx; video_set_palette(first, count, rgb); }
/* 00524738: paths.dat's showcase directory, from the data root as the original's (its names fit 0x28 bytes) */
static const char *show_dir(void) { return resource_german() ? "frontend/gshow/" : "frontend/show/"; }
static int32_t show_bank(void *ctx, const char *path, int32_t flags)
{
    (void)ctx, (void)flags;
    char full[1024];
    snprintf(full, sizeof full, "%s/%s", resource_root(), path);
    return ea_bank_load_path(full);
}
static void music_fade_out(void *ctx) { (void)ctx; menu_audio_music_fade_out(); }
static void music_end(void *ctx) { (void)ctx; menu_audio_music_end(); }
static void music_start(void *ctx, const char *name) { (void)ctx; menu_audio_music_start(name); }
static void voice_start(void *ctx, const char *name) { (void)ctx; menu_audio_track_music(name); }
static void voice_play(void *ctx) { (void)ctx; menu_audio_track_play(); }
static void voice_stop(void *ctx) { (void)ctx; menu_audio_track_stop(); }
static void voice_free(void *ctx) { (void)ctx; menu_audio_track_free(); }
static void movie(void *ctx, const char *name) { (void)ctx; glue_intro_movie(name); }  /* 004419c0 */
static void car_flags(void *ctx) { const ShowcaseGlue *in = ctx; in->car_flags(in->ctx); }

/* 00407290 / 00406008 (showcase_pages.c / showcase_figures.c) */
static void pages(void *ctx, const char *which)
{
    const ShowcaseGlue *const in = ctx;
    ShowcasePagesState st = { in->player_car, in->race_type, in->track, show_dir(), in->pointer_active,
                              in->buttons, ea_screen_slot() };
    const ShowcasePagesOps ops = {
        move_to, show_bank, music_feed, shape, palette_set, sync, mark, in_rect, select_picture, blit_own,
        blit_masked, select_default, picture_shape, blit_picture, flip, show, buttons, voice_start, fade_in, voice_play,
        deadline_set, sound, pointer_away, nothing, ticks, activate, key, click, car_flags, voice_stop, voice_free,
        fade_out, bank_free, off, music, deadline_passed, ctx };
    showcase_pages(&st, &ops, which);
}
static void figures(void *ctx, const char *which)
{
    const ShowcaseGlue *const in = ctx;
    const int16_t german = (int16_t)resource_german();
    ShowcaseFiguresState st = { in->player_car, in->race_type, in->track, &german, show_dir(),
                                in->pointer_active, in->buttons, ea_screen_slot() };
    const ShowcaseFiguresOps ops = {
        move_to, show_bank, music_feed, shape, palette_set, sync, mark, in_rect, select_picture, blit_own,
        blit_masked, select_default, picture_shape, blit_picture, flip, show, buttons, voice_start, fade_in, voice_play,
        deadline_set, sound, pointer_away, nothing, ticks, activate, key, click, car_flags, voice_stop, voice_free,
        fade_out, bank_free, off, music, deadline_passed, ctx };
    showcase_figures(&st, &ops, which);
}

/* 00408724 (showcase.c) */
void glue_showcase(const ShowcaseGlue *in)
{
    const int16_t movies = glue_movies();
    ShowcaseState st = { in->player_car, in->race_type, in->track, &movies, show_dir(),
                         in->pointer_active, in->buttons, ea_screen_slot() };
    const ShowcaseOps ops = {
        move_to, show_bank, music_feed, clear_both, shape, palette_set, palette_write, nothing, styles, sync, mark,
        in_rect, select_picture, blit_own, blit_masked, palette_write, select_default, picture_shape, blit_picture,
        flip, show, buttons, fade_in, deadline_set, sound, pointer_away, nothing, ticks, activate, key, click,
        bank_free, fade_out, pages, figures, music_fade_out, music_end, movie, music_start, car_flags, off, music,
        deadline_passed, (void *)in };
    showcase(&st, &ops);
}

/* the modem is out of scope (UDP only): the menu again */
static int32_t back_to_menu(void *ctx) { (void)ctx; return 1; }

/* --- the network screens (00454b78, 00453ef4) on the link game (link_game.c) --- */

/* 00464a30: both pages black */
static void flush(void *ctx)
{
    (void)ctx;
    for (int i = 0; i < 2; i++) ea_select_default(), video_clear_back(), video_flip();
}
static const char *service_first(void *ctx) { (void)ctx; return link_service_first(); }
static const char *service_next(void *ctx) { (void)ctx; return link_service_next(); }
static int32_t fit_width(void *ctx, const char *text) { (void)ctx; return text_width(text); }
static void fit_ramp(void *ctx, int shadow) { (void)ctx; text_ramp(shadow); }
static void fit_draw(void *ctx, const char *text, int32_t x, int32_t y) { (void)ctx; text_draw(text, x, y); }
static void fit(void *ctx, const char *text, int32_t x, int32_t y, int32_t w)
{
    (void)ctx;
    static const TextFitOps ops = { fit_width, fit_ramp, fit_draw, NULL };
    text_fit_shadowed(text, x, y, w, &ops);
}
static void clip(void *ctx, int32_t x0, int32_t y0, int32_t x1, int32_t y1) { (void)ctx; ea_clip(x0, y0, x1, y1); }
static RaceNetGames *games(void *ctx, int32_t timeout) { (void)ctx; return link_games(timeout); }
static RaceNetPlayers *players(void *ctx, const uint8_t *address) { (void)ctx; return link_players(address); }
static void players_draw(void *ctx, const uint8_t *address)
{
    (void)ctx;
    static const NetPlayersDrawOps ops = { players, style, glue_boxed, NULL };
    net_players_draw((int16_t)resource_german(), address, &ops);
}
static int32_t host(void *ctx, const char *name) { (void)ctx; return link_host(name); }
static int32_t connect(void *ctx, const uint8_t *address) { (void)ctx; return link_connect(address); }

static NetGameState *net_screen;  /* 00454b78's words while it runs (its bank 004c5570 for 00453ef4) */

/* 00453ef4 (net_host.c) */
static int32_t host_join(void *ctx)
{
    (void)ctx;
    static const NetHostOps ops = {
        picture_new, select_picture, picture_shape, grab, games, clip, move_to, ticks, in_rect, sound, deadline_set,
        shape, blit_own, style, width, fit, blit_masked, blit_at, players_draw, select_default, nothing, nothing, flip,
        show, deadline_passed, buttons, activate, music, key, click, off, picture_free, host, connect, NULL };
    link_game.view = net_screen->view;  /* 0052b316 is the menu's View word, the games listed and joined with it */
    NetHostState st = { ea_screen(), net_screen->pointer_active, net_screen->buttons, link_game.view, 0, 0,
                        net_screen->bank, frontend_player_name };
    int16_t *const was = pointer_bind_active(&st.pointer_active);
    const int32_t r = net_host_join(&st, &ops);
    pointer_bind_active(was);
    net_screen->pointer_active = st.pointer_active, net_screen->buttons = st.buttons, link_game.view = st.view;
    net_screen->view = st.view;
    return r;
}

static int32_t network_found(void *ctx) { (void)ctx; return link_services_found(); }
int32_t glue_network_found(void *ctx) { return network_found(ctx); }

/* 00454b78 (net_game.c): 0 to race, 1 the menu again, 2 the time ran out */
static int32_t network_game(void *ctx)
{
    (void)ctx;
    static const NetGameOps ops = {
        move_to, flush, palette_clear, load_art, music_feed, shape, palette_set, palette_write, nothing, styles, sync,
        service_first, service_next, mark, in_rect, style, select_picture, blit_own, fit, palette_write,
        select_default, nothing, nothing, picture_shape, nothing, flip, show, fade_in, deadline_set, sound, ticks,
        buttons, activate, key, click, host_join, off, music, deadline_passed, fade_out, bank_free, nothing, NULL };
    NetGameState st = { ea_screen(), (int16_t)pointer_active(), 0, link_game.view, 0 };
    NetGameState *const outer = net_screen;
    net_screen = &st;
    int16_t *const was = pointer_bind_active(&st.pointer_active);
    const int32_t r = net_game_menu(&st, &ops);
    pointer_bind_active(was);
    net_screen = outer;
    link_game.view = st.view;
    return r;
}
int32_t glue_network_game(void *ctx) { return network_game(ctx); }

/* 004372e4 (title.c) */
void glue_title_screen(int32_t state)
{
    TitleState s = { 0, resource_german(), menu_words.link_game, menu_words.link_lost };
    DialogPointer p = { pointer_active(), 0, 0, 0, 0, 0 };
    const TitleOps ops = { deadline_set, screen_show, banner_wait, buttons, music, deadline_passed, key, fade_out,
                           NULL };
    title_screen_show(state, &s, &p, &ops);
    menu_words.link_game = (int16_t)s.link_game, menu_words.link_lost = (int16_t)s.link_lost;
}

/* 004376ac's words while it runs: hosting or joining from its network ops changes them */
static GameModeState *mode_state;

static MenuWords words_of(const GameModeState *st)
{
    return (MenuWords){ st->lost, st->lost_shown, st->mode_menu, st->redraw, st->intro_seen, st->single,
                        st->network_raced };
}

static void words_into(GameModeState *st)
{
    st->mode_menu = menu_words.mode_menu, st->intro_seen = menu_words.intro_seen, st->single = menu_words.single;
    st->network_raced = menu_words.network_raced, st->lost = menu_words.link_game;
    st->lost_shown = menu_words.link_lost, st->redraw = menu_words.redraw;
}

static int32_t mode_network_found(void *ctx)
{
    menu_words = words_of(mode_state);
    const int32_t r = network_found(ctx);
    words_into(mode_state);
    return r;
}

static int32_t mode_network_game(void *ctx)
{
    menu_words = words_of(mode_state);
    const int32_t r = network_game(ctx);
    words_into(mode_state);
    return r;
}

/* port: the EXTRAS dispatcher the Game Mode's MODEM slot calls when the original modem flow is gone. It opens the
 * EXTRAS screen over the live Game Mode picture and saves extras.cfg and the unlock level on Done. */

/* the Game Mode screen's current picture (set before the menu is entered) */
static int32_t mode_extras_screen;
static int16_t mode_extras_unlock;

static void extras_save_glue(void *ctx, const PortExtras *e, int16_t unlock)
{
    (void)ctx;
    port_extras_save(e);
    config_dat_set_unlock_level(config_current(), unlock);
    mode_extras_unlock = unlock;
}

/* each frame starts with select(0): the back buffer with the calling menu's picture blitted on it, so the EXTRAS
 * text goes over that menu instead of a black field. */
static void extras_select_glue(void *ctx, int32_t picture)
{
    (void)ctx, (void)picture;
    ea_select_default();
    ea_blit_own((const uint8_t *)(intptr_t)ea_picture_shape(mode_extras_screen));
}
static void extras_blit_own_glue(void *ctx, const uint8_t *shape) { (void)ctx; ea_blit_own(shape); }
/* the panel shaded with the palette on screen (the caller's; extras_menu has none of its own) */
static void extras_panel_glue(void *ctx, const uint8_t *rgb, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t framed)
{
    uint8_t shown[768];
    if (rgb == NULL) video_get_palette(shown), rgb = shown;
    glue_plus_panel(ctx, rgb, x0, y0, x1, y1, framed);
}
/* port: the same clean background the Graphics screen uses, out of the one copy central_glue.c composed */
static int32_t extras_bgnd_glue(void *ctx, uint8_t pixels[EXTRAS_BGND_W * EXTRAS_BGND_H], uint8_t palette[768])
{
    (void)ctx;
    return glue_own_bgnd(pixels, palette);
}
static int32_t extras_bgnd_shape_glue(void *ctx, const uint8_t pixels[EXTRAS_BGND_W * EXTRAS_BGND_H],
                                      const uint8_t palette[768])
{
    return glue_own_bgnd_shape(ctx, pixels, palette);
}
/* Game Mode's palette as the clean cover read it (mode_cover): the label is first drawn before the fade puts it on
 * screen, when the screen's palette is still black */
static uint8_t mode_palette[768];
static int mode_palette_known;

/* Game Mode's items are art drawn larger than the game's text: "Single player" is 263 wide there, 158 in graver18 */
enum { MODE_TEXT_NUM = 33, MODE_TEXT_DEN = 20 };

/* the Game Mode label: the styles built for this screen's palette first, at the items' size */
static void extras_text_glue(void *ctx, int32_t style, const char *text, int32_t x, int32_t y)
{
    (void)ctx;
    if (mode_palette_known) text_build_styles_from(mode_palette);
    else text_build_styles();
    text_select((TextStyle)style);
    text_draw_smooth(text, x, y, MODE_TEXT_NUM, MODE_TEXT_DEN);
}
static int32_t extras_text_width_glue(void *ctx, const char *text)
{
    (void)ctx;
    return text_width_scaled(text, MODE_TEXT_NUM, MODE_TEXT_DEN);
}
static int32_t extras_deadline_passed_glue(void *ctx) { (void)ctx; return 0; }

/* port: the EXTRAS screen over whichever menu's live picture called it (Game Mode's Modem slot, Options' Extras
 * row). `unlock` is the caller's level: the one config.dat holds. Returns 1 on Done, 0 on Cancel / Escape. */
int32_t glue_extras_screen(int16_t unlock)
{
    PortExtras e = port_extras_load();
    /* the Done/Cancel art (donl/canl/dond/cand) lives in the Options bank; load it before draw_page blits and free it
     * afterwards. The Options menu loaded its bank before calling here too, but a separate load keeps each side
     * independent. */
    const int32_t options_bank = load_art(NULL, "option/options.qfs");
    /* the ops table: most slots are statics from this file's glue; the EXTRAS-specific ones (text/panel/save and
     * the select that restores the caller's picture to the back buffer) live next to this function */
    const ExtrasMenuOps ops = {
        move_to,            /* pointer_move_to */
        load_art,           /* load_art */
        shape,              /* shape */
        palette_set,        /* palette_set */
        in_rect,            /* pointer_in_rect */
        extras_select_glue, /* select */
        extras_blit_own_glue, /* blit_own */
        picture_shape,      /* picture_shape */
        select_default,     /* select_default */
        nothing,            /* pointer_hide */
        flip,               /* flip */
        show,               /* pointer_show */
        buttons,            /* pointer_buttons */
        styles,             /* text_styles */
        fade_in,            /* fade_in */
        deadline_set,       /* deadline_set */
        extras_deadline_passed_glue, /* deadline_passed */
        sound,              /* sound */
        ticks,              /* ticks */
        activate,           /* pointer_activate_if_moved */
        key,                /* key */
        click,              /* pointer_click */
        off,                /* pointer_off */
        music,              /* music */
        glue_plus_text,     /* plus_text */
        glue_plus_text_width, /* plus_text_width */
        extras_panel_glue,  /* plus_panel */
        extras_bgnd_glue,   /* plus_bgnd */
        extras_bgnd_shape_glue, /* plus_bgnd_shape */
        extras_save_glue,   /* extras_save */
        1,                  /* port_combine_variants: turning Mirror or Rally on leaves the other on (0 the exe) */
        NULL,               /* ctx */
    };
    mode_extras_screen = ea_screen();
    const int32_t result = extras_menu_run(&e, &unlock, options_bank, &ops);
    ea_bank_free(options_bank);
    return result;
}

/* Game Mode's Modem item with the extras op: no name box, back to the menu */
static void mode_extras(void *ctx)
{
    (void)ctx;
    glue_extras_screen(mode_extras_unlock);
}

/* port: the Modem item's words covered with the configuration screens' clean background in Game Mode's palette,
 * built once (it takes a while) and kept */
static void mode_cover(void *ctx, int32_t x, int32_t y, int32_t w, int32_t h)
{
    static uint8_t *cover;
    static int tried;
    (void)ctx;
    if (!tried) {
        tried = 1;
        uint8_t *pixels = malloc(CONFIG_BGND_W * CONFIG_BGND_H);
        cover = malloc(CONFIG_BGND_SHAPE_HEADER + (size_t)w * h);
        mode_palette_known = pixels != NULL && config_bgnd_load(resource_load_art, CONFIG_BGND_GAME_MODE, 0, pixels,
                                                                mode_palette);
        if (cover == NULL || !mode_palette_known || config_bgnd_shape(pixels, x, y, w, h, cover) == 0) {
            fprintf(stderr, "game mode: no clean background, the Modem words stay under the label\n");
            free(cover), cover = NULL;
        }
        free(pixels);
    }
    if (cover != NULL) ea_blit_own(cover);
}

/* 004376ac (game_mode.c) */
int32_t glue_game_mode(void)
{
    mode_extras_unlock = (int16_t)config_dat_unlock_level(config_current());
    const GameModeOps ops = { palette_clear, move_to, sound_load, load_art, music_feed, shape, palette_set, nothing,
                              in_rect, select_picture, blit_own, select_default, nothing, nothing, picture_shape,
                              nothing, flip, show, buttons, fade_in, nothing, banner_wait, deadline_set, sound, key,
                              ticks, activate, click, name_box, fade_out, pointer_away, back_to_menu, mode_network_found,
                              mode_network_game, yes_no, drain, off, music, deadline_passed, sound_free, bank_free, NULL,
                              mode_extras, extras_text_glue, extras_text_width_glue, mode_cover };
    GameModeState st = { ea_screen(), (int16_t)pointer_active(), 0, menu_words.mode_menu, menu_words.intro_seen,
                         menu_words.single, menu_words.network_raced, menu_words.link_game, menu_words.link_lost,
                         menu_words.redraw, frontend_player_name };
    int16_t *const was = pointer_bind_active(&st.pointer_active);
    mode_state = &st;
    const int32_t result = game_mode_menu(resource_german(), &st, &ops);
    mode_state = NULL;
    pointer_bind_active(was);
    menu_words = words_of(&st);
    return result;
}

/* port: the name box over Game Mode's picture (004376ac's draw, the first option lit), as the player sees it when
 * choosing there; the screen is faded out again after it, whatever came back */
static int32_t name_asked_over_game_mode(void)
{
    uint8_t rgb[PALETTE_PUT_BYTES];
    palette_clear(NULL, rgb);
    const int32_t bank = load_art(NULL, "control/gamemode.qfs");
    const uint8_t *pal = shape(NULL, bank, "!pal");
    palette_set(NULL, rgb, 0, 0x100, pal + 0x10, (int16_t)pal[0]);
    select_picture(NULL, ea_screen());
    blit_own(NULL, shape(NULL, bank, "bgnd"));
    blit_own(NULL, shape(NULL, bank, "sinl"));
    select_default(NULL);
    blit_own(NULL, (const uint8_t *)(intptr_t)picture_shape(NULL, ea_screen()));
    flip(NULL);
    fade_in(NULL, 0, 0x100, rgb);
    const int32_t taken = name_box(NULL, frontend_player_name);
    fade_out(NULL, 0, 0x100);
    bank_free(NULL, bank);
    return taken;
}

/* port: extras.cfg's `start`: the screen the front end opens on instead of Game Mode, what choosing it there does
 * (004376ac's choose) without showing Game Mode first. Network and Control Central ask the name only when
 * config.dat holds none (00524df0). Returns the front end's state (0 to race) or -1 when Game Mode is to be shown
 * (the default, or the choice was taken back: no network service, the name box cancelled, the network screen's Back). */
int32_t glue_start_screen(int start)
{
    enum { SINGLE_RACE = 0 };
    if (start != START_NETWORK && start != START_CENTRAL) return -1;
    if (start == START_NETWORK && !network_found(NULL)) return -1;
    if (frontend_player_name[0] == '\0' && !name_asked_over_game_mode()) return -1;
    menu_words.mode_menu = 1;
    menu_words.intro_seen = 1;
    if (start == START_CENTRAL) {
        menu_words.single = 0;
        menu_words.network_raced = 0;
        return SINGLE_RACE;
    }
    if (network_game(NULL) == 1) return -1;
    menu_words.network_raced = 1;
    return SINGLE_RACE;
}

int32_t glue_yes_no(void *ctx, const char *text) { return yes_no(ctx, text); }
void glue_banner(void *ctx, const char *text) { banner(ctx, text); }
void glue_drain(void *ctx) { drain(ctx); }
void glue_boxed(void *ctx, const char *text, int32_t x, int32_t y) { boxed(ctx, text, x, y); }
