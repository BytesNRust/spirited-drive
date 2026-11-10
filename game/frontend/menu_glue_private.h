/* The checked menus' shared names (menu_glue.c, menu_glue_dialogs.c, menu_glue_screens.c, menu_glue_extras.c), split from menu_glue.c
 * with no change in what they do: the one-line ops every screen's glue hands to the checked screens, and the few names that
 * cross a file. */
#ifndef TNFS_MENU_GLUE_PRIVATE_H
#define TNFS_MENU_GLUE_PRIVATE_H

#include "game/frontend/menu_glue.h"
#include "game/frontend/config.h"
#include "game/frontend/config_bgnd.h"
#include "game/frontend/config_bgnd.h"
#include "game/frontend/ea_lib.h"
#include "game/frontend/frontend.h"
#include "game/frontend/game_mode.h"
#include "game/frontend/intro_glue.h"
#include "game/frontend/item_art.h"
#include "game/frontend/link_game.h"
#include "game/frontend/menu_audio.h"
#include "game/frontend/message_banner.h"
#include "game/frontend/name_box.h"
#include "game/frontend/options_menu.h"
#include "game/frontend/extras_menu.h"
#include "game/platform/platform.h"
#include "game/sys/port_extras.h"
#include "game/sys/port_profile.h"
#include "game/sys/port_settings.h"
#include "game/platform/platform.h"
#include "game/frontend/net_game.h"
#include "game/frontend/net_host.h"
#include "game/frontend/palette_put.h"
#include "game/frontend/replay_box.h"
#include "game/frontend/save_box.h"
#include "game/frontend/save_files.h"
#include "game/frontend/showcase.h"
#include "game/frontend/showcase_figures.h"
#include "game/frontend/showcase_pages.h"
#include "game/frontend/showcase_variants.h"
#include "game/race/race_models.h"
#include "game/frontend/text_box.h"
#include "game/frontend/text_entry.h"
#include "game/frontend/text_fit.h"
#include "game/frontend/title.h"
#include "game/frontend/yes_no_box.h"
#include "game/sys/input.h"
#include "game/sys/pointer.h"
#include "game/sys/port_log.h"
#include "game/sys/resource.h"
#include "game/sys/text.h"
#include "game/sys/timer.h"
#include "game/sys/video.h"
#include "game/frontend/version_footer.h"
#include "game/platform/update.h"
#include "tnfs_version.h"
#include <stdio.h>
#include <stdlib.h>

#include <stdio.h>
#include <stdlib.h>

/* what 00463e08 leaves (menu_glue.c) */
extern uint8_t menu_glue_screen_palette[PALETTE_PUT_BYTES];

/* dialogs.c */
void menu_glue_banner_wait(void *ctx, const char *text, int32_t time);
int32_t menu_glue_yes_no(void *ctx, const char *text);
int32_t menu_glue_name_box(void *ctx, char *name);
/* screens.c */
int32_t menu_glue_network_found(void *ctx);
int32_t menu_glue_network_game(void *ctx);
int32_t menu_glue_back_to_menu(void *ctx);
/* extras.c */
extern uint8_t menu_glue_mode_palette[768];
extern int menu_glue_mode_palette_known;
void menu_glue_extras_text_glue(void *ctx, int32_t style, const char *text, int32_t x, int32_t y);
int32_t menu_glue_extras_text_width_glue(void *ctx, const char *text);
void menu_glue_window_follows(const PortSettings *was);

/* --- the ops: one line each, over game/sys and ea_lib (static inline: each file takes the ones it uses) --- */

static inline void nothing(void *ctx) { (void)ctx; }
static inline void sound(void *ctx, int32_t s) { (void)ctx; menu_audio_sound(s); }
static inline void sound_load(void *ctx) { (void)ctx; menu_audio_bank_load(); }
static inline void sound_free(void *ctx) { (void)ctx; menu_audio_bank_free(); }
static inline void music_feed(void *ctx) { (void)ctx; menu_audio_music_feed(); }
static inline void music(void *ctx) { (void)ctx; menu_audio_music_service(); }

