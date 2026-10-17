/* 00432ff4: the checked Control Central (central.c) over game/sys and ea_lib (glue, no oracle). Its globals are
 * shorts here, filled from config.dat's race settings on entry and put back on leaving; the race settings' checked
 * steps (race_settings.c, tournament.c) run on a RaceSettings made from them. */
#include "game/frontend/menu_glue.h"

#include "game/frontend/central.h"
#include "game/frontend/config.h"
#include "game/frontend/config_dat.h"
#include "game/frontend/control_central_art.h"
#include "game/frontend/ea_lib.h"
#include "game/frontend/palette_put.h"
#include "game/frontend/race_settings.h"
#include "game/frontend/tournament.h"
#include "game/race/race_models.h"
#include "game/sys/input.h"
#include "game/sys/pointer.h"
#include "game/sys/resource.h"
#include "game/sys/text.h"
#include "game/sys/timer.h"
#include "game/sys/video.h"
#include "game/viewer/track_viewer.h"
#include "platform/platform.h"

#include <stdio.h>
#include <stdlib.h>

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

/* TODO(004648dc, 0046492c, 004649c8, 004640d0, 0046417c, 00464428, 004644f0, 00464568): the menu sounds and music;
 * TODO(00408724, 004562e8, 004687d0, 00461540, 00444bb0, 0040a5d8): the showcase, the track slides, the standings,
 * Options and chat; TODO(00446284): the race's setup; TODO(004104d0, 004109b4, 0044633c, 00432e90): the link
 * game; TODO(00437258, 00483f8c): the cheats need a key-down table on game/sys */
static void no_wait(void *ctx, int32_t v) { (void)ctx, (void)v; }
static void no_sound(void *ctx, int32_t sound) { (void)ctx, (void)sound; }
static void no_music(void *ctx, const char *name) { (void)ctx, (void)name; }
static void no_mode(void *ctx, int32_t mode) { (void)ctx, (void)mode; }
static void no_kick(void *ctx, int32_t player) { (void)ctx, (void)player; }
static int32_t zero(void *ctx) { (void)ctx; return 0; }
static int32_t no_inverse(void *ctx, const char *name) { (void)ctx, (void)name; return 0; }
static int32_t no_cheat(void *ctx, int which) { (void)ctx, (void)which; return 0; }
static int32_t not_down(void *ctx, int32_t scancode) { (void)ctx, (void)scancode; return 0; }
static int32_t scancode(void *ctx, int32_t c) { (void)ctx; return c & 0xff; }

/* 00427ae0: the race, as the build drives one (track_viewer_drive, its own window): the track's stem from 00426660's
 * table (an open road's first segment, the city's by 00524e83), the car's model by 00426fec, the race type's field */
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
    const PlatformConfig config = { VIDEO_WIDTH, VIDEO_HEIGHT, "The Need for Speed SE" };
    ea_select_default();
    platform_shutdown();
    if (!track_viewer_drive(resource_root(), name, model, 1, g.race_type, 0, NULL))
        fprintf(stderr, "control_central: the race on %s could not start\n", name);
    if (!platform_init(&config)) {
        fprintf(stderr, "control_central: the window could not be opened again\n");
        exit(1);
    }
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
        no_wait, palette_clear, move_to, nothing, load_art, picture_new, nothing, no_inverse, nothing, mark, shape,
        palette_set, nothing, in_rect, car_flags, type_changed, ticks, zero, select_picture, blit, blit_masked,
        blit_word, picture_shape, width, style, shadowed, frame, free_bank, load_bank, text_at, select_default, flip,
        show, buttons, fade_in, nothing, styles, no_sound, deadline_set, deadline_passed, key, scancode, no_cheat,
        not_down, activate, zero, click, no_kick, nothing, fade_out, nothing, no_music, nothing, nothing,
        step_race_type, track_forward, track_backward, nothing, no_mode, nothing, art_free, nothing, nothing,
        glue_yes_no, glue_drain, glue_banner, nothing, nothing, picture_free, flush, race, off, NULL };
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
