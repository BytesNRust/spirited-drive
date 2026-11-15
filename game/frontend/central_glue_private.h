/* The Control Central glue (central_glue*.c): what its files share. The names with a cg_ prefix are the file-level
 * state and steps that central_glue.c once kept static; moving them changed no behaviour. */
#ifndef TNFS_CENTRAL_GLUE_PRIVATE_H
#define TNFS_CENTRAL_GLUE_PRIVATE_H

/* 00432ff4: the checked Control Central (central.c) over game/sys and ea_lib (glue, no oracle). Its globals are
 * shorts here, filled from config.dat's race settings on entry and put back on leaving; the race settings' checked
 * steps (race_settings.c, tournament.c) run on a RaceSettings made from them. */
#include "game/frontend/menu_glue.h"
#include "game/frontend/central_glue_ops.h"

/* 00444bb0's shorts in config.dat's image (offsets from 00524df0), all at odd addresses: copied out to aligned
 * shorts around the screen */
enum { OPT_TRANSMISSION, OPT_AUTOMATIC, OPT_ABS, OPT_TRACTION, OPT_RACE_TYPE, OPT_TRACK, OPT_OPPONENT, OPT_CAR,
       OPT_NET_CAR, OPT_LAPS, OPT_DIFFICULTY, OPT_MUSIC_VOLUME, OPT_SHORTS };
/* 00413cb0's controls: config.dat's 13 shorts from 0052ad26, copied to aligned shorts around the screen */
enum { CONTROLS_AT = 0x5f36 };
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

/* the globals 00432ff4 writes (central.h gives each address) */
typedef struct {
    int16_t race_type, opponent, track, player_car, other_car[8], net_car, laps, difficulty;
    int16_t car_flag_a, car_flag_b, variant;
    int16_t volumes[3], tournament, unlock_level, races[9];
    int32_t unlock_m, unlock_r, lost_vegas, hidden, vegas_cheat, variant_m, variant_r;
    int16_t net_player, net_player_was, was[6];
    int32_t art[7];
    int16_t nomercy, warrior, net_hover, net_self, net_second, letter_held, buttons;
    int32_t explode, inverse, nitro;
} CentralGlobals;

#include "game/frontend/central_glue_ops.h"

enum { SEGMENT_AT = 0x93 };                         /* 00524e83: a stage's segment, 0..2 (00427ae0's run) */
enum { OPTIONS_AT = 0x81 };                        /* 00524e71: the five option words the race takes and gives back */
typedef void ResultsShow(int32_t ask, const ResultsInputs *in, ResultsState *st, DialogPointer *p,
                         const ResultsOps *ops);

extern CentralGlobals cg_g;
extern uint8_t cg_game_state[GAME_STATE_SIZE];
extern uint8_t cg_screen_palette[PALETTE_PUT_BYTES];
RaceSettings cg_settings_of(void);
void cg_settings_put(const RaceSettings *s);
/* port (dep59): the added car picked and the head to head rival kept in restomod.cfg when they changed (car_looks.h) */
void cg_picks_save(void);
/* port (dep59 step 3): ControlCentralOps.load_added_card (added_card_glue.c) */
int32_t cg_load_added_card(void *ctx, int car, int rival, const uint8_t palette[768]);
void cg_level_to_image(void);
void cg_races_from_state(void);
int16_t *cg_word_52b302(void);
Unlocks cg_unlocks_of(void);
void cg_unlocks_put(const Unlocks *u);
void cg_extras_apply(void);
/* port (dep56): a Restomod tournament's variants (tournament_variants.h) onto the settings for the track set; in Stock and
 * Restored a tournament's are the exe's (none), so a profile changed mid-run drops them until Restomod is back */