static inline void deadline_set(void *ctx, int32_t time) { (void)ctx; timer_set_deadline(time); }
static inline int32_t deadline_passed(void *ctx) { (void)ctx; return timer_deadline_passed(); }
static inline int32_t ticks(void *ctx) { (void)ctx; return timer_ticks_since_last(); }
static inline int32_t key(void *ctx) { (void)ctx; return input_get_key(); }
/* 004770ac: the keys typed so far (00476f94's queue) dropped */
static inline void drain(void *ctx) { (void)ctx; while (input_next_char() != 0) {} }
static inline void screen_show(void *ctx, const char *file, int32_t mode, int32_t one) { (void)ctx; show_screen(file, mode, one); }
static inline void fade_in(void *ctx, int32_t first, int32_t count, const uint8_t *rgb) { (void)ctx; video_fade_in(first, count, rgb); }
static inline void fade_out(void *ctx, int32_t first, int32_t count) { (void)ctx; video_fade_out(first, count); }
static inline void palette_clear(void *ctx, uint8_t *rgb) { (void)ctx; palette_put_clear(rgb); }
static inline void palette_set(void *ctx, uint8_t *rgb, int32_t first, int32_t count, const uint8_t *src, int32_t mode)
{
    (void)ctx;
    palette_put(rgb, first, count, src, mode, menu_glue_screen_palette);
}

static inline void move_to(void *ctx, int32_t x, int32_t y) { (void)ctx; pointer_move_to(x, y); }
static inline int32_t in_rect(void *ctx, int32_t x0, int32_t x1, int32_t y0, int32_t y1) { (void)ctx; return pointer_in_rect(x0, x1, y0, y1); }
static inline int32_t buttons(void *ctx) { (void)ctx; return pointer_read_buttons(); }
static inline void show(void *ctx) { (void)ctx; pointer_show(); }
static inline void activate(void *ctx) { (void)ctx; pointer_activate_if_moved(); }
static inline void click(void *ctx) { (void)ctx; pointer_click_feedback(); }
static inline void off(void *ctx) { (void)ctx; pointer_off(); }
/* 00474558's end of the menu: the pointer taken off the screen, as the draft did */
static inline void pointer_away(void *ctx) { (void)ctx; video_set_overlay(NULL, 0, 0); }

static inline int32_t load_art(void *ctx, const char *name) { (void)ctx; return ea_bank_load(name); }
static inline void bank_free(void *ctx, int32_t bank) { (void)ctx; ea_bank_free(bank); }
static inline const uint8_t *shape(void *ctx, int32_t bank, const char *tag) { (void)ctx; return ea_shape(bank, tag); }
static inline void select_picture(void *ctx, int32_t picture) { (void)ctx; ea_select(picture); }
static inline void select_default(void *ctx) { (void)ctx; ea_select_default(); }
static inline int32_t picture_shape(void *ctx, int32_t picture) { (void)ctx; return ea_picture_shape(picture); }
static inline void blit_own(void *ctx, const uint8_t *s) { (void)ctx; ea_blit_own(s); }
static inline void flip(void *ctx) { (void)ctx; video_flip(); }
static inline int32_t picture_new(void *ctx, int32_t w, int32_t h, int32_t flags) { (void)ctx, (void)flags; return ea_picture_new(w, h); }
static inline void picture_free(void *ctx, int32_t picture) { (void)ctx; ea_picture_free(picture); }
static inline void grab(void *ctx, int32_t s, int32_t x, int32_t y) { (void)ctx; ea_save(s, x, y); }
static inline void blit_at(void *ctx, int32_t s, int32_t x, int32_t y) { (void)ctx; ea_restore(s, x, y); }
static inline void blit_picture(void *ctx, int32_t s) { (void)ctx; ea_blit_own((const uint8_t *)(intptr_t)s); }
static inline void shade_table(void *ctx, int32_t r, int32_t g, int32_t b, const uint8_t *pal, uint8_t *table)
{
    (void)ctx, (void)g, (void)b;
    ea_shade_table(r, pal, table);
}
static inline void shade_set(void *ctx, uint8_t *table) { (void)ctx; ea_shade_set(table); }
static inline void shade_polygon(void *ctx, int32_t count, const int32_t *xy) { (void)ctx; ea_shade_polygon(count, xy); }
static inline int32_t colour(void *ctx, int32_t rgb) { (void)ctx; return ea_colour(rgb); }
static inline void frame(void *ctx, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t c) { (void)ctx; ea_frame(x0, y0, x1, y1, c); }

static inline int32_t width(void *ctx, const char *text) { (void)ctx; return text_width(text); }
static inline void styles(void *ctx) { (void)ctx; text_build_styles(); }
static inline void style(void *ctx, int32_t s) { (void)ctx; text_select((TextStyle)s); }
static inline void centred(void *ctx, const char *text, int32_t cx, int32_t y) { (void)ctx; text_draw_centered(text, cx, y); }
static inline void shadowed(void *ctx, const char *text, int32_t x, int32_t y) { (void)ctx; text_draw_shadowed(text, x, y); }

#endif
