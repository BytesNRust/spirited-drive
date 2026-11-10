/* The Control Central glue's one-line ops (central_glue.c): each is a screen's callback over game/sys and ea_lib that reads no state of central_glue.c,
 * so every file that hands them to a screen takes the ones it uses (static inline). Moved out of central_glue.c with no change in what they do. */
#ifndef TNFS_CENTRAL_GLUE_OPS_H
#define TNFS_CENTRAL_GLUE_OPS_H

#include "game/frontend/menu_glue.h"
#include "game/frontend/central_split.h"
#include "game/frontend/audio_menu.h"
#include "game/frontend/best_times.h"
#include "game/frontend/best_totals_screen.h"
#include "game/frontend/circuit_results.h"
#include "game/frontend/graphics_menu.h"
#include "game/frontend/graphics_own.h"
#include "game/frontend/config_bgnd.h"
#include "game/frontend/page_arrows.h"
#include "game/frontend/title_art.h"
#include "game/sys/port_log.h"
#include "game/viewer/screen_layout.h"
#include "game/frontend/central.h"
#include "game/frontend/after_race.h"
#include "game/frontend/chat.h"
#include "game/frontend/check_options.h"
#include "game/frontend/next_segment.h"
#include "game/frontend/cheat_code.h"
#include "game/frontend/config.h"
#include "game/frontend/config_bgnd.h"
#include "formats/fsh.h"
#include "game/frontend/config_dat.h"
#include "game/frontend/control_central_art.h"
#include "game/frontend/controls_menu.h"
#include "game/frontend/extras_apply.h"
#include "game/frontend/variant_rule.h"
#include "game/sys/port_extras.h"
#include "game/sys/reverse_track.h"
#include "game/sys/port_controls.h"
#include "game/frontend/cop_stop.h"
#include "game/frontend/win_movie.h"
#include "game/frontend/credits.h"
#include "game/frontend/ea_lib.h"
#include "game/frontend/key_capture.h"
#include "game/frontend/file_list.h"
#include "game/frontend/frontend.h"
#include "game/frontend/intro_glue.h"
#include "game/frontend/game_list.h"
#include "game/frontend/key_name.h"
#include "game/frontend/link_central.h"
#include "game/frontend/link_game.h"
#include "game/frontend/link_race.h"
#include "game/frontend/load_save.h"
#include "game/frontend/load_save2.h"
#include "game/frontend/lap_times.h"
#include "game/frontend/menu_audio.h"
#include "game/frontend/menu_race.h"
#include "game/frontend/palette_put.h"
#include "game/frontend/options_menu.h"
#include "game/sys/port_profile.h"
#include "game/sys/port_settings.h"
#include "game/frontend/race_settings.h"
#include "game/frontend/random_pick.h"
#include "game/frontend/save_files.h"
#include "game/frontend/record_check.h"
#include "game/frontend/records_glue.h"
#include "game/frontend/results_screen.h"
#include "game/frontend/standings.h"
#include "game/frontend/stat_text.h"
#include "game/frontend/text_fit.h"
#include "game/frontend/tournament.h"
#include "game/frontend/track_results.h"
#include "game/frontend/track_select.h"
#include "game/race/race_models.h"
#include "game/sys/input.h"
#include "game/sys/key_codes.h"
#include "game/sys/pointer.h"
#include "game/sys/resource.h"
#include "game/sys/text.h"
#include "game/sys/timer.h"
#include "game/sys/video.h"
#include "game/platform/platform.h"
#include "game/viewer/drive_joy.h"
#include "game/viewer/track_viewer.h"
#include "game/frontend/boot_screen.h"
#include "tnfs_version.h"
#include "game/sys/port_stats.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <stdio.h>
#include <stdlib.h>

