/* 00432ff4: the checked Control Central (central.c) over game/sys and ea_lib (glue, no oracle). Its globals are
 * shorts here, filled from config.dat's race settings on entry and put back on leaving; the race settings' checked
 * steps (race_settings.c, tournament.c) run on a RaceSettings made from them. */
#include "game/frontend/menu_glue.h"

#include "game/frontend/audio_menu.h"
#include "game/frontend/best_times.h"
#include "game/frontend/best_totals_screen.h"
#include "game/frontend/circuit_results.h"
#include "game/frontend/graphics_menu.h"
#include "game/frontend/central.h"
#include "game/frontend/config.h"
#include "game/frontend/config_dat.h"
#include "game/frontend/control_central_art.h"
#include "game/frontend/controls_menu.h"
#include "game/frontend/credits.h"
#include "game/frontend/ea_lib.h"
#include "game/frontend/key_capture.h"
#include "game/frontend/file_list.h"
#include "game/frontend/game_list.h"
#include "game/frontend/key_name.h"
#include "game/frontend/load_save.h"
#include "game/frontend/lap_times.h"
#include "game/frontend/menu_audio.h"
#include "game/frontend/menu_race.h"
#include "game/frontend/palette_put.h"
#include "game/frontend/options_menu.h"
#include "game/frontend/race_settings.h"
#include "game/frontend/save_files.h"
#include "game/frontend/record_check.h"
#include "game/frontend/results_screen.h"
#include "game/frontend/standings.h"
#include "game/frontend/stat_text.h"
#include "game/frontend/text_fit.h"
#include "game/frontend/tournament.h"
#include "game/frontend/track_results.h"
#include "game/frontend/track_select.h"
#include "game/race/race_models.h"
#include "game/sys/input.h"
#include "game/sys/pointer.h"
#include "game/sys/resource.h"
#include "game/sys/text.h"
#include "game/sys/timer.h"
#include "game/sys/video.h"
#include "game/platform/platform.h"
#include "game/viewer/track_viewer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* the globals 00432ff4 writes (central.h gives each address) */
static struct {
    int16_t race_type, opponent, track, player_car, other_car[8], net_car, laps, difficulty;
    int16_t car_flag_a, car_flag_b, variant;
    int16_t volumes[3], tournament, unlock_level, races[9];
    int32_t unlock_m, unlock_r, lost_vegas, hidden, vegas_cheat, variant_m, variant_r;
    int16_t net_player, net_player_was, was[6];
    int32_t art[7];
    int16_t nomercy, warrior, music_silent, net_hover, net_self, net_second, letter_held, pointer_active, buttons;
    int32_t explode, inverse, nitro;
} g = { .was = { 0x14, 0x14, 0x14, 0x14, 0x14, 0x14 } };  /* 004bafb4..'s data: no art shown yet */

static uint8_t game_state[GAME_STATE_SIZE];       /* 0052b2c0..: the tournament's */
static uint8_t keys[0x100];                       /* 004c693c: no key-down table on game/sys yet, none held */
static uint8_t screen_palette[PALETTE_PUT_BYTES];

static RaceSettings settings_of(void)
{
    RaceSettings s = { g.race_type, g.track, g.variant_m, g.variant_r, g.player_car, g.opponent, g.laps,
                       g.difficulty, g.car_flag_a, g.car_flag_b, g.variant, { 0 }, g.net_car };
    for (int i = 0; i < 8; i++) s.other_car[i] = g.other_car[i];
    return s;
}

static void settings_put(const RaceSettings *s)
{
    g.race_type = (int16_t)s->race_type, g.track = (int16_t)s->track, g.player_car = (int16_t)s->player_car;
    g.variant_m = s->variant_m, g.variant_r = s->variant_r, g.opponent = (int16_t)s->opponent;
    g.laps = (int16_t)s->laps, g.difficulty = (int16_t)s->difficulty, g.variant = (int16_t)s->variant;
    g.car_flag_a = (int16_t)s->car_flag_a, g.car_flag_b = (int16_t)s->car_flag_b, g.net_car = (int16_t)s->unknown_524e9d;
    for (int i = 0; i < 8; i++) g.other_car[i] = (int16_t)s->other_car[i];
}

static Unlocks unlocks_of(void) { return (Unlocks){ g.unlock_m, g.unlock_r, g.lost_vegas, g.hidden, g.warrior }; }

static void unlocks_put(const Unlocks *u)
{
    g.unlock_m = u->variant_m, g.unlock_r = u->variant_r, g.lost_vegas = u->lost_vegas, g.hidden = u->hidden;
    g.warrior = (int16_t)u->warrior;
}

static FrontendSession session_of(void)
{
    FrontendSession f = { g.unlock_level, g.vegas_cheat, menu_words.link_game, g.net_self, g.net_player,
                          menu_words.intro_seen, g.nomercy, { 0 }, g.tournament, resource_german(), 0 };
    for (int i = 0; i < 6; i++) f.unknown_4bafb4[i] = g.was[i];
    return f;
}

static void session_put(const FrontendSession *f)
{
    g.vegas_cheat = f->unknown_4baf1c, g.net_self = (int16_t)f->unknown_4faa56, g.net_player = (int16_t)f->other_index;
    g.nomercy = (int16_t)f->unknown_4c5cc2, g.tournament = (int16_t)f->tournament_on;
    for (int i = 0; i < 6; i++) g.was[i] = (int16_t)f->unknown_4bafb4[i];
}

/* 00465ec8, 00432974, 00432c40, 00432d48 and 00432720 on the shorts */
static void car_flags(void *ctx)
{
    (void)ctx;
    RaceSettings s = settings_of();
    race_settings_car_flags(&s);
    settings_put(&s);
}

static void type_changed(void *ctx)
{
    (void)ctx;
    RaceSettings s = settings_of();
    Unlocks u = unlocks_of();
    FrontendSession f = session_of();
    race_settings_type_changed(&s, &u, &f);
    settings_put(&s), unlocks_put(&u), session_put(&f);
}

static void track_forward(void *ctx)
{
    (void)ctx;
    RaceSettings s = settings_of();
    const Unlocks u = unlocks_of();
    race_settings_track_forward(&s, &u);
    settings_put(&s);
}

