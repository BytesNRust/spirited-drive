/* 00432ff4: the checked Control Central (central.c) over game/sys and ea_lib (glue, no oracle). Its globals are
 * shorts here, filled from config.dat's race settings on entry and put back on leaving; the race settings' checked
 * steps (race_settings.c, tournament.c) run on a RaceSettings made from them. */
#include "game/frontend/menu_glue.h"

#include "game/frontend/central.h"
#include "game/frontend/config.h"
#include "game/frontend/config_dat.h"
#include "game/frontend/control_central_art.h"
#include "game/frontend/controls_menu.h"
#include "game/frontend/credits.h"
#include "game/frontend/ea_lib.h"
#include "game/frontend/key_capture.h"
#include "game/frontend/key_name.h"
#include "game/frontend/menu_audio.h"
#include "game/frontend/palette_put.h"
#include "game/frontend/options_menu.h"
#include "game/frontend/race_settings.h"
#include "game/frontend/results_screen.h"
#include "game/frontend/stat_text.h"
#include "game/frontend/text_fit.h"
#include "game/frontend/tournament.h"
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

/* TODO(00464788, 0046486c, 00464850, 004648a8): the track screen's own music (EA's player, 00492xxx);
 * TODO(00408724, 004676d8, 00461540, 0040a5d8): the showcase, the best times, the standings (no tournament results
 * yet) and chat; TODO(00446284): the race's setup; TODO(004104d0, 004109b4, 0044633c, 00432e90): the link
 * game; TODO(00437258, 00483f8c): the cheats need a key-down table on game/sys */
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

/* 00427ae0: the race, as the build drives one (track_viewer_drive in the menus' window): the track's stem from
 * 00426660's table (an open road's first segment, the city's by 00524e83), the car's model by 00426fec, the race
 * type's field; the options from config.dat (track_viewer.h's TrackViewerMenus; Options' 0 is on, 1 off, 2 none).
 * After a finished race (0052b2fe, 00509d6c, 1) its results screen (0045cea0).
 * TODO(00427ae0, 004275f8): the difficulty (00524ea3); the records (0045bad8) and the other screens after the race
 * (00461540 standings, 0045de08, 0045eff4) */
static void results(const TrackViewerResults *r);
static void race(void *ctx)
{
    (void)ctx;
    static const char *const STEMS[9] = { "CY", "CL", "AL", "TR1", "TR2", "TR6", "TR3", "TR7", "TR4" };
    if (g.track < 0 || g.track >= 9) {
        fprintf(stderr, "control_central: no race for track %d\n", g.track);
        return;
    }
    char name[8];
    if (g.track < 3) snprintf(name, sizeof name, "%s%d", STEMS[g.track], g.track == 0 ? g.variant + 1 : 1);
    else snprintf(name, sizeof name, "%s", STEMS[g.track]);
    const int model = race_head_to_head_model(g.player_car);
    const RaceSettings s = settings_of();
    config_dat_set_race(config_current(), &s);
    const TrackViewerMenus menus = { image_short(0x97) == 0, image_short(0x99) == 0, image_short(0x8f) == 0, g.laps,
                                     g.opponent, image_short(0x73), image_short(0x75), image_short(0x79),
                                     image_short(0x71), game_state };
    ea_select_default();
    menu_audio_close();  /* the drive opens the device itself (drive_audio_open); its pause menu uses the menus' layer */
    track_viewer_drive_menus(&menus);
    if (!track_viewer_drive(resource_root(), name, model, 1, g.race_type, 0, NULL))
        fprintf(stderr, "control_central: the race on %s could not start\n", name);
    menu_audio_open();
    menu_audio_bank_load();
    const TrackViewerResults r = track_viewer_drive_results();
    const int16_t raced = r.over ? r.end_request : 0;
    memcpy(game_state + 0x5ba, &raced, sizeof raced);  /* 0052b2fe = 00509d6c (004275f8) */
    if (raced == 1) results(&r);
}

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
static void no_name(void *ctx, const char *name) { (void)ctx, (void)name; }
static int32_t no_best_times(void *ctx) { (void)ctx; return 0; }
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
        show, mark_at, buttons, no_name, fade_in, nothing, deadline_set, sound, ticks, activate, key, click,
        nothing, nothing, fade_out, free_bank, free_handle_inverse, no_best_times, image_car_flags, off, music,
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
static int32_t no_records(void *ctx) { (void)ctx; return 0; }  /* TODO(0045bad8): the records beaten */

/* 0045cea0(1) after the race, on the race's results and config.dat's image */
static void results(const TrackViewerResults *r)
{
    static const char *const CAR_NAMES[RESULTS_CARS] = { "RX-7", "NSX", "SUPRA", "911", "ZR-1", "VIPER",
                                                         "512TR", "DIABLO", "PACK", "CLOCK", "CLASS", "WARRIOR" };
    int32_t *const slot = ea_screen_slot();
    const int32_t screen = *slot = ea_picture_new(0x280, 0x1e0);  /* 00427ae0's 0047310c(0x280, 0x1e0, 0) */
    const ResultsInputs in = { game_state, config_current()->image, r->dropped, 0, r->place, r->rival_place,
                               r->rival, r->rival_car, resource_german(), 0, screen, ea_picture_shape(screen),
                               CAR_NAMES };
    ResultsState st = { g.inverse, screen_palette, 0 };
    int x, y;
    pointer_position(&x, &y);
    DialogPointer p = { pointer_active(), 0, x, y, x, y };
    const ResultsOps ops = {
        no_records, flush, move_to, load_art, no_inverse, music_feed, shape, fade_in, styles, poll_pointer, in_rect,
        select_picture, blit, style, shadowed, fit, time_text, average_text, speed_text, set_palette, nothing,
        select_default, hide, nothing, blit_word, nothing, flip, show, mark_at, buttons, no_name, nothing,
        deadline_set, sound, ticks, activate, key, click, off, music, deadline_passed, nothing, nothing, fade_out,
        free_bank, free_handle_inverse, &p };
    results_screen(1, &in, &st, &p, &ops);
    ea_picture_free(screen);  /* Control Central makes its own next */
    *slot = 0;
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

/* TODO(004015dc, 00423d28, 0042c498): audio, graphics and load / save */
static void options(void *ctx)
{
    ConfigDat *const config = config_current();
    const RaceSettings before = settings_of();
    config_dat_set_race(config, &before);
    opt_out();
    const OptionsMenuOps ops = {
        move_to, load_art, no_inverse, music_feed, shape, palette_set, styles, poll, mark, in_rect, select_picture,
        blit, select_default, hide, nothing, picture_shape, nothing, flip, show, buttons, fade_in, deadline_set, sound,
        ticks, activate, key, click, fade_out, free_handle_inverse, free_bank, no_mode, no_mode, controls, no_mode,
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

int32_t glue_central(void)
{
    static const ControlCentralOps OPS = {
        no_wait, palette_clear, move_to, music, load_art, picture_new, sound_load, no_inverse, music_feed, mark, shape,
        palette_set, nothing, in_rect, car_flags, type_changed, ticks, zero, select_picture, blit, blit_masked,
        blit_word, picture_shape, width, style, shadowed, frame, free_bank, load_bank, text_at, select_default, flip,
        show, buttons, fade_in, nothing, styles, sound, deadline_set, deadline_passed, key, scancode, no_cheat,
        not_down, activate, zero, click, no_kick, music_fade_out, fade_out, music_end, music_start, nothing,
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