static inline void net_sync(void *ctx) { (void)ctx, link_sync(); }
static inline void free_handle(void *ctx, void *art) { (void)ctx; ea_free((int32_t)(intptr_t)art); }
static inline void free_bank(void *ctx, int32_t bank) { (void)ctx; ea_free(bank); }
static inline void nothing(void *ctx) { (void)ctx; }
static inline void no_wait(void *ctx, int32_t v) { (void)ctx, (void)v; }
/* the link game (link_central.h): 00475c14(0) runs its transport */
static inline void pump(void *ctx, int32_t v) { (void)ctx, (void)v, link_service(); }
static inline int32_t chat_waiting(void *ctx) { (void)ctx; return link_any_ready(); }
static inline int32_t net_start(void *ctx) { (void)ctx; return link_ready(); }
static inline void pre_race(void *ctx) { (void)ctx, link_host_ready(); }
static inline void kick(void *ctx, int32_t player) { (void)ctx, link_kick(player); }
static inline void sound(void *ctx, int32_t s) { (void)ctx; menu_audio_sound(s); }
static inline void sound_load(void *ctx) { (void)ctx; menu_audio_bank_load(); }
static inline void sound_free(void *ctx) { (void)ctx; menu_audio_bank_free(); }
static inline void music(void *ctx) { (void)ctx; menu_audio_music_service(); }
static inline void music_feed(void *ctx) { (void)ctx; menu_audio_music_feed(); }
static inline void music_start(void *ctx, const char *name) { (void)ctx; menu_audio_music_start(name); }
static inline void music_fade_out(void *ctx) { (void)ctx; menu_audio_music_fade_out(); }
static inline void music_end(void *ctx) { (void)ctx; menu_audio_music_end(); }
static inline void music_cut(void *ctx) { (void)ctx; menu_audio_music_cut(); }
static inline void music_fade_to(void *ctx, int32_t volume) { (void)ctx; menu_audio_music_fade_to(volume); }
static inline void music_pick(void *ctx) { (void)ctx; menu_audio_music_pick(); }
static inline void no_mode(void *ctx, int32_t mode) { (void)ctx, (void)mode; }
static inline int32_t no_inverse(void *ctx, const char *name) { (void)ctx, (void)name; return 0; }
static inline int32_t scancode(void *ctx, int32_t c) { (void)ctx; return key_code((uint16_t)c); }
static inline void palette_clear(void *ctx, uint8_t *rgb) { (void)ctx; palette_put_clear(rgb); }
static inline void fade_in(void *ctx, int32_t first, int32_t count, const uint8_t *rgb) { (void)ctx; video_fade_in(first, count, rgb); }
static inline void fade_out(void *ctx, int32_t first, int32_t count) { (void)ctx; video_fade_out(first, count); }
static inline void flip(void *ctx) { (void)ctx; video_flip(); }
static inline void move_to(void *ctx, int32_t x, int32_t y) { (void)ctx; pointer_move_to(x, y); }
static inline void mark(void *ctx) { (void)ctx; pointer_mark(); }
static inline int32_t in_rect(void *ctx, int32_t x0, int32_t x1, int32_t y0, int32_t y1) { (void)ctx; return pointer_in_rect(x0, x1, y0, y1); }
static inline void show(void *ctx) { (void)ctx; pointer_show(); }
static inline int32_t buttons(void *ctx) { (void)ctx; return pointer_read_buttons(); }
static inline void activate(void *ctx) { (void)ctx; pointer_activate_if_moved(); }
static inline void click(void *ctx) { (void)ctx; pointer_click_feedback(); }
static inline void off(void *ctx) { (void)ctx; pointer_off(); }
static inline int32_t ticks(void *ctx) { (void)ctx; return timer_ticks_since_last(); }
static inline void deadline_set(void *ctx, int32_t time) { (void)ctx; timer_set_deadline(time); }
static inline const int16_t *stage_word(void) { return &link_game.flags_a[RACE_NET_STAGE_FLAG]; }
static inline int32_t load_art(void *ctx, const char *name) { (void)ctx; return ea_bank_load(name); }
/* 00476268: central passes the data path ("" here) and control/..., as 00466c34 takes it */
static inline int32_t load_bank(void *ctx, const char *path, int32_t flags) { (void)ctx, (void)flags; return ea_bank_load(path); }
static inline int32_t picture_new(void *ctx, int32_t w, int32_t h, int32_t flags) { (void)ctx, (void)flags; return ea_picture_new(w, h); }
static inline void picture_free(void *ctx, int32_t picture) { (void)ctx; ea_picture_free(picture); }
static inline const uint8_t *shape(void *ctx, int32_t bank, const char *tag) { (void)ctx; return ea_shape(bank, tag); }
static inline int32_t picture_shape(void *ctx, int32_t picture) { (void)ctx; return ea_picture_shape(picture); }
static inline void select_picture(void *ctx, int32_t picture) { (void)ctx; ea_select(picture); }
static inline void select_default(void *ctx) { (void)ctx; ea_select_default(); }
static inline void blit(void *ctx, const uint8_t *s) { (void)ctx; ea_blit_own(s); }
static inline void blit_masked(void *ctx, const uint8_t *s) { (void)ctx; ea_blit_masked(s); }
static inline void blit_word(void *ctx, int32_t s) { (void)ctx; ea_blit_own((const uint8_t *)(intptr_t)s); }
static inline void frame(void *ctx, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t c) { (void)ctx; ea_frame(x0, y0, x1, y1, c); }
static inline int32_t width(void *ctx, const char *text) { (void)ctx; return text_width(text); }
static inline void style(void *ctx, int32_t s) { (void)ctx; text_select((TextStyle)s); }
static inline void shadowed(void *ctx, const char *text, int32_t x, int32_t y) { (void)ctx; text_draw_shadowed(text, x, y); }
static inline void text_at(void *ctx, const char *text, int32_t x, int32_t y) { (void)ctx; text_draw(text, x, y); }
static inline void styles(void *ctx) { (void)ctx; text_build_styles(); }
static inline void hide(void *ctx) { (void)ctx; video_set_overlay(NULL, 0, 0); }
/* the screens with a DialogPointer as ctx: its short 00524790 set as 00441798, 00464088 and 00464060 set the one word */
static inline void show_dialog(void *ctx) { pointer_show(), ((DialogPointer *)ctx)->active = pointer_active(); }
static inline void activate_dialog(void *ctx) { pointer_activate_if_moved(), ((DialogPointer *)ctx)->active = pointer_active(); }
static inline void off_dialog(void *ctx) { pointer_off(), ((DialogPointer *)ctx)->active = pointer_active(); }
static inline void set_palette(void *ctx, int32_t first, int32_t count, const uint8_t *rgb) { (void)ctx; video_set_palette(first, count, rgb); }
static inline void track_music(void *ctx, const char *name) { (void)ctx; menu_audio_track_music(name); }
static inline void track_play(void *ctx) { (void)ctx; menu_audio_track_play(); }
static inline void track_stop(void *ctx) { (void)ctx; menu_audio_track_stop(); }
static inline void track_free(void *ctx) { (void)ctx; menu_audio_track_free(); }
static inline void free_handle_inverse(void *ctx) { (void)ctx; }
/* 0045cea0's text: 00463d34 as text_fit.c cuts it, 00466d5c / 00466e5c / 00466c9c as stat_text.c writes them */
static inline int32_t fit_width(void *ctx, const char *text) { (void)ctx; return text_width(text); }
static inline void fit_ramp(void *ctx, int shadow) { (void)ctx; text_ramp(shadow); }
static inline void fit_draw(void *ctx, const char *text, int32_t x, int32_t y) { (void)ctx; text_draw(text, x, y); }
static inline void time_text(void *ctx, char *out, int32_t time) { (void)ctx; stat_time_text(out, time); }
static inline void speed_text(void *ctx, char *out, int32_t speed) { (void)ctx; stat_speed_text(out, resource_kmh(), speed); }
/* 00461540 on the menus' game state and 0045b6c8's finishers */
static inline void fade_out_bank(void *ctx, int32_t first, int32_t count, int32_t bank) { (void)bank; fade_out(ctx, first, count); }
static inline void clip(void *ctx, int32_t x0, int32_t y0, int32_t x1, int32_t y1) { (void)ctx; ea_clip(x0, y0, x1, y1); }
static inline void no_two(void *ctx, int32_t a, int32_t b) { (void)ctx, (void)a, (void)b; }
static inline void no_three(void *ctx, int32_t a, int32_t b, int32_t c) { (void)ctx, (void)a, (void)b, (void)c; }
static inline void game_reset(void *ctx) { (void)ctx; glue_game_reset(); }
/* 00428610 (cop_stop.c): cop1..3.tgv, or the ticket (german) */
static inline void screen_file(void *ctx, const char *file, int32_t mode, int32_t one) { (void)ctx; show_screen(file, mode, one); }
static inline void movie(void *ctx, const char *file) { (void)ctx; glue_intro_movie(file); }  /* 004419c0 */
static inline int32_t read_buttons(void *ctx) { (void)ctx; pointer_sync_from_os(); return pointer_read_buttons(); }
static inline void blit_at(void *ctx, int32_t s, int32_t x, int32_t y) { (void)ctx; ea_restore(s, x, y); }
static inline void poll(void *ctx) { (void)ctx; pointer_sync_from_os(); }
/* port: our dedication and credits pages after the original's, over the credits' music */
static inline void credits_service(void) { menu_audio_music_service(); }
static inline void grab(void *ctx, int32_t s, int32_t x, int32_t y) { (void)ctx; ea_save(s, x, y); }
static inline void key_name(void *ctx, int32_t k, char *out) { (void)ctx; key_name_text(out, resource_german(), k); }
static inline int32_t keys_pending(void *ctx) { (void)ctx; return input_get_key(); }
static inline int32_t capture_buttons(void *ctx) { (void)ctx; pointer_sync_from_os(); return pointer_read_buttons(); }
static inline int32_t plus_key_down(void *ctx, int32_t scancode) { (void)ctx; return platform_key_down(scancode); }
static inline int32_t audio_clock(void *ctx) { (void)ctx; return (int32_t)timer_ticks(); }
static inline void audio_volume(void *ctx, int32_t volume) { (void)ctx; menu_audio_music_fade_to(volume); }
static inline void audio_track_music(void *ctx) { (void)ctx; menu_audio_music_track(); }
static inline void audio_stop_4a(void *ctx) { (void)ctx; menu_audio_stop_4a(); }
static inline int32_t audio_4a_done(void *ctx) { (void)ctx; return menu_audio_4a_done(); }
static inline void audio_effects_volume(void *ctx, int32_t volume) { (void)ctx; menu_audio_effects_volume(volume); }
static inline void audio_voice(void *ctx, int32_t voice, int32_t volume) { (void)ctx; menu_audio_voice_volume(voice, volume); }
static inline void audio_blit_at(void *ctx, const uint8_t *s, int32_t x, int32_t y) { (void)ctx; ea_blit_at(s, x, y); }
static inline void audio_clip(void *ctx, int32_t l, int32_t r, int32_t t, int32_t b) { (void)ctx; video_set_clip(l, r, t, b); }
static inline int32_t plus_text_width(void *ctx, const char *text) { (void)ctx; return text_width(text); }
/* 0042ae9c's calls on the player's directory (save_files.h): no directories to change into */
static inline void no_cwd(void *ctx, char *out) { (void)ctx; out[0] = 0; }
static inline void no_chdir(void *ctx, const char *path) { (void)ctx, (void)path; }
static inline const char *find_first(void *ctx, const char *pattern) { (void)ctx; return save_files_first(pattern); }
static inline const char *find_next(void *ctx) { (void)ctx; return save_files_next(); }
static inline void masked_at(void *ctx, const uint8_t *s, int32_t x, int32_t y) { (void)ctx; ea_blit_masked_at(s, x, y); }
static inline void remove_save(void *ctx, const char *path) { (void)ctx; save_files_remove(path); }
static inline void redraw_pointer(void *ctx) { (void)ctx; pointer_show(); }
static inline void replay_box(void *ctx, char *name) { (void)ctx; glue_replay_box(name, config_current()->image); }
static inline void clear_one(void *ctx) { (void)ctx; ea_select_default(), video_clear_back(), video_flip(); }
static inline void no_mode_set(void *ctx, int32_t w, int32_t h, int32_t bits) { (void)ctx, (void)w, (void)h, (void)bits; }
static inline void save_config(void *ctx) { (void)ctx; config_save(); }
static inline void after_options(void *ctx, int32_t mode) { (void)ctx; glue_check_options(mode); }
/* 0040a5d8's ops: one thread, so the lines' lock is always free; no joystick, no keyboard layouts to switch */
static inline int32_t lines_free(void *ctx, uint32_t lock) { (void)ctx, (void)lock; return 1; }
static inline void lines_done(void *ctx, uint32_t lock) { (void)ctx, (void)lock; }
static inline int32_t no_joystick(void *ctx) { (void)ctx; return 0; }
static inline void restore(void *ctx, int32_t s, int32_t x, int32_t y) { (void)ctx; ea_restore(s, x, y); }
/* 0047380c: 004737d0 less the shape's own origin, which a picture's shape (all chat shows) has at 0, 0 */
static inline void show_at(void *ctx, int32_t s, int32_t x, int32_t y) { (void)ctx; ea_restore(s, x, y); }
static inline void chat_shown(void *ctx) { (void)ctx, video_flip(), pointer_show(); }
/* 00410b7c and 00410c18: the link's deadline and the wait for the others (link_central.h) */
static inline void deadline(void *ctx, int32_t ticks) { (void)ctx, link_deadline(ticks); }
static inline int32_t all_came(void *ctx, int32_t stage) { (void)ctx; return link_wait((int16_t)stage); }

#endif