static void track_backward(void *ctx)
{
    (void)ctx;
    RaceSettings s = settings_of();
    const Unlocks u = unlocks_of();
    race_settings_track_backward(&s, &u);
    settings_put(&s);
}

static int quit_tournament(int german, void *ctx)
{
    return glue_yes_no(ctx, german != 0 ? "Wettkampf beenden ?" : "Quit tournament ?");
}

static void step_race_type(void *ctx, int32_t key)
{
    RaceSettings s = settings_of();
    Unlocks u = unlocks_of();
    FrontendSession f = session_of();
    race_settings_type_step(&s, &u, &f, game_state, config_current(), key, quit_tournament, ctx);
    settings_put(&s), unlocks_put(&u), session_put(&f);
}

static void free_handle(void *ctx, void *art) { (void)ctx; ea_free((int32_t)(intptr_t)art); }
static void free_bank(void *ctx, int32_t bank) { (void)ctx; ea_free(bank); }
static void nothing(void *ctx) { (void)ctx; }

/* 00432884 on the art's handles */
static void art_free(void *ctx)
{
    ControlCentralArt a;
    void **const slot[7] = { &a.art_4bafc0, &a.art_4bafc4, &a.art_4bafc8, &a.art_4bafcc, &a.art_4bafd0, &a.art_4bafd4,
                             &a.art_4bafd8 };
    for (int i = 0; i < 7; i++) *slot[i] = (void *)(intptr_t)g.art[i];
    FrontendSession f = session_of();
    const ControlCentralArtOps ops = { free_handle, nothing, ctx };
    control_central_art_free(&a, &f, &ops);
    for (int i = 0; i < 7; i++) g.art[i] = (int32_t)(intptr_t)*slot[i];
    session_put(&f);
}

/* TODO(0040a5d8): chat; TODO(00446284): the race's setup; TODO(004104d0, 004109b4,
 * 0044633c, 00432e90): the link game; TODO(00437258, 00483f8c): the cheats need a key-down table on game/sys */
static void no_wait(void *ctx, int32_t v) { (void)ctx, (void)v; }
static void sound(void *ctx, int32_t s) { (void)ctx; menu_audio_sound(s); }
static void sound_load(void *ctx) { (void)ctx; menu_audio_bank_load(); }
static void sound_free(void *ctx) { (void)ctx; menu_audio_bank_free(); }
static void music(void *ctx) { (void)ctx; menu_audio_music_service(); }
static void music_feed(void *ctx) { (void)ctx; menu_audio_music_feed(); }
static void music_start(void *ctx, const char *name) { (void)ctx; menu_audio_music_start(name); }
static void music_fade_out(void *ctx) { (void)ctx; menu_audio_music_fade_out(); }
static void music_end(void *ctx) { (void)ctx; menu_audio_music_end(); }
static void music_fade_to(void *ctx, int32_t volume) { (void)ctx; menu_audio_music_fade_to(volume); }
static void music_pick(void *ctx) { (void)ctx; menu_audio_music_pick(); }
static void no_mode(void *ctx, int32_t mode) { (void)ctx, (void)mode; }
static void no_kick(void *ctx, int32_t player) { (void)ctx, (void)player; }
static int32_t zero(void *ctx) { (void)ctx; return 0; }
static int32_t no_inverse(void *ctx, const char *name) { (void)ctx, (void)name; return 0; }
static int32_t no_cheat(void *ctx, int which) { (void)ctx, (void)which; return 0; }
static int32_t not_down(void *ctx, int32_t scancode) { (void)ctx, (void)scancode; return 0; }
static int32_t scancode(void *ctx, int32_t c) { (void)ctx; return c & 0xff; }

/* config.dat's short at 00524df0 + at */
static int16_t image_short(int at)
{
    int16_t v;
    memcpy(&v, config_current()->image + at, sizeof v);
    return v;
}

static void race(void *ctx);  /* 00427ae0, below */

static void palette_clear(void *ctx, uint8_t *rgb) { (void)ctx; palette_put_clear(rgb); }
static void palette_set(void *ctx, uint8_t *rgb, int32_t first, int32_t count, const uint8_t *src, int32_t mode)
{
    (void)ctx;
    palette_put(rgb, first, count, src, mode, screen_palette);
}
static void fade_in(void *ctx, int32_t first, int32_t count, const uint8_t *rgb) { (void)ctx; video_fade_in(first, count, rgb); }
static void fade_out(void *ctx, int32_t first, int32_t count) { (void)ctx; video_fade_out(first, count); }
static void flip(void *ctx) { (void)ctx; video_flip(); }
/* 00464a30: both pages black (screen_clear_both) */
static void flush(void *ctx)
{
    (void)ctx;
    for (int i = 0; i < 2; i++) ea_select_default(), video_clear_back(), video_flip();
}

static void move_to(void *ctx, int32_t x, int32_t y) { (void)ctx; pointer_move_to(x, y); }
static void mark(void *ctx) { (void)ctx; pointer_mark(); }
static int32_t in_rect(void *ctx, int32_t x0, int32_t x1, int32_t y0, int32_t y1) { (void)ctx; return pointer_in_rect(x0, x1, y0, y1); }
static void show(void *ctx) { (void)ctx; pointer_show(); }
static int32_t buttons(void *ctx) { (void)ctx; return pointer_read_buttons(); }
static void activate(void *ctx) { (void)ctx; pointer_activate_if_moved(); }
static void click(void *ctx) { (void)ctx; pointer_click_feedback(); }
static void off(void *ctx) { (void)ctx; pointer_off(); }

static int32_t ticks(void *ctx) { (void)ctx; return timer_ticks_since_last(); }
static void deadline_set(void *ctx, int32_t time) { (void)ctx; timer_set_deadline(time); }
static int32_t deadline_passed(void *ctx) { (void)ctx; return timer_deadline_passed(); }
static int32_t key(void *ctx) { (void)ctx; return input_get_key(); }