void cg_tournament_variants_apply(void);
/* port (dep65, central_glue_choice.c): Restomod's switches kept as the player's choice in the mode's file. _apply puts on the
 * globals what the track in hand runs from it (outside a tournament); _keep writes the switches the track screen left as the
 * choice (kept_m / kept_r the screen's TrackSelectPlus.kept_*, for a screen ended on Lost Vegas; reverse NULL when the row was
 * not offered). cg_reverse_offered: 1 when the Reverse row is offered (the mode's file's reverse, level 2). */
void cg_variant_choice_apply(void);
void cg_variant_choice_keep(int kept_m, int kept_r, const int *reverse);
/* ControlCentralOps.variants_chosen: a code typed in Control Central put a variant on; Restomod keeps the variants as it left them as
 * the choice, the same way as the switches, then puts on what the track runs of them */
void cg_variants_chosen(void *ctx);
int cg_reverse_offered(void);
int16_t cg_image_short(int at);
void cg_palette_set(void *ctx, uint8_t *rgb, int32_t first, int32_t count, const uint8_t *src, int32_t mode);
void cg_flush(void *ctx);
extern int *cg_stage_in;  /* the running screen's inputs' copy of it (results_screen.h, standings.h) */
int32_t cg_deadline_passed(void *ctx);
int32_t cg_key(void *ctx);
void cg_poll_pointer(void *ctx);
void cg_mark_at(void *ctx, DialogPointer *p);
extern int32_t cg_time_of_day[10];
void cg_fit(void *ctx, const char *text, int32_t x, int32_t y, int32_t width);
/* port: results_screen.h's band_text: the font's size, or the largest smaller one that fits `room` */
void cg_band_text(void *ctx, const char *text, int32_t x, int32_t y_mid, int32_t room);
void cg_average_text(void *ctx, char *out, int32_t time, int32_t distance);
extern int32_t cg_race_times[MENU_RACE_TIMES];
extern int16_t cg_car_points[GAME_STATE_PLAYERS];
extern TrackViewerResults cg_last_drive;
void cg_drive(void *ctx, int32_t argc, char *const *argv);
void cg_segment_results(void *ctx, int32_t mode);
void cg_track_results(void *ctx, int32_t ask);
void cg_circuit_results(void *ctx, int32_t ask);
void cg_standings(void *ctx, int32_t segment);
void cg_race(void *ctx);
extern int32_t cg_credits_idle;
extern uint32_t cg_credits_last;
void cg_credits_tick(void *ctx);
void cg_credits(void *ctx, int32_t mode);
void cg_chat(void *ctx);
void cg_after_race(void *ctx);
int32_t cg_next_segment(void *ctx);

void cg_image_car_flags(void *ctx);
extern int16_t cg_opt[OPT_SHORTS];
void cg_opt_out(void);
void cg_opt_in(void);
void cg_opt_car_flags(void *ctx);
int32_t cg_key_read(void *ctx);
void cg_controls(void *ctx, int32_t mode);
void cg_audio(void *ctx, int32_t mode);
void cg_plus_text(void *ctx, int32_t style_id, const char *text, int32_t x, int32_t y);
void cg_plus_panel(void *ctx, const uint8_t *rgb, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t framed);
void cg_plus_save(void *ctx, const PortSettings *settings);
int32_t cg_own_bgnd(uint8_t pixels[640 * 480], uint8_t palette[768]);
int32_t cg_own_bgnd_shape(void *ctx, const uint8_t pixels[640 * 480], const uint8_t palette[768]);
void cg_own_note_text(void *ctx, const char *text, int32_t x, int32_t y);
int32_t cg_display_dark(void *ctx);
void cg_own_subtitle_text(void *ctx, const char *text, int32_t x, int32_t y);
void cg_volumes_sync(void);
void cg_net_service(void *ctx);
/* the three volumes the menus keep (menu_audio_kept) */
#define KEPT_VOLUMES { menu_audio_kept()[0], menu_audio_kept()[2], menu_audio_kept()[3] }
void cg_load_save(void *ctx, int32_t mode);
void cg_load_save2(void *ctx, int32_t mode);

#endif
