/* The checked menus over game/sys and ea_lib (menu_glue.h) */
#include "game/frontend/menu_glue.h"

#include "game/frontend/ea_lib.h"
#include "game/frontend/frontend.h"
#include "game/frontend/game_mode.h"
#include "game/frontend/intro_glue.h"
#include "game/frontend/menu_audio.h"
#include "game/frontend/message_banner.h"
#include "game/frontend/name_box.h"
#include "game/frontend/palette_put.h"
#include "game/frontend/replay_box.h"
#include "game/frontend/save_box.h"
#include "game/frontend/save_files.h"
#include "game/frontend/showcase.h"
#include "game/frontend/showcase_figures.h"
#include "game/frontend/showcase_pages.h"
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
                               blit_at, blit_picture, blit_at, style, shadowed, key, nothing, drain, pointer_poll,
                               nothing, typed, ticks, picture_free, nothing, nothing, NULL };
    TextEntryState st = { ea_screen(), (int16_t)pointer_active(), 0, 0 };
    int16_t *const was = pointer_bind_active(&st.pointer_active);
    const int32_t result = text_entry_edit(text, length, x, y, one, resource_german(), &st, &ops);
    pointer_bind_active(was);
    return result;
}

/* 004667fc (name_box.c) */
static int32_t name_box(void *ctx, char *name)
{
    (void)ctx;
    video_get_palette(banner_palette);
    const MessageBannerInputs in = { ea_screen(), banner_palette };
    const NameBoxOps ops = { &BANNER, nothing, text_field, sound, NULL };
    NameBoxState st = { &menu_cheats.keys_506, &menu_cheats.keys_50a };
    return name_box_show(name, resource_german(), &in, &st, &ops);
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

/* the modem is out of scope (UDP only); the network branch until it is wired: no service, the menu again */
static int32_t back_to_menu(void *ctx) { (void)ctx; return 1; }
static int32_t no_network(void *ctx) { (void)ctx; return 0; }

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

/* 004376ac (game_mode.c) */
int32_t glue_game_mode(void)
{
    const GameModeOps ops = { palette_clear, move_to, sound_load, load_art, music_feed, shape, palette_set, nothing,
                              in_rect, select_picture, blit_own, select_default, nothing, nothing, picture_shape,
                              nothing, flip, show, buttons, fade_in, nothing, banner_wait, deadline_set, sound, key,
                              ticks, activate, click, name_box, fade_out, pointer_away, back_to_menu, no_network,
                              back_to_menu, yes_no, drain, off, music, deadline_passed, sound_free, bank_free, NULL };
    GameModeState st = { ea_screen(), (int16_t)pointer_active(), 0, menu_words.mode_menu, menu_words.intro_seen,
                         menu_words.single, menu_words.network_raced, menu_words.link_game, menu_words.link_lost,
                         menu_words.redraw, frontend_player_name };
    int16_t *const was = pointer_bind_active(&st.pointer_active);
    const int32_t result = game_mode_menu(resource_german(), &st, &ops);
    pointer_bind_active(was);
    menu_words = (MenuWords){ st.lost, st.lost_shown, st.mode_menu, st.redraw, st.intro_seen, st.single,
                              st.network_raced };
    return result;
}

int32_t glue_yes_no(void *ctx, const char *text) { return yes_no(ctx, text); }
void glue_banner(void *ctx, const char *text) { banner(ctx, text); }
void glue_drain(void *ctx) { drain(ctx); }
void glue_boxed(void *ctx, const char *text, int32_t x, int32_t y) { boxed(ctx, text, x, y); }