static int32_t load_art(void *ctx, const char *name) { (void)ctx; return ea_bank_load(name); }
/* 00476268: central passes the data path ("" here) and control/..., as 00466c34 takes it */
static int32_t load_bank(void *ctx, const char *path, int32_t flags) { (void)ctx, (void)flags; return ea_bank_load(path); }
static int32_t picture_new(void *ctx, int32_t w, int32_t h, int32_t flags) { (void)ctx, (void)flags; return ea_picture_new(w, h); }
static void picture_free(void *ctx, int32_t picture) { (void)ctx; ea_picture_free(picture); }
static const uint8_t *shape(void *ctx, int32_t bank, const char *tag) { (void)ctx; return ea_shape(bank, tag); }
static int32_t picture_shape(void *ctx, int32_t picture) { (void)ctx; return ea_picture_shape(picture); }
static void select_picture(void *ctx, int32_t picture) { (void)ctx; ea_select(picture); }
static void select_default(void *ctx) { (void)ctx; ea_select_default(); }
static void blit(void *ctx, const uint8_t *s) { (void)ctx; ea_blit_own(s); }
static void blit_masked(void *ctx, const uint8_t *s) { (void)ctx; ea_blit_masked(s); }
static void blit_word(void *ctx, int32_t s) { (void)ctx; ea_blit_own((const uint8_t *)(intptr_t)s); }
static void frame(void *ctx, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t c) { (void)ctx; ea_frame(x0, y0, x1, y1, c); }

static int32_t width(void *ctx, const char *text) { (void)ctx; return text_width(text); }
static void style(void *ctx, int32_t s) { (void)ctx; text_select((TextStyle)s); }
static void shadowed(void *ctx, const char *text, int32_t x, int32_t y) { (void)ctx; text_draw_shadowed(text, x, y); }
static void text_at(void *ctx, const char *text, int32_t x, int32_t y) { (void)ctx; text_draw(text, x, y); }
static void styles(void *ctx) { (void)ctx; text_build_styles(); }

/* 004687d0's pointer: ctx is its DialogPointer */
static void poll_pointer(void *ctx)
{
    DialogPointer *const p = ctx;
    int x, y;
    pointer_sync_from_os();
    pointer_position(&x, &y);
    p->x = x, p->y = y;
}
static void hide(void *ctx) { (void)ctx; video_set_overlay(NULL, 0, 0); }
static void mark_at(void *ctx, DialogPointer *p)
{
    (void)ctx;
    p->mark_x = (int16_t)p->x, p->mark_y = (int16_t)p->y;
    pointer_mark();
}
static void set_palette(void *ctx, int32_t first, int32_t count, const uint8_t *rgb) { (void)ctx; video_set_palette(first, count, rgb); }
static void track_music(void *ctx, const char *name) { (void)ctx; menu_audio_track_music(name); }
static void track_play(void *ctx) { (void)ctx; menu_audio_track_play(); }
static void track_stop(void *ctx) { (void)ctx; menu_audio_track_stop(); }
static void track_free(void *ctx) { (void)ctx; menu_audio_track_free(); }
static void free_handle_inverse(void *ctx) { (void)ctx; }

static int32_t time_of_day[10];  /* 004fbae0 */

/* 00465ec8 on config.dat's image, which 004687d0 changes */
static void image_car_flags(void *ctx)
{
    (void)ctx;
    ConfigDat *const config = config_current();
    RaceSettings s = settings_of();
    if (!config_dat_get_race(config, &s)) return;
    race_settings_car_flags(&s);
    config_dat_set_race(config, &s);
}

/* 004687d0 on config.dat's image (00524df9..), the globals written to it first and read back after */
static int32_t best_times(void *ctx);
static void track_select(void *ctx)
{
    (void)ctx;
    ConfigDat *const config = config_current();
    const RaceSettings before = settings_of();
    config_dat_set_race(config, &before);
    const int32_t screen = ea_screen();
    const TrackSelectInputs in = { "", resource_german(), g.variant_m, g.variant_r, g.lost_vegas, screen,
                                   ea_picture_shape(screen) };
    TrackSelectState st = { config->image + (0x524df9 - 0x524df0), time_of_day, g.inverse, screen_palette };
    int x, y;
    pointer_position(&x, &y);
    DialogPointer p = { pointer_active(), 0, x, y, x, y };
    const TrackSelectOps ops = {
        flush, move_to, load_bank, no_inverse, music_feed, shape, set_palette, nothing, styles, poll_pointer, hide,
        in_rect, select_picture, select_default, blit, blit_masked, blit_word, style, shadowed, nothing, nothing, flip,
        show, mark_at, buttons, track_music, fade_in, track_play, deadline_set, sound, ticks, activate, key, click,
        track_stop, track_free, fade_out, free_bank, free_handle_inverse, best_times, image_car_flags, off, music,
        deadline_passed, music_fade_out, music_end, &p };
    track_select_screen(&in, &st, &p, &ops);
    RaceSettings after = before;
    if (config_dat_get_race(config, &after)) settings_put(&after);
}

/* 0045cea0's text: 00463d34 as text_fit.c cuts it, 00466d5c / 00466e5c / 00466c9c as stat_text.c writes them */
static int32_t fit_width(void *ctx, const char *text) { (void)ctx; return text_width(text); }
static void fit_ramp(void *ctx, int shadow) { (void)ctx; text_ramp(shadow); }
static void fit_draw(void *ctx, const char *text, int32_t x, int32_t y) { (void)ctx; text_draw(text, x, y); }
static void fit(void *ctx, const char *text, int32_t x, int32_t y, int32_t width)
{
    (void)ctx;
    static const TextFitOps ops = { fit_width, fit_ramp, fit_draw, NULL };
    text_fit_shadowed(text, x, y, width, &ops);
}
static void time_text(void *ctx, char *out, int32_t time) { (void)ctx; stat_time_text(out, time); }
static void average_text(void *ctx, char *out, int32_t time, int32_t distance)
{
    (void)ctx;
    stat_average_speed_text(out, resource_german(), time, distance);
}
static void speed_text(void *ctx, char *out, int32_t speed) { (void)ctx; stat_speed_text(out, resource_german(), speed); }

/* 004676d8 from the track screen on config.dat's image and its records; ctx is the track screen's pointer */
static int32_t best_times(void *ctx)
{
    uint8_t *const image = config_current()->image;
    const int32_t screen = ea_screen();
    const BestTimesInputs in = { "", resource_german(), g.lost_vegas, screen, ea_picture_shape(screen),
                                 image + (0x524f8f - 0x524df0) };
    BestTimesState st = { image + (0x524df9 - 0x524df0), g.inverse, screen_palette };
    const BestTimesOps ops = {
        move_to, load_art, no_inverse, music_feed, shape, flush, set_palette, nothing, styles, poll_pointer, mark_at,
        in_rect, select_picture, blit, blit_masked, blit_word, style, fit, time_text, speed_text, shadowed,
        select_default, hide, nothing, nothing, flip, show, buttons, fade_in, deadline_set, sound, ticks, activate,
        key, click, image_car_flags, fade_out, free_handle_inverse, free_bank, load_bank, off, music,
        deadline_passed, music_fade_out, music_end, ctx };
    return best_times_screen(&in, &st, ctx, &ops);
}
/* 004fa0cc..: the segments' top speeds and totals (menu_race fills them); 0051dccc and 0051dcec..: 0045b6c8's;
 * the shorts 0051ce24 + 0x1d2 x player, the cars' points (TODO(0045aec8): none given yet) */
static int32_t race_times[MENU_RACE_TIMES];
static int32_t finish[GAME_STATE_PLAYERS];
static TournamentOrderWork order_work;
static int16_t car_points[GAME_STATE_PLAYERS];

/* 0045bad8, 0045be50, 0045c2d0 and 0045b908 on config.dat's image and the menus' game state, this machine's player
 * 0; 0045b73c, 0045b6c8 on the game state */
static int32_t records(void *ctx) { (void)ctx; return record_check(config_current()->image, game_state, 0); }
static int32_t totals(void *ctx) { (void)ctx; return record_check_totals(config_current()->image, game_state, 0, race_times); }
static int32_t top_record(void *ctx) { (void)ctx; return record_check_top(config_current()->image, game_state, 0); }
static int32_t lap_record(void *ctx) { (void)ctx; return record_check_lap(config_current()->image, game_state, 0); }
static int32_t leads(void *ctx) { (void)ctx; return tournament_leads(game_state, 0); }
static void results_order(void *ctx) { (void)ctx; tournament_results_order(game_state, finish, &order_work); }

/* the results screens' ctx: their pointer first (poll_pointer's), and what 0045c45c / 0046067c are shown with */
typedef struct ResultsCtx {
    DialogPointer p;
    const ResultsInputs *in;
    ResultsState *st;
    const ResultsOps *ops;
} ResultsCtx;
static void best_totals(void *ctx, int32_t row, int32_t top)
{
    ResultsCtx *const c = ctx;
    best_totals_screen(row, top, c->in, c->st, &c->p, c->ops);
}
static int32_t lap_times(void *ctx, int32_t record, int32_t tune)
{
    ResultsCtx *const c = ctx;
    return lap_times_screen(record, tune, c->in, c->st, &c->p, c->ops);
}

typedef void ResultsShow(int32_t ask, const ResultsInputs *in, ResultsState *st, DialogPointer *p,
                         const ResultsOps *ops);

/* 0045cea0, 0045de08 or 0045eff4 after a race, on the drive's results and config.dat's image, on 00427ae0's screen
 * picture */
static void results(ResultsShow *screen_of, const TrackViewerResults *r, int32_t mode)
{
    static const char *const CAR_NAMES[RESULTS_CARS] = { "RX-7", "NSX", "SUPRA", "911", "ZR-1", "VIPER",
                                                         "512TR", "DIABLO", "PACK", "CLOCK", "CLASS", "WARRIOR" };
    const int32_t screen = ea_screen();
    const ResultsInputs in = { game_state, config_current()->image, r->dropped, 0, r->place, r->rival_place,
                               r->rival, r->rival_car, resource_german(), 0, screen, ea_picture_shape(screen),
                               CAR_NAMES, g.lost_vegas, g.variant_r, r->player_place, r->shown_place, finish };
    ResultsState st = { g.inverse, screen_palette, 0 };
    int x, y;
    pointer_position(&x, &y);
    ResultsCtx c = { { pointer_active(), 0, x, y, x, y }, &in, &st, NULL };
    const ResultsOps ops = {
        records, flush, move_to, load_art, no_inverse, music_feed, shape, fade_in, styles, poll_pointer, in_rect,
        select_picture, blit, style, shadowed, fit, time_text, average_text, speed_text, set_palette, nothing,
        select_default, hide, nothing, blit_word, nothing, flip, show, mark_at, buttons, track_music, track_play,
        deadline_set, sound, ticks, activate, key, click, off, music, deadline_passed, track_stop, track_free, fade_out,
        free_bank, free_handle_inverse, blit_masked, totals, top_record, leads, best_totals, lap_record,
        results_order, lap_times, &c };
    c.ops = &ops;
    screen_of(mode, &in, &st, &c.p, &ops);
}

/* 004275f8 for 00427ae0: the drive on argv[1]'s track (its stem in capitals) in the menus' window, the options from
 * config.dat (track_viewer.h's TrackViewerMenus; Options' 0 is on, 1 off, 2 none); its end (0052b2fe = 00509d6c,
 * 0 when not over) and results kept for 0045cea0 */
static TrackViewerResults last_drive;
static void drive(void *ctx, int32_t argc, char *const *argv)
{
    (void)ctx, (void)argc;
    char name[8] = { 0 };
    for (size_t i = 0; i + 1 < sizeof name && argv[1][i] != 0; i++)
        name[i] = (char)(argv[1][i] >= 'a' && argv[1][i] <= 'z' ? argv[1][i] - 0x20 : argv[1][i]);
    const TrackViewerMenus menus = { image_short(0x97) == 0, image_short(0x99) == 0, image_short(0x8f) == 0, g.laps,
                                     g.opponent, image_short(0x73), image_short(0x75), image_short(0x79),
                                     image_short(0x71), game_state };
    menu_audio_close();  /* the drive opens the device itself (drive_audio_open); its pause menu uses the menus' layer */
    track_viewer_drive_menus(&menus);
    if (!track_viewer_drive(resource_root(), name, race_head_to_head_model(g.player_car), 1, g.race_type, 0, NULL))
        fprintf(stderr, "control_central: the race on %s could not start\n", name);
    menu_audio_open();
    last_drive = track_viewer_drive_results();
    const int16_t raced = last_drive.over ? last_drive.end_request : 0;
    memcpy(game_state + 0x5ba, &raced, sizeof raced);
}
static void segment_results(void *ctx, int32_t mode) { (void)ctx; results(results_screen, &last_drive, mode); }
static void track_results(void *ctx, int32_t ask) { (void)ctx; results(track_results_screen, &last_drive, ask); }
static void circuit_results(void *ctx, int32_t ask) { (void)ctx; results(circuit_results_screen, &last_drive, ask); }

/* 00461540 on the menus' game state and 0045b6c8's finishers */
static void fade_out_bank(void *ctx, int32_t first, int32_t count, int32_t bank) { (void)bank; fade_out(ctx, first, count); }
static void standings(void *ctx, int32_t segment)
{
    (void)ctx;
    const int32_t screen = ea_screen();
    const StandingsInputs in = { game_state, finish, car_points, image_short(0x95), image_short(0x93),
                                 image_short(0xb1), resource_german(), g.unlock_level, 0, screen,
                                 ea_picture_shape(screen) };
    StandingsState st = { g.inverse, screen_palette };
    int x, y;
    pointer_position(&x, &y);
    DialogPointer p = { pointer_active(), 0, x, y, x, y };
    const StandingsOps ops = {
        flush, move_to, load_art, no_inverse, music_feed, shape, set_palette, set_palette, nothing, styles,
        poll_pointer, hide, nothing, in_rect, select_picture, select_default, blit, blit_masked, blit_word, style,
        glue_boxed, time_text, results_order, nothing, nothing, flip, show, mark_at, buttons, fade_in,
        fade_out_bank, deadline_set, sound, ticks, activate, key, click, off, music, deadline_passed, free_bank,
        free_handle_inverse, &p };
    standings_screen(segment, &in, &st, &p, &ops);
}
static void clip(void *ctx, int32_t x0, int32_t y0, int32_t x1, int32_t y1) { (void)ctx; ea_clip(x0, y0, x1, y1); }
static void no_two(void *ctx, int32_t a, int32_t b) { (void)ctx, (void)a, (void)b; }
static void no_three(void *ctx, int32_t a, int32_t b, int32_t c) { (void)ctx, (void)a, (void)b, (void)c; }
static int32_t no_answer(void *ctx, int32_t mode) { (void)ctx, (void)mode; return 0; }
static int32_t next_segment(void *ctx) { (void)ctx; return 1; }  /* TODO(0040b648): the screen between segments */
/* 0047310c for 00427ae0's screen: the last one freed first (the original leaves it) */
static int32_t race_picture(void *ctx, int32_t w, int32_t h, int32_t flags)
{
    (void)ctx, (void)flags;
    int32_t *const slot = ea_screen_slot();
    if (*slot != 0) ea_picture_free(*slot);
    return *slot = ea_picture_new(w, h);
}
static void reset_tournament(void *ctx)
{
    (void)ctx;
    RaceSettings s = settings_of();
    tournament_reset(game_state, &s);
    settings_put(&s);
}
static void game_reset(void *ctx) { (void)ctx; glue_game_reset(); }

/* 00427ae0 (menu_race.c) on config.dat's image and the menus' game state, this machine's player 0, no link game.
 * TODO(0045aec8, 0040c710, 0040b648, 004283c4, 00428610, 0045ae80, 00432e48): the tournament's points, the
 * winner's and the ticket's screens, the tournament's end */
static void race(void *ctx)
{
    (void)ctx;
    ConfigDat *const config = config_current();
    const RaceSettings before = settings_of();
    config_dat_set_race(config, &before);
    ea_select_default();
    *ea_screen_slot() = 0;  /* Control Central freed it */
    static int32_t ghost[MENU_RACE_GHOST];
    static const int16_t no_link = 0;
    static const int32_t player = 0, font = 0, host = 0;
    int16_t net_off = 0, link_done = 0;
    int32_t warrior = g.warrior;
    const MenuRaceState st = { config->image, game_state, &net_off, race_times, ghost, &no_link, &no_link,
                               &g.pointer_active, &player, &font, ea_screen_slot(), &warrior, &g.unlock_m,
                               &g.unlock_r, &g.lost_vegas, &host, &link_done };
    const MenuRaceOps ops = {
        no_mode, hide, nothing, clip, nothing, drive, no_mode, nothing, set_palette, no_two, no_three, no_mode,
        nothing, music_pick, nothing, nothing, sound_load, race_picture, circuit_results, segment_results, standings, track_results,
        nothing, nothing, next_segment, reset_tournament, no_answer, nothing, game_reset, ctx };
    menu_race(&st, &ops);
    g.warrior = (int16_t)warrior;
    int32_t *const slot = ea_screen_slot();
    if (*slot != 0) ea_picture_free(*slot);  /* Control Central makes its own next */
    *slot = 0;
    RaceSettings after = before;
    if (config_dat_get_race(config, &after)) settings_put(&after);
    g.unlock_level = (int16_t)config_dat_unlock_level(config);
}

/* 004bb8dc: counted up by the timer (0043b9e8); here by the ticks gone between two of 004166e0's music calls */
static int32_t credits_idle;
static uint32_t credits_last;
static void credits_tick(void *ctx)
{
    (void)ctx;
    const uint32_t now = timer_ticks();
    credits_idle += (int32_t)(now - credits_last);
    credits_last = now;
    menu_audio_music_service();
}
static int32_t read_buttons(void *ctx) { (void)ctx; pointer_sync_from_os(); return pointer_read_buttons(); }
static void blit_at(void *ctx, int32_t s, int32_t x, int32_t y) { (void)ctx; ea_restore(s, x, y); }
static void poll(void *ctx) { (void)ctx; pointer_sync_from_os(); }

static const int16_t no_quit;  /* 0050a5b6: nothing asks the screens to leave yet */

/* 004166e0 from Options */
static void credits(void *ctx, int32_t mode)
{
    (void)mode;
    const CreditsOps ops = { load_art, shape, palette_set, music_feed, select_picture, blit, select_default,
                             picture_shape, blit_at, nothing, flip, fade_in, credits_tick, read_buttons, key, fade_out,
                             free_handle_inverse, music_fade_out, music_end, free_bank, mark, ctx };
    const CreditsState st = { ea_screen_slot(), &g.pointer_active, &g.buttons, &no_quit, &credits_idle };
    credits_last = timer_ticks();
    credits_show(&st, &ops);
}

/* 00444bb0's shorts in config.dat's image (offsets from 00524df0), all at odd addresses: copied out to aligned
 * shorts around the screen */
enum { OPT_TRANSMISSION, OPT_AUTOMATIC, OPT_ABS, OPT_TRACTION, OPT_RACE_TYPE, OPT_TRACK, OPT_OPPONENT, OPT_CAR,
       OPT_NET_CAR, OPT_LAPS, OPT_DIFFICULTY, OPT_MUSIC_VOLUME, OPT_SHORTS };
static const uint16_t OPT_AT[OPT_SHORTS] = { 0x97, 0x8d, 0x99, 0x8f, 0x6d, 0x95, 0x91, 0x9b, 0xad, 0xb1, 0xb3, 0x77 };
static int16_t opt[OPT_SHORTS];

static void opt_out(void)
{
    const ConfigDat *const config = config_current();
    for (int i = 0; i < OPT_SHORTS; i++) memcpy(&opt[i], config->image + OPT_AT[i], sizeof opt[i]);
}

static void opt_in(void)
{
    ConfigDat *const config = config_current();
    for (int i = 0; i < OPT_SHORTS; i++) memcpy(config->image + OPT_AT[i], &opt[i], sizeof opt[i]);
}

/* 00465ec8 on the screen's shorts */
static void opt_car_flags(void *ctx)
{
    opt_in();
    image_car_flags(ctx);
    opt_out();
}

/* 00413cb0's controls: config.dat's 13 shorts from 0052ad26, copied to aligned shorts around the screen */
enum { CONTROLS_AT = 0x5f36 };
static int16_t controls_kept[CONTROLS_SHORTS];
static void controls_out(void) { memcpy(controls_kept, config_current()->image + CONTROLS_AT, sizeof controls_kept); }
static void controls_in(void) { memcpy(config_current()->image + CONTROLS_AT, controls_kept, sizeof controls_kept); }
static void grab(void *ctx, int32_t s, int32_t x, int32_t y) { (void)ctx; ea_save(s, x, y); }
static void key_name(void *ctx, int32_t k, char *out) { (void)ctx; key_name_text(out, resource_german(), k); }
static int32_t clash(void *ctx)
{
    (void)ctx;
    controls_in();
    return config_dat_controls_clash(config_current());
}
/* 00476f94: the arrows from the key queue, anything else typed from the text queue (Enter, Escape among them) */
static int32_t key_read(void *ctx)
{
    (void)ctx;
    static const int32_t ARROWS[] = { [PLATFORM_KEY_LEFT] = 0x4b00, [PLATFORM_KEY_RIGHT] = 0x4d00,
                                      [PLATFORM_KEY_UP] = 0x4800, [PLATFORM_KEY_DOWN] = 0x5000 };
    for (PlatformKey k; (k = platform_next_key()) != PLATFORM_KEY_NONE;)
        if (k <= PLATFORM_KEY_DOWN) return ARROWS[k];
    return input_next_char();
}
static int32_t keys_pending(void *ctx) { (void)ctx; return input_get_key(); }
static int32_t capture_buttons(void *ctx) { (void)ctx; pointer_sync_from_os(); return pointer_read_buttons(); }
static int32_t capture(void *ctx, char *name)
{
    KeyCaptureState st = { pointer_active(), 0, 0, 0, 0 };
    const KeyCaptureOps ops = { keys_pending, music, capture_buttons, nothing, ticks, glue_drain, key_read, nothing,
                                ctx };
    return key_capture(name, resource_german(), &st, &ops);
}
static void controls(void *ctx, int32_t mode)
{
    static const int16_t no_joystick_b = 0, quit = 0, net_off = 1;  /* no joysticks, no link game */
    static const int32_t no_joystick_a = 0, no_calibration = 0;
    const int16_t german = (int16_t)resource_german();
    int32_t *const screen = ea_screen_slot();
    controls_out();
    const ControlsMenuState st = { controls_kept, &german, &no_joystick_a, &no_joystick_b, &no_calibration,
                                   &g.pointer_active, &g.buttons, screen, &g.inverse, &net_off, &quit };
    const ControlsMenuOps ops = {
        picture_new, flush, palette_clear, move_to, load_art, no_inverse, music_feed, shape, palette_set, fade_in,
        styles, poll, mark, in_rect, style, select_picture, blit, picture_shape, grab, key_name, shadowed, set_palette,
        select_default, flip, show, deadline_set, sound, nothing, blit_at, ticks, buttons, activate, key, click, clash,
        glue_boxed, capture, deadline_passed, music, fade_out, free_handle_inverse, no_mode, off, nothing,
        picture_free, free_bank, ctx };
    controls_menu(&st, mode, &ops);
    controls_in();
}

/* 004015dc's pointer x (004c6110), read as it drags a slider: kept by its button and move polls */
static int16_t audio_pointer_x;
static void audio_track(void)
{
    int x, y;
    pointer_position(&x, &y);
    audio_pointer_x = (int16_t)x;
}
static int32_t audio_buttons(void *ctx) { (void)ctx; audio_track(); return pointer_read_buttons(); }
static void audio_activate(void *ctx) { (void)ctx; audio_track(); pointer_activate_if_moved(); }
static int32_t audio_clock(void *ctx) { (void)ctx; return (int32_t)timer_ticks(); }
static void audio_volume(void *ctx, int32_t volume) { (void)ctx; menu_audio_music_fade_to(volume); }
static void audio_no_volume(void *ctx, int32_t volume) { (void)ctx, (void)volume; }
static void audio_no_voice(void *ctx, int32_t voice, int32_t volume) { (void)ctx, (void)voice, (void)volume; }
static int32_t audio_effects(void *ctx) { (void)ctx; return image_short(0x79); }
static void audio_blit_at(void *ctx, const uint8_t *s, int32_t x, int32_t y) { (void)ctx; ea_blit_at(s, x, y); }
static void audio_no_clip(void *ctx, int32_t l, int32_t r, int32_t t, int32_t b) { (void)ctx, (void)l, (void)r, (void)t, (void)b; }

/* 004015dc from Options (mode 0) on config.dat's image: no sound card modes, so neither the mode nor 3D sound is
 * offered; the effects' volume, the speech and the voices' volumes are not heard yet, nor is the song tried
 * (00456244) or the knobs clipped (0047383c); the test sound 0x4a plays once */
static void audio(void *ctx, int32_t mode)
{
    static int16_t track_silent, small_stream, kept[4];
    static const int16_t no_modes = 1, no_bit0 = 0, sound_alt = 0, sound_on = 1, net_off = 1;
    static const int32_t voice_4a = 0, voice = 0;
    const int16_t german = (int16_t)resource_german();
    audio_track();
    opt_in();
    const AudioMenuState st = {
        config_current()->image + 9, &track_silent, kept, &no_modes, &no_bit0, &sound_alt, &menu_words.link_game,
        &german, &sound_on, &voice_4a, &voice, &audio_pointer_x, &g.pointer_active, &g.buttons, ea_screen_slot(),
        &g.inverse, &small_stream, &net_off, &no_quit };
    const AudioMenuOps ops = {
        music_fade_out, music_end, nothing, picture_new, move_to, load_art, no_inverse, music_feed, shape,
        palette_set, poll, mark, audio_clock, nothing, zero, sound, in_rect, audio_volume, select_picture, blit,
        blit_at, audio_blit_at, audio_no_clip, picture_shape, select_default, hide, nothing, flip, show,
        audio_buttons, styles, fade_in, deadline_set, deadline_passed, nothing, music, nothing, blit_masked, ticks,
        audio_activate, key, key, click, music_end, audio_no_voice, audio_effects, music_fade_to, audio_no_volume,
        glue_banner, sound_free, nothing, sound_load, off, nothing, fade_out, free_handle_inverse, picture_free,
        free_bank, music_pick, music_start, ctx };
    audio_menu(&st, mode, &ops);
    opt_out();
}

/* 00423d28 from Options (mode 0, the resolution offered) on config.dat's image; the race's view does not read its
 * window, detail, distance or horizon yet */
static void graphics(void *ctx, int32_t mode)
{
    static int16_t showing;  /* 004f9e8c */
    static const int16_t net_off = 1;
    opt_in();
    const GraphicsMenuState st = { config_current()->image + 9, &showing, &net_off, &g.pointer_active, &g.buttons,
                                   ea_screen_slot(), &g.inverse, &no_quit };
    const GraphicsMenuOps ops = {
        move_to, load_art, no_inverse, music_feed, shape, palette_set, poll, mark, in_rect, select_picture, blit,
        picture_shape, select_default, hide, nothing, flip, show, buttons, styles, fade_in, deadline_set, sound,
        nothing, ticks, activate, key, click, off, music, nothing, deadline_passed, fade_out, free_handle_inverse,
        free_bank, ctx };
    graphics_menu(&st, mode, &ops);
    opt_out();
}

/* 0042ae9c's and 0042c498's words: the shorts 004fa9ac (check/'s art) and 004bad00 (a highlight), the names
 * 004fa1dc, the game state kept 00524838, 004fbad8's bytes kept around a replay */
static int16_t list_check, list_highlight;
static uint8_t list_names[(GAME_LIST_NAMES + 1) * FILE_LIST_NAME];
static uint8_t game_kept[GAME_LIST_STATE];
static uint8_t replay_race[LOAD_SAVE_RACE];

/* 0042ae9c's calls on the player's directory (save_files.h): no directories to change into */
static void no_cwd(void *ctx, char *out) { (void)ctx; out[0] = 0; }
static void no_chdir(void *ctx, const char *path) { (void)ctx, (void)path; }
static const char *find_first(void *ctx, const char *pattern) { (void)ctx; return save_files_first(pattern); }
static const char *find_next(void *ctx) { (void)ctx; return save_files_next(); }
static void masked_at(void *ctx, const uint8_t *s, int32_t x, int32_t y) { (void)ctx; ea_blit_masked_at(s, x, y); }
static void remove_save(void *ctx, const char *path) { (void)ctx; save_files_remove(path); }
static void game_load(void *ctx, const char *name) { (void)ctx; game_file_load(name, game_state, &save_files_ops); }
static void redraw_pointer(void *ctx) { (void)ctx; pointer_show(); }

/* 0042ae9c on config.dat's image and the menus' game state */
static int32_t game_list(void *ctx, int32_t mode)
{
    uint8_t *const image = config_current()->image;
    GameListState st = { resource_german(), list_check, list_highlight, ea_screen(), "", "", list_names,
                         &g.pointer_active, &g.buttons, image + 9, (char *)image + 0x3b, game_state, game_kept,
                         image + CONTROLS_AT, &g.lost_vegas, &g.tournament };
    const GameListOps ops = {
        styles, picture_new, picture_shape, select_picture, grab, load_art, music_feed, no_cwd, no_chdir, find_first,
        find_next, clip, move_to, ticks, in_rect, sound, deadline_set, shape, audio_blit_at, style, fit, masked_at,
        select_default, nothing, blit_word, blit_at, nothing, flip, show, deadline_passed, nothing, buttons,
        activate, music, key, click, glue_yes_no, remove_save, glue_banner_wait, game_load, music_fade_to,
        redraw_pointer, off, blit_masked, picture_free, free_bank, ctx };
    return game_list_screen(mode, &st, &ops);
}
static void save_box(void *ctx, char *name)
{
    (void)ctx;
    glue_save_box(name, config_current()->image, game_state, (int16_t)g.lost_vegas);
}
static void replay_box(void *ctx, char *name) { (void)ctx; glue_replay_box(name, config_current()->image); }
static void clear_one(void *ctx) { (void)ctx; ea_select_default(), video_clear_back(), video_flip(); }
static void no_replay(void *ctx, int32_t mode, const char *path) { (void)ctx, (void)mode, (void)path; }
static void no_mode_set(void *ctx, int32_t w, int32_t h, int32_t bits) { (void)ctx, (void)w, (void)h, (void)bits; }

/* 0042c498 from Options (mode 0) on config.dat's image, its shorts copied around it to aligned ones; no replay
 * file is listed yet (TODO(0042a5e8, 004275f8): replays saved and played from here) */
static void load_save(void *ctx, int32_t mode)
{
    ConfigDat *const config = config_current();
    opt_in();
    int16_t variant = image_short(0x93), replay_flag = image_short(0xaf);
    LoadSaveState st = {
        resource_german(), g.tournament, menu_words.link_game, image_short(0x6d), g.pointer_active, &g.buttons,
        ea_screen_slot(), &g.inverse, &list_check, &variant, &list_highlight, &replay_flag, replay_race,
        &g.music_silent, g.volumes, { g.volumes[0], g.volumes[1], g.volumes[2] }, image_short(0x7f),
        (const char *)config->image + 9, (const char *)config->image + 0x3b };
    const LoadSaveOps ops = {
        move_to, load_art, no_inverse, music_feed, palette_clear, shape, palette_set, poll, mark, in_rect,
        select_picture, blit, select_default, hide, nothing, picture_shape, nothing, flip, show, buttons, fade_in,
        styles, deadline_set, sound, ticks, activate, key, click, game_list, save_box, replay_box,
        free_handle_inverse, free_bank, music_fade_out, fade_out, music_end, picture_free, sound_free, clear_one,
        clip, no_replay, nothing, set_palette, no_mode_set, picture_new, sound_load, music_pick, off, music,
        deadline_passed, ctx };
    load_save_menu(mode, &st, &ops);
    memcpy(config->image + 0x93, &variant, sizeof variant), memcpy(config->image + 0xaf, &replay_flag, sizeof replay_flag);
    opt_out();
    g.unlock_level = (int16_t)config_dat_unlock_level(config);
}

static void options(void *ctx)
{
    ConfigDat *const config = config_current();
    const RaceSettings before = settings_of();
    config_dat_set_race(config, &before);
    opt_out();
    const OptionsMenuOps ops = {
        move_to, load_art, no_inverse, music_feed, shape, palette_set, styles, poll, mark, in_rect, select_picture,
        blit, select_default, hide, nothing, picture_shape, nothing, flip, show, buttons, fade_in, deadline_set, sound,
        ticks, activate, key, click, fade_out, free_handle_inverse, free_bank, audio, graphics, controls, load_save,
        music_fade_out, music_end, music_start, credits, music_pick, opt_car_flags, music_fade_to, off, music, nothing,
        deadline_passed, ctx };
    static const int16_t net_off = 1;  /* 0052b302: no link game to keep in step */
    OptionsMenuState st = {
        &opt[OPT_TRANSMISSION], &opt[OPT_AUTOMATIC], &opt[OPT_ABS], &opt[OPT_TRACTION], &opt[OPT_RACE_TYPE],
        &opt[OPT_TRACK], &opt[OPT_OPPONENT], &opt[OPT_CAR], &opt[OPT_NET_CAR], &opt[OPT_LAPS], &opt[OPT_DIFFICULTY],
        &opt[OPT_MUSIC_VOLUME], &menu_words.link_game, &g.pointer_active, &g.buttons, ea_screen_slot(), &g.inverse,
        &net_off, &no_quit, &menu_words.redraw };
    options_menu(&st, &ops);
    opt_in();
    RaceSettings after = before;
    if (config_dat_get_race(config, &after)) settings_put(&after);
}

/* config.dat's race settings and unlock level into the globals, as the draft read them */
static void load(void)
{
    ConfigDat *const config = config_current();
    RaceSettings s;
    if (!config_dat_get_race(config, &s)) {
        ConfigDat defaults;
        config_dat_defaults(&defaults, resource_german());
        config_dat_get_race(&defaults, &s);
    }
    settings_put(&s);
    g.unlock_level = (int16_t)config_dat_unlock_level(config);
    g.pointer_active = (int16_t)pointer_active();
}

/* 00466ae4 before Control Central: the game state for a new tournament (the controls, the names: config.dat's for
 * player 0, the computer players' for the rest), the race settings read from config.dat and put back to it */
void glue_game_reset(void)
{
    load();
    RaceSettings s = settings_of();
    FrontendSession f = session_of();
    tournament_new(game_state, &s, config_current(), &f);
    settings_put(&s);
    session_put(&f);
    config_dat_set_race(config_current(), &s);
}

/* 00408724 on Control Central's words */
static void showcase(void *ctx)
{
    const ShowcaseGlue in = { &g.player_car, &g.race_type, &g.track, &g.pointer_active, &g.buttons, car_flags, ctx };
    glue_showcase(&in);
}

int32_t glue_central(void)
{
    static const ControlCentralOps OPS = {
        no_wait, palette_clear, move_to, music, load_art, picture_new, sound_load, no_inverse, music_feed, mark, shape,
        palette_set, nothing, in_rect, car_flags, type_changed, ticks, zero, select_picture, blit, blit_masked,
        blit_word, picture_shape, width, style, shadowed, frame, free_bank, load_bank, text_at, select_default, flip,
        show, buttons, fade_in, nothing, styles, sound, deadline_set, deadline_passed, key, scancode, no_cheat,
        not_down, activate, zero, click, no_kick, music_fade_out, fade_out, music_end, music_start, showcase,
        music_pick, step_race_type, track_forward, track_backward, nothing, no_mode, track_select, art_free, options,
        nothing, glue_yes_no, glue_drain, glue_banner, nothing, sound_free, picture_free, flush, race, off, NULL };
    load();
    ControlCentralState st = {
        &g.race_type, &g.opponent, &g.track, &g.player_car, g.other_car, &g.net_car, &g.laps, g.difficulty,
        menu_words.mode_menu, g.volumes, menu_words.link_game, 0, &menu_words.redraw, (int16_t)resource_german(), 0,
        &g.tournament, &menu_words.link_lost, g.races, { 0, 0, 0 }, (uint32_t *)&menu_cheats.keys_506,
        &g.unlock_level, &g.unlock_m, &g.unlock_r, &g.lost_vegas, &g.vegas_cheat, &g.net_player, &g.net_player_was,
        g.was, g.art, &g.variant_m, &g.variant_r, menu_words.single, menu_words.intro_seen, &g.nomercy, &g.warrior,
        &g.explode, &g.music_silent, &g.inverse, &g.nitro, &g.net_hover, &g.net_self, &g.net_second, &g.letter_held,
        0, "", &g.pointer_active, &g.buttons, ea_screen_slot(), "", keys };
    const int32_t result = control_central(&st, &OPS);
    video_set_overlay(NULL, 0, 0);
    const RaceSettings s = settings_of();
    config_dat_set_race(config_current(), &s);
    return result;
}
