/* 00432ff4: the checked Control Central (central.c) over game/sys and ea_lib (glue, no oracle). Its globals are
 * shorts here, filled from config.dat's race settings on entry and put back on leaving; the race settings' checked
 * steps (race_settings.c, tournament.c) run on a RaceSettings made from them. */
#include "game/frontend/menu_glue.h"

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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* the globals 00432ff4 writes (central.h gives each address) */
static struct {
    int16_t race_type, opponent, track, player_car, other_car[8], net_car, laps, difficulty;
    int16_t car_flag_a, car_flag_b, variant;
    int16_t volumes[3], tournament, unlock_level, races[9];
    int32_t unlock_m, unlock_r, lost_vegas, hidden, vegas_cheat, variant_m, variant_r;
    int16_t net_player, net_player_was, was[6];
    int32_t art[7];
    int16_t nomercy, warrior, net_hover, net_self, net_second, letter_held, buttons;
    int32_t explode, inverse, nitro;
} g = { .was = { 0x14, 0x14, 0x14, 0x14, 0x14, 0x14 } };  /* 004bafb4..'s data: no art shown yet */

enum { SEGMENT_AT = 0x93 };                         /* 00524e83: a stage's segment, 0..2 (00427ae0's run) */
enum { OPTIONS_AT = 0x81 };                        /* 00524e71: the five option words the race takes and gives back */
enum { MOVIES_AT = 0x5b8 };                        /* 0052b2fc: 2, the movies offered (the original's default) */
static uint8_t game_state[GAME_STATE_SIZE] = { [MOVIES_AT] = 2 };  /* 0052b2c0..: the tournament's */

/* 0052b2fc: the movies the game offers (the showcase's video page, the arrest's and the wins' movies); the port's
 * no_movies only skips the intro (intro_glue.c) */
int16_t glue_movies(void)
{
    return (int16_t)(game_state[MOVIES_AT] | game_state[MOVIES_AT + 1] << 8);
}
static uint8_t keys[0x100];                       /* 004c693c: by scancode, refreshed at each key (00464bd0) */
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

/* The short 0052ad40 is config.dat's (00524df0 + 0x5f50): the level a cheat raised (central_keys.c) back into the image
 * before a path that reads it from there, and before the image is saved (0042a504) */
static void level_to_image(void) { config_dat_set_unlock_level(config_current(), g.unlock_level); }

/* 0052b2d6..0052b2e8, the tournament's races by track that 00432ff4 sums: the menus' game state's (+0x592) */
static void races_from_state(void) { memcpy(g.races, game_state + 0x592, sizeof g.races); }

/* The short 0052b302: set while 00427ae0 runs a stage (menu_race.c), the link game's in a link game; the option
 * screens read it for their art and, while clear, keep a link game in step (004104d0) */
static int16_t solo_52b302;
static int16_t *word_52b302(void) { return menu_words.link_game != 0 ? &link_game.unknown_52b302 : &solo_52b302; }
static void net_sync(void *ctx) { (void)ctx, link_sync(); }

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

/* port: the extras the player left on, over the flags the original owns (central.h names each address) */
static void extras_flags(ExtrasFlags *f)
{
    f->unlock_m = &g.unlock_m;
    f->unlock_r = &g.unlock_r;
    f->variant_m = &g.variant_m;
    f->variant_r = &g.variant_r;
    f->explode = &g.explode;
    f->nitro = &g.nitro;
    f->vegas_cheat = &g.vegas_cheat;
    f->lost_vegas = &g.lost_vegas;
    f->warrior = &g.warrior;
    f->track_flags = (uint32_t *)&menu_cheats.keys_506;
}

/* port: the Warrior-as-rival switch is a separate flag (no equivalent in the original's globals), read by the
 * opponent arrows through ControlCentralState.warrior_rival. Kept up to date with extras.cfg by extras_apply. */
static ExtrasSession extras_session;
static int16_t warrior_rival_flag, warrior_keep_flag;   /* and the Warrior kept as the player's car */

/* port: called after every place the original clears the flags, so a switch stays on across races and the
 * tournament. With extras.cfg absent this reads the defaults and changes nothing. */
static void extras_apply(void)
{
    ExtrasFlags f;
    extras_flags(&f);
    const PortExtras e = port_extras_load();
    /* port: the level, the switches and the cheats typed in the session decide what is on: a switch turned off in
     * CUSTOMIZE takes its flag away, a cheat typed stays for the session and is never written to extras.cfg (the
     * switch is the player's default, not a veto on the cheat); Mirror and Rally both on stay both on */
    extras_settle(&e, g.unlock_level, &f, &extras_session);
    /* cheat_codes writes the globals, so they are read back and put into g again after the reapply */
    g.unlock_m = *f.unlock_m, g.unlock_r = *f.unlock_r, g.variant_m = *f.variant_m, g.variant_r = *f.variant_r;
    g.explode = *f.explode, g.nitro = *f.nitro, g.vegas_cheat = *f.vegas_cheat, g.lost_vegas = *f.lost_vegas;
    g.warrior = *f.warrior;
    menu_cheats.keys_506 = (int32_t)*f.track_flags;
    if (e.no_assists) extras_no_assists(&g.car_flag_a, &g.car_flag_b);   /* after 00465ec8 */
    warrior_rival_flag = (int16_t)(e.warrior_rival != 0);
    warrior_keep_flag = (int16_t)(e.warrior != 0);
}

/* 00465ec8, 00432974, 00432c40, 00432d48 and 00432720 on the shorts */
static void car_flags(void *ctx)
{
    (void)ctx;
    RaceSettings s = settings_of();
    race_settings_car_flags(&s);
    settings_put(&s);
    extras_apply();   /* port: after 00465ec8 / 00432974 */
}

static void type_changed(void *ctx)
{
    (void)ctx;
    RaceSettings s = settings_of();
    Unlocks u = unlocks_of();
    FrontendSession f = session_of();
    race_settings_type_changed(&s, &u, &f);
    settings_put(&s), unlocks_put(&u), session_put(&f);
    extras_apply();   /* port: the type change clears the variants */
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

static void no_wait(void *ctx, int32_t v) { (void)ctx, (void)v; }
/* the link game (link_central.h): 00475c14(0) runs its transport */
static void pump(void *ctx, int32_t v) { (void)ctx, (void)v, link_service(); }
/* Control Central holds 0052b316 and 004f81d2 as values, but in the original they are live words that 004104d0
 * changes (its 004465e4 rebuilds the seats as machines join): read again after each call while Central runs */
static ControlCentralState *central_state;
static void net_service(void *ctx)
{
    (void)ctx, link_sync();
    if (central_state != NULL) central_state->view_only = link_view_only(), central_state->players = link_humans();
}
static int32_t chat_waiting(void *ctx) { (void)ctx; return link_any_ready(); }
static int32_t net_start(void *ctx) { (void)ctx; return link_ready(); }
static void pre_race(void *ctx) { (void)ctx, link_host_ready(); }
static void kick(void *ctx, int32_t player) { (void)ctx, link_kick(player); }
static void sound(void *ctx, int32_t s) { (void)ctx; menu_audio_sound(s); }
static void sound_load(void *ctx) { (void)ctx; menu_audio_bank_load(); }
static void sound_free(void *ctx) { (void)ctx; menu_audio_bank_free(); }
static void music(void *ctx) { (void)ctx; menu_audio_music_service(); }
static void music_feed(void *ctx) { (void)ctx; menu_audio_music_feed(); }
static void music_start(void *ctx, const char *name) { (void)ctx; menu_audio_music_start(name); }
static void music_fade_out(void *ctx) { (void)ctx; menu_audio_music_fade_out(); }
static void music_end(void *ctx) { (void)ctx; menu_audio_music_end(); }
static void music_cut(void *ctx) { (void)ctx; menu_audio_music_cut(); }
static void music_fade_to(void *ctx, int32_t volume) { (void)ctx; menu_audio_music_fade_to(volume); }
static void music_pick(void *ctx) { (void)ctx; menu_audio_music_pick(); }
static void no_mode(void *ctx, int32_t mode) { (void)ctx, (void)mode; }
static int32_t no_inverse(void *ctx, const char *name) { (void)ctx, (void)name; return 0; }
/* 004baf44..: the codes' letters with 0x63 added, NUL after each; 004baf90..: each one's position, in
 * ControlCentral's order (00434d69..00434f93 pass them in pairs); LUNAR's (004baf7f, 004bafac) is not stepped there */
static const uint8_t CHEAT_LETTERS[] = {
    0xb6, 0xae, 0xa6, 0xa4, 0xb5, 0xb7, 0x00, 0xa8, 0xa4, 0xb5, 0xa4, 0xaf, 0xaf, 0xbc, 0xa8, 0x00, 0xa8, 0xbb, 0xb3,
    0xaf, 0xb2, 0xa7, 0xa8, 0x00, 0xa6, 0xa4, 0xb6, 0xac, 0xb1, 0xb2, 0xb9, 0xac, 0xaf, 0xaf, 0xa8, 0x00, 0xa6, 0xb2,
    0xb1, 0xa6, 0xa8, 0xb3, 0xb7, 0xa6, 0xa4, 0xb5, 0x00, 0xb7, 0xb8, 0xb5, 0xa5, 0xb2, 0x00, 0xb1, 0xac, 0xb7, 0xb5,
    0xb2, 0x00, 0xaf, 0xb8, 0xb1, 0xa4, 0xb5, 0x00, 0xb5, 0xb2, 0xa6, 0xae, 0xbc, 0xb5, 0xb2, 0xa4, 0xa7, 0x00
};
static const uint8_t CHEAT_AT[CENTRAL_CHEATS] = { 0x00, 0x07, 0x10, 0x35, 0x18, 0x24, 0x2f, 0x41 };  /* - 004baf44 */
static int32_t cheat_pos[CENTRAL_CHEATS];  /* 004baf90, 94, 98, a8, 9c, a0, a4, b0 */

static int32_t cheat_step(void *ctx, int which)
{
    (void)ctx;
    if (which < 0 || which >= CENTRAL_CHEATS) return 0;
    return cheat_code_step(&cheat_pos[which], CHEAT_LETTERS + CHEAT_AT[which], keys);
}

/* 00483f8c */
static int32_t key_down(void *ctx, int32_t scancode) { (void)ctx; return keys[(uint32_t)scancode & 0xff]; }
/* port: the screen palette's index nearest an RGB (ea_lib.c), for the car select's Color line's swatch */
static int32_t palette_colour(void *ctx, int32_t rgb)
{
    (void)ctx;
    return ea_colour(rgb);
}
/* port: the pad's virtual buttons this frame (control_map.h), for the car select's Color line */
static int32_t pad_buttons(void *ctx)
{
    uint32_t buttons = 0;
    int32_t axis[4] = { 0, 0, 0, 0 };
    (void)ctx;
    return platform_joy_read(0, &buttons, axis) ? (int32_t)buttons : 0;
}
static int32_t scancode(void *ctx, int32_t c) { (void)ctx; return key_code((uint16_t)c); }

/* config.dat's short at 00524df0 + at */
static int16_t image_short(int at)
{
    int16_t v;
    memcpy(&v, config_current()->image + at, sizeof v);
    return v;
}

/* config.dat's 00524e65 / 67 / 69 as g.volumes for the screens that put back the volumes F5 kept
 * (menu_audio_kept's +0, +4, +6) when 004c5cbe is set: before and after each, what a screen changed there goes
 * to config.dat, then g.volumes is read again (F5 itself writes config.dat) */
enum { VOLUMES_AT = 0x75 };
#define KEPT_VOLUMES { menu_audio_kept()[0], menu_audio_kept()[2], menu_audio_kept()[3] }
static int16_t volumes_read[3];
static void volumes_sync(void)
{
    uint8_t *const at = config_current()->image + VOLUMES_AT;
    if (memcmp(g.volumes, volumes_read, sizeof g.volumes) != 0) memcpy(at, g.volumes, sizeof g.volumes);
    memcpy(g.volumes, at, sizeof g.volumes);
    memcpy(volumes_read, g.volumes, sizeof volumes_read);
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
/* The short 0050a5b6 is the link's stage word (0050a5ac[5], race_net_on_stage): a client's becomes 1 when the host
 * reaches 00410c18(1), and every screen of the menus leaves on it. The original's service thread keeps it current; here
 * each screen's loop calls its deadline op first, which serves the link and, for the screens that took the word by
 * value in their inputs, writes it there */
static const int16_t *stage_word(void) { return &link_game.flags_a[RACE_NET_STAGE_FLAG]; }
static int *stage_in;  /* the running screen's inputs' copy of it (results_screen.h, standings.h) */
static int32_t deadline_passed(void *ctx)
{
    (void)ctx;
    if (menu_words.link_game != 0) {
        link_service();
        if (stage_in != NULL) *stage_in = *stage_word();
    }
    return timer_deadline_passed();
}
static int32_t key(void *ctx)
{
    (void)ctx;
    for (int code = 0; code < CHEAT_KEY_FLAGS; code++) keys[code] = platform_key_down(code) ? 1 : 0;
    return input_get_key();
}

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
/* the screens with a DialogPointer as ctx: its short 00524790 set as 00441798, 00464088 and 00464060 set the one word */
static void show_dialog(void *ctx) { pointer_show(), ((DialogPointer *)ctx)->active = pointer_active(); }
static void activate_dialog(void *ctx) { pointer_activate_if_moved(), ((DialogPointer *)ctx)->active = pointer_active(); }
static void off_dialog(void *ctx) { pointer_off(), ((DialogPointer *)ctx)->active = pointer_active(); }
static void set_palette(void *ctx, int32_t first, int32_t count, const uint8_t *rgb) { (void)ctx; video_set_palette(first, count, rgb); }
static void track_music(void *ctx, const char *name) { (void)ctx; menu_audio_track_music(name); }
static void track_play(void *ctx) { (void)ctx; menu_audio_track_play(); }
static void track_stop(void *ctx) { (void)ctx; menu_audio_track_stop(); }
static void track_free(void *ctx) { (void)ctx; menu_audio_track_free(); }
static void free_handle_inverse(void *ctx) { (void)ctx; }

/* 004fbae0: a dword per track; 00437540 sets the nine to 1 (midday) as the game starts (00481f27) */
static int32_t time_of_day[10] = { 1, 1, 1, 1, 1, 1, 1, 1, 1 };

/* 00465ec8 on config.dat's image, which 004687d0 changes */
static void image_car_flags(void *ctx)
{
    (void)ctx;
    ConfigDat *const config = config_current();
    RaceSettings s = settings_of();
    if (!config_dat_get_race(config, &s)) return;
    race_settings_car_flags(&s);
    if (port_extras_load().no_assists) {   /* port: no driving aids */
        int16_t abs = (int16_t)s.car_flag_a, traction = (int16_t)s.car_flag_b;
        extras_no_assists(&abs, &traction);
        s.car_flag_a = abs, s.car_flag_b = traction;
    }
    config_dat_set_race(config, &s);
}

/* 004687d0 on config.dat's image (00524df9..), the globals written to it first and read back after */
static int32_t best_times(void *ctx);
static void plus_text(void *ctx, int32_t style_id, const char *text, int32_t x, int32_t y);   /* port: the cheats item */
static int32_t plus_text_width(void *ctx, const char *text);
static void track_select(void *ctx)
{
    (void)ctx;
    ConfigDat *const config = config_current();
    const RaceSettings before = settings_of();
    config_dat_set_race(config, &before);
    const int32_t screen = ea_screen();
    const TrackSelectInputs in = { "", resource_kmh(), g.variant_m, g.variant_r, g.lost_vegas, screen,
                                   ea_picture_shape(screen) };
    /* port: the weather row, and with the PS1 look the time-of-day item on its psx_time; both written to port.cfg
     * when the screen leaves them changed (the race reads them there, as it does today) */
    int weather = port_weather(), psx_time = port_psx_time();
    int reverse = reverse_track_picked();   /* port: extras.cfg's reverse offers the row; the pick is the session's, not saved */
    const PortExtras extras = port_extras_load();
    TrackSelectPlus plus = { g.unlock_m, g.unlock_r, &g.variant_m, &g.variant_r,
                             extras.variant_time,   /* port: variants as entries */
                             1, &weather, port_psx_light() ? &psx_time : NULL, extras.reverse ? &reverse : NULL };
    const int was_weather = weather, was_psx_time = psx_time;
    TrackSelectState st = { config->image + (0x524df9 - 0x524df0), time_of_day, g.inverse, screen_palette, &plus };
    int x, y;
    pointer_position(&x, &y);
    DialogPointer p = { pointer_active(), 0, x, y, x, y };
    const TrackSelectOps ops = {
        flush, move_to, load_bank, no_inverse, music_feed, shape, set_palette, nothing, styles, poll_pointer, hide,
        in_rect, select_picture, select_default, blit, blit_masked, blit_word, style, shadowed, nothing, nothing, flip,
        show_dialog, mark_at, buttons, track_music, fade_in, track_play, deadline_set, sound, ticks, activate_dialog,
        key, click, track_stop, track_free, fade_out, free_bank, free_handle_inverse, best_times, image_car_flags,
        off_dialog, music,
        deadline_passed, music_fade_out, music_end, &p };
    track_select_screen(&in, &st, &p, &ops);
    if (extras.reverse) reverse_track_pick(reverse);
    if (weather != was_weather || psx_time != was_psx_time) {
        PortSettings saved = port_settings_load();
        if (weather != was_weather) saved.weather = weather;
        if (psx_time != was_psx_time) saved.psx_time = psx_time;
        if (!port_settings_save(&saved)) port_log("track select: port.cfg could not be saved\n");
    }
    RaceSettings after = before;
    if (config_dat_get_race(config, &after)) {
        after.variant_m = g.variant_m, after.variant_r = g.variant_r;   /* port: as the screen left them (get_race zeroes them) */
        settings_put(&after);
    }
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
    stat_average_speed_text(out, resource_kmh(), time, distance);
}
static void speed_text(void *ctx, char *out, int32_t speed) { (void)ctx; stat_speed_text(out, resource_kmh(), speed); }

/* 004676d8 from the track screen on config.dat's image and its records; ctx is the track screen's pointer */
static int32_t best_times(void *ctx)
{
    uint8_t *const image = config_current()->image;
    const int32_t screen = ea_screen();
    const BestTimesInputs in = { "", resource_german(), g.lost_vegas, screen, ea_picture_shape(screen),
                                 image + (0x524f8f - 0x524df0) };
    BestTimesState st = { image + (0x524df9 - 0x524df0), g.inverse, screen_palette,
                          records_glue_best_plus(image, g.variant_m, g.variant_r) };   /* port: the tables with cheats */
    const BestTimesOps ops = {
        move_to, load_art, no_inverse, music_feed, shape, flush, set_palette, nothing, styles, poll_pointer, mark_at,
        in_rect, select_picture, blit, blit_masked, blit_word, style, fit, time_text, speed_text, shadowed,
        select_default, hide, nothing, nothing, flip, show_dialog, buttons, fade_in, deadline_set, sound, ticks,
        activate_dialog, key, click, image_car_flags, fade_out, free_handle_inverse, free_bank, load_bank, off_dialog,
        music,
        deadline_passed, music_fade_out, music_end, ctx, records_glue_toggle, plus_text, plus_text_width };   /* port: the cheats item */
    return best_times_screen(&in, &st, ctx, &ops);
}
/* 004fa0cc..: the segments' top speeds and totals (menu_race fills them); 0051dccc and 0051dcec..: 0045b6c8's;
 * the shorts 0051ce24 + 0x1d2 x player, the cars' points (0045aec8's) */
static int32_t race_times[MENU_RACE_TIMES];
static int32_t finish[GAME_STATE_PLAYERS];
static TournamentOrderWork order_work;
static int16_t car_points[GAME_STATE_PLAYERS];

/* 0045bad8, 0045be50, 0045c2d0 and 0045b908 on config.dat's image and the menus' game state, this machine's player
 * 0; 0045b73c, 0045b6c8 on the game state */
/* the records the finishing player takes are this machine's (004fbb08: the slot in a link game, 0 alone) */
/* the records a results screen shows: its image is config.dat's with the race's table in its track block, refreshed after
 * each check so that the row a check set is the row shown (records_glue_shown) */
static uint8_t shown_image[RECORD_CHECK_IMAGE];
static ResultsMarks shown_marks;   /* port: the cheats of the same table, refreshed with it */
static int32_t records_checked(RouteOp op, int32_t *times)
{
    const int32_t answer = records_glue_route(op, config_current()->image, game_state, menu_words.single, times);
    records_glue_shown_as(op, shown_image, config_current()->image);
    records_glue_marks_as(op, config_current()->image, &shown_marks);
    return answer;
}
static int32_t records(void *ctx) { (void)ctx; return records_checked(ROUTE_SEGMENT, NULL); }
static int32_t totals(void *ctx) { (void)ctx; return records_checked(ROUTE_TOTALS, race_times); }
static int32_t top_record(void *ctx) { (void)ctx; return records_checked(ROUTE_TOP, NULL); }
static int32_t lap_record(void *ctx) { (void)ctx; return records_checked(ROUTE_LAP, NULL); }
static int32_t leads(void *ctx) { (void)ctx; return tournament_leads(game_state, menu_words.single); }
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
    records_glue_shown(shown_image, config_current()->image);
    records_glue_marks_as(ROUTE_SEGMENT, config_current()->image, &shown_marks);
    ResultsInputs in = { game_state, shown_image, r->dropped, menu_words.single, r->place, r->rival_place,
                               r->rival, r->rival_car, resource_german(), resource_kmh(), 0, screen, ea_picture_shape(screen),
                               CAR_NAMES, g.lost_vegas, g.variant_r, r->player_place, r->shown_place, finish, &shown_marks };
    ResultsState st = { g.inverse, screen_palette, 0 };
    int x, y;
    pointer_position(&x, &y);
    ResultsCtx c = { { pointer_active(), 0, x, y, x, y }, &in, &st, NULL };
    const ResultsOps ops = {
        records, flush, move_to, load_art, no_inverse, music_feed, shape, fade_in, styles, poll_pointer, in_rect,
        select_picture, blit, style, shadowed, fit, time_text, average_text, speed_text, set_palette, nothing,
        select_default, hide, nothing, blit_word, nothing, flip, show_dialog, mark_at, buttons, track_music, track_play,
        deadline_set, sound, ticks, activate_dialog, key, click, off_dialog, music, deadline_passed, track_stop, track_free, fade_out,
        free_bank, free_handle_inverse, blit_masked, totals, top_record, leads, best_totals, lap_record,
        results_order, lap_times, &c };
    c.ops = &ops;
    stage_in = &in.unknown_50a5b6;
    screen_of(mode, &in, &st, &c.p, &ops);
    stage_in = NULL;
}

/* 004275f8 for 00427ae0: the drive on argv[1]'s track (its stem in capitals) in the menus' window, the options from
 * config.dat (track_viewer.h's TrackViewerMenus; Options' 0 is on, 1 off, 2 none); its end (0052b2fe = 00509d6c,
 * 0 when not over) and results kept for 0045cea0 */
static TrackViewerResults last_drive;
static void chat(void *ctx);
static void drive(void *ctx, int32_t argc, char *const *argv)
{
    (void)ctx, (void)argc;
    char name[8] = { 0 };
    for (size_t i = 0; i + 1 < sizeof name && argv[1][i] != 0; i++)
        name[i] = (char)(argv[1][i] >= 'a' && argv[1][i] <= 'z' ? argv[1][i] - 0x20 : argv[1][i]);
    /* 004275f8: 004fbadc on track 3 races "tr5" (004ae1e0); port: with both variants on — g's own pair, the state
     * being raced, not extras.cfg — the rally's own track is the one raced, the mirror keeping the geometry
     * (variant_rule.h) */
    const char *const stem = rally_track_stem(name, g.variant_m, g.variant_r, g.variant_m != 0 && g.variant_r != 0);
    if (stem != name) memcpy(name, stem, 4);
    const TrackViewerMenus menus = { image_short(0x97) == 0, image_short(0x99) == 0, image_short(0x8f) == 0, g.laps,
                                     race_head_to_head_model((int16_t)g.opponent), image_short(0x73), image_short(0x75), image_short(0x79),
                                     image_short(0x71), game_state,
                                     g.track >= 0 && g.track < 9 ? time_of_day[g.track] : 1, g.variant_m, g.variant_r,
                                     (uint32_t)menu_cheats.keys_506, g.explode, g.nitro, port_extras_load().replay_cameras,
                                     (const char *)config_current()->image, g.track,
                                     config_current()->image + OPTIONS_AT };
    records_glue_begin(g.race_type, game_state + 0x592, g.variant_m, g.variant_r, g.explode, g.nitro,
                       (menu_cheats.keys_506 & 8) != 0);   /* port: which table its records go to */
    menu_audio_close();  /* the drive opens the device itself (drive_audio_open); its pause menu uses the menus' layer */
    track_viewer_drive_menus(&menus);
    if (menu_words.link_game != 0) link_race_begin((const char *)config_current()->image, chat);
    if (!track_viewer_drive(resource_root(), name, race_head_to_head_model(g.player_car), 1, g.race_type, 0, NULL))
        port_log("control_central: the race on %s could not start\n", name);
    link_race_end();
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
    StandingsInputs in = { game_state, finish, car_points, image_short(0x95), image_short(0x93),
                                 image_short(0xb1), resource_german(), g.unlock_level, 0, screen,
                                 ea_picture_shape(screen), records_glue_cheats(ROUTE_SEGMENT, config_current()->image) };
    StandingsState st = { g.inverse, screen_palette };
    int x, y;
    pointer_position(&x, &y);
    DialogPointer p = { pointer_active(), 0, x, y, x, y };
    const StandingsOps ops = {
        flush, move_to, load_art, no_inverse, music_feed, shape, set_palette, set_palette, nothing, styles,
        poll_pointer, hide, nothing, in_rect, select_picture, select_default, blit, blit_masked, blit_word, style,
        glue_boxed, time_text, results_order, nothing, nothing, flip, show_dialog, mark_at, buttons, fade_in,
        fade_out_bank, deadline_set, sound, ticks, activate_dialog, key, click, off_dialog, music, deadline_passed,
        free_bank,
        free_handle_inverse, &p };
    stage_in = &in.unknown_50a5b6;
    standings_screen(segment, &in, &st, &p, &ops);
    stage_in = NULL;
}
static void clip(void *ctx, int32_t x0, int32_t y0, int32_t x1, int32_t y1) { (void)ctx; ea_clip(x0, y0, x1, y1); }
static void no_two(void *ctx, int32_t a, int32_t b) { (void)ctx, (void)a, (void)b; }
static void no_three(void *ctx, int32_t a, int32_t b, int32_t c) { (void)ctx, (void)a, (void)b, (void)c; }
static int32_t next_segment(void *ctx);
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
    extras_apply();   /* port: the tournament's start clears the variants; the switch comes back */
}
static void game_reset(void *ctx) { (void)ctx; glue_game_reset(); }
/* 00428610 (cop_stop.c): cop1..3.tgv, or the ticket (german) */
static void screen_file(void *ctx, const char *file, int32_t mode, int32_t one) { (void)ctx; show_screen(file, mode, one); }
static void movie(void *ctx, const char *file) { (void)ctx; glue_intro_movie(file); }  /* 004419c0 */
static int32_t pick(void *ctx)
{
    static uint32_t rand_state;
    (void)ctx;
    return random_pick_of_three((uint32_t)time(NULL), &rand_state);
}
static void cops(void *ctx)
{
    const int16_t movie_on = glue_movies();
    const int16_t german = (int16_t)resource_german();
    const CopStopState st = { &movie_on, &german, ea_screen_slot() };
    cop_stop(&st, &(const CopStopOps){ picture_new, screen_file, picture_free, pick, movie, ctx });
}
/* 004283c4 (win_movie.c) on the drive's racers and finishers */
static void won(void *ctx)
{
    const WinMovieState st = { game_state, last_drive.player, last_drive.racers, link_view_only(), last_drive.dropped, g.race_type,
                               g.track, g.player_car, g.lost_vegas, ea_screen_slot() };
    win_movie(&st, &(const CopStopOps){ picture_new, screen_file, picture_free, pick, movie, ctx });
}
static void points(void *ctx) { (void)ctx; tournament_points(game_state, car_points); }
static int32_t champion(void *ctx, int32_t player) { (void)ctx; return tournament_champion(game_state, player); }

/* the short 004c5cc4 while 00427ae0 runs */
static int32_t race_warrior;

/* 00432e48 at a tournament won: the unlocks raised to config.dat's level, as 00427ae0 just set it */
static void raise_unlocks(void *ctx)
{
    (void)ctx;
    Unlocks u = unlocks_of();
    u.warrior = race_warrior;
    unlocks_raise(&u, config_dat_unlock_level(config_current()));
    unlocks_put(&u);
    race_warrior = g.warrior;
}

/* 00427ae0 (menu_race.c) on config.dat's image and the menus' game state: alone, this machine's player 0; in a link
 * game, the link game's words */
static void after_race(void *ctx);
static void race(void *ctx)
{
    (void)ctx;
    extras_apply();   /* port: a tournament's start (central_select.c) clears rally, explode, warrior and turbo */
    ConfigDat *const config = config_current();
    const RaceSettings before = settings_of();
    config_dat_set_race(config, &before);
    level_to_image();
    ea_select_default();
    ea_screen_release();  /* Control Central freed it; a picture it left is freed here, not lost from the pool */
    static int32_t ghost[MENU_RACE_GHOST];
    static const int16_t no_link = 0;
    static const int32_t player = 0, font = 0, host = 0;
    int16_t link_done = 0;
    race_warrior = g.warrior;
    const int linked = menu_words.link_game != 0;
    const MenuRaceState st = {
        config->image, game_state, word_52b302(), race_times, ghost,
        linked ? &menu_words.link_game : &no_link, linked ? &menu_words.link_lost : &no_link, pointer_active_word(),
        linked ? &menu_words.single : &player, &font, ea_screen_slot(), &race_warrior, &g.unlock_m, &g.unlock_r,
        &g.lost_vegas, linked ? &menu_words.intro_seen : &host, linked ? &link_game.flags_a[7] : &link_done };
    const MenuRaceOps ops = {
        no_mode, hide, nothing, clip, nothing, drive, no_mode, nothing, set_palette, no_two, no_three, no_mode,
        nothing, music_pick, cops, won, sound_load, race_picture, circuit_results, segment_results, standings, track_results,
        points, after_race, next_segment, reset_tournament, champion, raise_unlocks, game_reset, ctx };
    menu_race(&st, &ops);
    g.warrior = (int16_t)race_warrior;
    int32_t *const slot = ea_screen_slot();
    if (*slot != 0) ea_picture_free(*slot);  /* Control Central makes its own next */
    *slot = 0;
    RaceSettings after = before;
    if (config_dat_get_race(config, &after)) settings_put(&after);
    g.unlock_level = (int16_t)config_dat_unlock_level(config);
    races_from_state();
    extras_apply();   /* port: get_race zeroes the variants; the next race from the same Control Central keeps them */
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


/* port: our dedication and credits pages after the original's, over the credits' music */
static void credits_service(void) { menu_audio_music_service(); }
static void credits_extra(void *ctx)
{
    (void)ctx;
    credits_extra_show(TNFS_VERSION, credits_service);
}

/* 004166e0 from Options */
static void credits(void *ctx, int32_t mode)
{
    (void)mode;
    const CreditsOps ops = { load_art, shape, palette_set, music_feed, select_picture, blit, select_default,
                             picture_shape, blit_at, nothing, flip, fade_in, credits_tick, read_buttons, key, fade_out,
                             free_handle_inverse, music_fade_out, music_end, free_bank, mark, ctx, credits_extra };
    const CreditsState st = { ea_screen_slot(), pointer_active_word(), &g.buttons, stage_word(), &credits_idle };
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
    int16_t *const was = pointer_bind_active(&st.pointer_active);
    const int32_t result = key_capture(name, resource_german(), &st, &ops);
    pointer_bind_active(was);
    return result;
}
/* port: the added pages' text, panels and saving (graphics_plus' routines below), and the pad as it is now */
static void plus_text(void *ctx, int32_t style_id, const char *text, int32_t x, int32_t y);
static int32_t plus_text_width(void *ctx, const char *text);
static int32_t plus_arrow_size(void *ctx, int32_t right, int32_t *w, int32_t *h);
static void plus_arrow(void *ctx, int32_t right, int32_t lit, int32_t x, int32_t y);
static void plus_panel(void *ctx, const uint8_t *rgb, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t framed);
static void controls_plus_save(void *ctx, const PortControls *controls)
{
    (void)ctx;
    PortControls saved = *controls;   /* its `custom` as the pages left it: Defaults makes a wheel pass straight through */
    if (!port_controls_save(&saved)) port_log("controls: controls.cfg could not be saved\n");
    platform_controls_set(&saved);
}
/* Test aid, not for play: TNFS_FAKE_PAD=<source> (button4, axis5+, axis0, hat0.8) is a gamepad that presses that
 * source a moment after each time it is asked afresh (a gap of 10 ticks), so that a script can capture a binding
 * without a pad. */
static int fake_pad(RawPad *out)
{
    enum { FRESH_GAP = 10, IDLE_CALLS = 20 };
    static ControlSource source;
    static int parsed, calls;
    static uint32_t last;
    const char *spec = getenv("TNFS_FAKE_PAD");
    if (spec == NULL) return 0;
    if (!parsed) {
        char line[64];
        snprintf(line, sizeof line, "gas=%s\n", spec);
        source = port_controls_parse(line, strlen(line)).bind[ACT_GAS][0];
        parsed = 1;
    }
    const uint32_t now = timer_ticks();
    if (now - last > FRESH_GAP) calls = 0;
    last = now;
    memset(out, 0, sizeof *out);
    out->gamepad = 1, out->buttons = 15, out->axes = 6, out->hats = 1;
    if (++calls <= IDLE_CALLS) return 1;
    switch (source.kind) {
    case SRC_BUTTON: if (source.index < out->buttons) out->button[source.index] = 1; break;
    case SRC_AXIS_NEG: out->axis[source.index] = -32767; break;
    case SRC_AXIS_POS:
    case SRC_AXIS_FULL: out->axis[source.index] = 32767; break;
    case SRC_HAT: out->hat[source.index] = (uint8_t)source.rest; break;
    default: break;
    }
    return 1;
}
static int32_t pad_raw(void *ctx, RawPad *out) { (void)ctx; return fake_pad(out) || platform_joy_raw(0, out); }
/* port: the configuration screens' clean background for the controls' pages 2 and 3, the same one the Graphics and
 * CUSTOMIZE screens use (glue_own_bgnd's copy, shared) */
static int32_t controls_bgnd(void *ctx, uint8_t pixels[CONTROLS_BGND_W * CONTROLS_BGND_H], uint8_t palette[768])
{
    (void)ctx;
    return glue_own_bgnd(pixels, palette);
}
static void own_subtitle_text(void *ctx, const char *text, int32_t x, int32_t y);
static int32_t plus_key_down(void *ctx, int32_t scancode) { (void)ctx; return platform_key_down(scancode); }
static int32_t plus_key_name(void *ctx, int32_t scancode, char *out, size_t size)
{
    (void)ctx;
    return platform_key_name(scancode, out, size);
}
/* port: the controls screen, from Options (mode 0) and from the options after a race (mode 1), is the port's three pages
 * (controls.cfg's keys, the pad's bindings and its tuning), the exe's page left out */
static void controls(void *ctx, int32_t mode)
{
    static const int16_t no_joystick_b = 0, quit = 0;  /* no second joystick */
    static const int32_t no_calibration = 0;  /* 0043bc00 dropped: no Calibrate item */
    const int16_t german = (int16_t)resource_german();
    int32_t *const screen = ea_screen_slot();
    PortControls port = port_controls_load();
    ControlsPlus plus = { &port, 1 };   /* port: the three pages, the exe's left out (input_read.c reads the keys and the pad together) */
    controls_out();
    const ControlsMenuState st = { controls_kept, &german, &drive_joy_state()->present, &no_joystick_b, &no_calibration,
                                   pointer_active_word(), &g.buttons, screen, &g.inverse, word_52b302(), &quit,
                                   &plus };
    const ControlsMenuOps ops = {
        picture_new, flush, palette_clear, move_to, load_art, no_inverse, music_feed, shape, palette_set, fade_in,
        styles, poll, mark, in_rect, style, select_picture, blit, picture_shape, grab, key_name, shadowed, set_palette,
        select_default, flip, show, deadline_set, sound, nothing, blit_at, ticks, buttons, activate, key, click, clash,
        glue_boxed, capture, deadline_passed, music, fade_out, free_handle_inverse, no_mode, off, net_sync,
        picture_free, free_bank, ctx, plus_text, plus_text_width, plus_panel, controls_plus_save, pad_raw,
        plus_arrow_size, plus_arrow,
        /* port: the pages 2 and 3 over the clean background, with the title and the group headers as CUSTOMIZE's */
        controls_bgnd, glue_own_bgnd_shape, own_plus_text, own_title_text, own_header_text,
        /* port: the keyboard page's capture and key names, and the pages' subtitle */
        plus_key_down, plus_key_name, own_subtitle_text };
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
static void audio_track_music(void *ctx) { (void)ctx; menu_audio_music_track(); }
static void audio_stop_4a(void *ctx) { (void)ctx; menu_audio_stop_4a(); }
static int32_t audio_4a_done(void *ctx) { (void)ctx; return menu_audio_4a_done(); }
static void audio_effects_volume(void *ctx, int32_t volume) { (void)ctx; menu_audio_effects_volume(volume); }
static void audio_voice(void *ctx, int32_t voice, int32_t volume) { (void)ctx; menu_audio_voice_volume(voice, volume); }
static int32_t audio_effects(void *ctx) { (void)ctx; return image_short(0x79); }
static void audio_blit_at(void *ctx, const uint8_t *s, int32_t x, int32_t y) { (void)ctx; ea_blit_at(s, x, y); }
static void audio_clip(void *ctx, int32_t l, int32_t r, int32_t t, int32_t b) { (void)ctx; video_set_clip(l, r, t, b); }

/* 004015dc from Options (mode 0) on config.dat's image: the card 0049212c(-1) answers is the port's mixer, 16-bit
 * stereo with no modes to choose (bit 0 set, bit 1 clear), so neither the mode nor 3D sound is offered and the mode
 * shown is 0049212c's 3, config.dat's 0 (one saved as 1 or 2 by an older build put back); the speech is not heard
 * yet */
static void audio(void *ctx, int32_t mode)
{
    enum { AUDIO_MODE_AT = 0x71 };  /* 00524e61 */
    static int16_t small_stream;
    static const int16_t no_modes = 1, bit0 = 1, sound_alt = 0, sound_on = 1;
    static const int32_t voice_4a = 0, voice = 0;
    const int16_t german = (int16_t)resource_german();
    memset(config_current()->image + AUDIO_MODE_AT, 0, sizeof(int16_t));
    audio_track();
    opt_in();
    const AudioMenuState st = {
        config_current()->image + 9, menu_audio_silent(), menu_audio_kept(), &no_modes, &bit0, &sound_alt, &menu_words.link_game,
        &german, &sound_on, &voice_4a, &voice, &audio_pointer_x, pointer_active_word(), &g.buttons, ea_screen_slot(),
        &g.inverse, &small_stream, word_52b302(), stage_word() };
    const AudioMenuOps ops = {
        music_fade_out, music_end, audio_track_music, picture_new, move_to, load_art, no_inverse, music_feed, shape,
        palette_set, poll, mark, audio_clock, audio_stop_4a, audio_4a_done, sound, in_rect, audio_volume,
        select_picture, blit, blit_at, audio_blit_at, audio_clip, picture_shape, select_default, hide, nothing, flip,
        show, audio_buttons, styles, fade_in, deadline_set, deadline_passed, nothing, music, nothing, blit_masked,
        ticks, audio_activate, key, key, click, music_cut, audio_voice, audio_effects, music_fade_to,
        audio_effects_volume,
        glue_banner, sound_free, nothing, sound_load, off, net_sync, fade_out, free_handle_inverse, picture_free,
        free_bank, music_pick, music_start, ctx };
    audio_menu(&st, mode, &ops);
    opt_out();
}

/* port: the text and the dark panels of graphics_plus.c, controls_plus.c and extras_menu.c, through the game's own
 * routines (0046376c's styles, the name box's shade table and frame); the text in the menu art's look
 * (text_draw_smooth), safe as each of those screens repaints its background and panel before writing */
static void plus_text(void *ctx, int32_t style_id, const char *text, int32_t x, int32_t y)
{
    (void)ctx;
    text_select((TextStyle)style_id);
    text_draw_smooth(text, x, y, 1, 1);
}
static int32_t plus_text_width(void *ctx, const char *text) { (void)ctx; return text_width(text); }

/* port: the page indicator's triangles (page_arrows.h), cut from the CD the first time and kept; none when the CD
 * cannot give them, and the indicator is its text */
static int32_t plus_arrow_size(void *ctx, int32_t right, int32_t *w, int32_t *h)
{
    (void)ctx;
    const PageArrowMasks *const m = page_arrows_shared(resource_load_art);
    if (m == NULL) return 0;
    *w = m->w[right ? PAGE_ARROW_RIGHT : PAGE_ARROW_LEFT], *h = m->h[right ? PAGE_ARROW_RIGHT : PAGE_ARROW_LEFT];
    return 1;
}
static void plus_arrow(void *ctx, int32_t right, int32_t lit, int32_t x, int32_t y)
{
    (void)ctx;
    const PageArrowMasks *const m = page_arrows_shared(resource_load_art);
    const int a = right ? PAGE_ARROW_RIGHT : PAGE_ARROW_LEFT;
    if (m == NULL) return;
    text_select(lit ? TEXT_YELLOW : TEXT_CREAM);
    text_draw_item_mask(m->mask[a], m->w[a], m->h[a], x, y);
}
int32_t glue_plus_arrow_size(void *ctx, int32_t right, int32_t *w, int32_t *h) { return plus_arrow_size(ctx, right, w, h); }
static void plus_panel(void *ctx, const uint8_t *rgb, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t framed)
{
    enum { DARK = 0x1000, RED = 0xff0000, CORNERS = 4 };
    static uint8_t shade_table[256];  /* ea_shade_set keeps the pointer */
    (void)ctx;
    ea_shade_table(DARK, rgb, shade_table);
    ea_shade_set(shade_table);
    const int32_t corners[2 * CORNERS] = { x0, y0, x1, y0, x1, y1, x0, y1 };
    ea_shade_polygon(CORNERS, corners);
    if (!framed) return;
    ea_frame(x0 + 2, y0 + 2, x1 - 2, y1 - 2, ea_colour(RED));
    ea_frame(x0 + 3, y0 + 3, x1 - 3, y1 - 3, ea_colour(RED));
}

/* port: the text and panel the additions draw with, for the EXTRAS screen's glue in menu_glue.c */
void glue_plus_text(void *ctx, int32_t style_id, const char *text, int32_t x, int32_t y)
{
    plus_text(ctx, style_id, text, x, y);
}
int32_t glue_plus_text_width(void *ctx, const char *text) { return plus_text_width(ctx, text); }
void glue_plus_panel(void *ctx, const uint8_t *rgb, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t framed)
{
    plus_panel(ctx, rgb, x0, y0, x1, y1, framed);
}
static void plus_save(void *ctx, const PortSettings *settings)
{
    (void)ctx;
    if (!port_settings_save(settings)) port_log("graphics: port.cfg could not be saved\n");
}

/* port: the Graphics screen's own background (graphics_own.c). config_bgnd_load reads the ten configuration screens
 * off the CD and composes them, which takes about half a second, so it is done once per process and kept -- it does
 * not depend on anything but the CD.
 *
 * The screen always puts this palette on, in OPTIONS' palette, whatever screen brought it up: entered from Game Mode
 * its art is the Game Mode picture, and blitting the Options background over that with Game Mode's palette is what
 * left DONE/CANCEL in rubbish colours. `own_palette` is the palette to hand back on the way out. */
static uint8_t *own_shape;
static uint8_t own_pixels[CONFIG_BGND_W * CONFIG_BGND_H];
static uint8_t own_palette[768];
static int own_tried, own_ok;

/* compose once, into the caller's buffers, and keep a copy: the EXTRAS screen asks for the same background
 * (menu_glue.c), and reading the ten CD screens twice would cost another half second for nothing */
static void own_compose(uint8_t pixels[CONFIG_BGND_W * CONFIG_BGND_H], uint8_t palette[768])
{
    if (!own_tried) {
        own_tried = 1;
        own_ok = config_bgnd_load(resource_load_art, CONFIG_BGND_OPTIONS, 1, own_pixels, own_palette);
        if (own_ok) {
            own_shape = malloc(CONFIG_BGND_SHAPE_HEADER + CONFIG_BGND_W * CONFIG_BGND_H);
            own_ok = own_shape != NULL
                     && config_bgnd_shape(own_pixels, 0, 0, CONFIG_BGND_W, CONFIG_BGND_H, own_shape) != 0;
        }
        if (!own_ok) {
            port_log("graphics: no clean configuration background, the burnt-in one stays\n");
            free(own_shape);
            own_shape = NULL;
        }
    }
    memcpy(pixels, own_pixels, sizeof own_pixels);
    memcpy(palette, own_palette, sizeof own_palette);
}

static int32_t own_bgnd(uint8_t pixels[640 * 480], uint8_t palette[768])
{
    own_compose(pixels, palette);
    return own_ok;
}

/* the shape, already a game shape, put on the screen; the palette goes with it, then the text styles are built from
 * that palette -- they are a ramp matched to what is showing, and they were last built for another screen's */
static int32_t own_bgnd_shape(void *ctx, const uint8_t pixels[640 * 480], const uint8_t palette[768])
{
    (void)ctx, (void)pixels;
    if (own_shape == NULL) return 0;
    /* RAW, into the screen's own copy rather than back into own_palette: config_bgnd.c read this palette through
     * fsh_palette_rgb, which shifts the CD's 6-bit values left by one on the way, so it is already display-ready
     * and any other mode shifts it twice; and palette_put reads and writes the same buffer, so aliasing the two
     * would rewrite the bytes it is reading, a little further every redraw. */
    memcpy(screen_palette, own_palette, sizeof screen_palette);
    palette_set(ctx, screen_palette, 0, 0x100, own_palette, 0x24 /* FSH_TYPE_PALETTE_8BIT */);
    text_build_styles_from(own_palette);
    ea_blit_own(own_shape);
    (void)palette;
    return 1;
}

/* the same background for the EXTRAS screen (menu_glue.c): one composed copy, shared by both screens rather than
 * read off the CD twice */
int32_t glue_own_bgnd(uint8_t pixels[CONFIG_BGND_W * CONFIG_BGND_H], uint8_t palette[768])
{
    own_compose(pixels, palette);
    return own_ok;
}

int32_t glue_own_bgnd_shape(void *ctx, const uint8_t pixels[CONFIG_BGND_W * CONFIG_BGND_H],
                            const uint8_t palette[768])
{
    return own_bgnd_shape(ctx, pixels, palette);
}

/* the smooth text for the Graphics screen: the letters with the background put back under them first, taken from the
 * composed copy in own_pixels -- the same picture the page is blitted with, not Options' bank. This is
 * options_plus_text's draw over the Graphics screen's own background instead of the Options one. */
void own_plus_text(void *ctx, int32_t style_id, const char *text, int32_t x, int32_t y)
{
    enum { MARGIN = 2, SHADOW_ROOM = 6 };
    (void)ctx;
    if (own_shape != NULL) {
        const int left = x - MARGIN, top = y - MARGIN;
        const int w = text_width(text) + MARGIN + SHADOW_ROOM, h = text_line_height() + MARGIN + SHADOW_ROOM;
        uint8_t *const cover = malloc(CONFIG_BGND_SHAPE_HEADER + (size_t)w * h);
        if (cover != NULL && config_bgnd_shape(own_pixels, left, top, w, h, cover) != 0)
            ea_blit_own(cover);
        free(cover);
    }
    own_panel_text(ctx, style_id, text, x, y);
}

/* port: the titles in the original titles' letters (title_art.h), cut from the CD the first time each is drawn and
 * kept; a title the CD cannot give is drawn in the font */
static const uint8_t *title_mask(const char *text, int *w)
{
    static uint8_t masks[TITLE_ART_COUNT][TITLE_ART_MAX_W * TITLE_ART_H];
    static int widths[TITLE_ART_COUNT], tried[TITLE_ART_COUNT], ok[TITLE_ART_COUNT];
    const TitleArtId id = title_art_for(text);
    if (id == TITLE_ART_COUNT) return NULL;
    if (!tried[id]) {
        tried[id] = 1;
        ok[id] = title_art_load(resource_load_art, id, masks[id], &widths[id]);
        if (!ok[id]) port_log("title: \"%s\" not on the CD, drawn in the font\n", text);
    }
    *w = widths[id];
    return ok[id] ? masks[id] : NULL;
}

/* port: whether the display is already dark, so the Graphics screen skips the fade of a display Options has already
 * faded out. video_display_is_dark answers from the palette on the display, so it is right whoever faded it. */
static int32_t display_dark(void *ctx)
{
    (void)ctx;
    return video_display_is_dark();
}

/* the screen's title as the original screens draw theirs ("Game Mode", "Control Central"): red, at their items' size */
void own_title_text(void *ctx, const char *text, int32_t x, int32_t y)
{
    enum { TITLE_NUM = 33, TITLE_DEN = 20 };   /* menu_glue.c's MODE_TEXT_NUM / DEN */
    (void)ctx;
    text_build_styles_from(own_palette);
    text_select(TEXT_RED);
    int w = 0;
    const uint8_t *const art = title_mask(text, &w);
    if (art != NULL)   /* where the originals stand theirs, taken from where the font's would stand */
        text_draw_mask(art, w, TITLE_ART_H, x - TITLE_ART_FONT_X + TITLE_ART_LEFT, y - TITLE_ART_FONT_Y + TITLE_ART_ROW0);
    else
        text_draw_smooth(text, x, y, TITLE_NUM, TITLE_DEN);
}

/* the device a controls page is for, under its title: in the title's red, at the items' size */
static void own_subtitle_text(void *ctx, const char *text, int32_t x, int32_t y)
{
    (void)ctx;
    text_build_styles_from(own_palette);
    text_select(TEXT_RED);
    text_draw_smooth(text, x, y, 1, 1);
}

/* a group's header as the credits draw theirs ("EA Seattle"), at their size: about 2/3 of the game's text */
void own_header_text(void *ctx, const char *text, int32_t x, int32_t y, int32_t width)
{
    enum { HEADER_NUM = 2, HEADER_DEN = 3 };
    (void)ctx;
    text_build_styles_from(own_palette);
    text_draw_header(text, x, y, HEADER_NUM, HEADER_DEN, width);
}

void own_panel_text(void *ctx, int32_t style_id, const char *text, int32_t x, int32_t y)
{
    (void)ctx;
    /* the styles from the background that is showing: they are a ramp matched to it, and the last ones were built
     * for another screen's palette */
    text_build_styles_from(own_palette);
    text_select((TextStyle)style_id);
    text_draw_smooth(text, x, y, 1, 1);
}

/* the triangle as own_panel_text draws a word: in the styles of the background that is showing */
void own_panel_arrow(void *ctx, int32_t right, int32_t lit, int32_t x, int32_t y)
{
    text_build_styles_from(own_palette);
    plus_arrow(ctx, right, lit, x, y);
}

/* Cancel or Escape: the settings back as they were on entry, and the config.dat image with them -- the view distance
 * is one of the screen's shorts there, not in port.cfg, so the kept image is what puts it back. Both are kept for as
 * long as the screen may be up, which is what the file scope is for. */
static PortSettings port_settings_was;
static uint8_t config_current_kept_image[GRAPHICS_KEPT];

static void own_put_back(void *ctx, PortSettings *settings)
{
    (void)ctx;
    *settings = port_settings_was;
    memcpy(config_current()->image + 9, config_current_kept_image, GRAPHICS_KEPT);
}

/* port: the Graphics screen (graphics_own.c) from Options (mode 0, with the second page) and from the race's pause menu
 * (mode 1, GraphicsPlus.race: the rows the next race reads are dimmed there). Both edit port.cfg; the view distance is
 * config.dat's own short, which the race's menu takes back through its image. The race applies what it can when the
 * menu closes (drive_menu.c, track_viewer_graphics_apply). */
static void graphics(void *ctx, int32_t mode)
{
    static int16_t showing;  /* 004f9e8c */
    const int race = mode != 0;
    PortSettings port = port_settings_load();
    const PortSettings was = port;
    int desktop_w = 0, desktop_h = 0;
    const int has_desktop = platform_desktop_size(&desktop_w, &desktop_h);
    GraphicsPlus plus = { &port, has_desktop ? screen_format_nearest(desktop_w, desktop_h) : SCREEN_4_3, race, port.f3_ultra };
    opt_in();
    /* after opt_in: it writes Options' unsaved edits into the image, which Cancel must keep */
    port_settings_was = port;
    memcpy(config_current_kept_image, config_current()->image + 9, GRAPHICS_KEPT);
    /* port: the palette that brought this screen up, kept: it fades the whole display out as it closes (00423d28's
     * close_screen), so the styles rebuilt after it must be built from this copy rather than from what is left on the
     * display -- which is that palette faded down to black, and a ramp built from black is not the caller's menu. */
    uint8_t caller_palette[PALETTE_PUT_BYTES];
    memcpy(caller_palette, screen_palette, sizeof caller_palette);
    const GraphicsMenuState st = { config_current()->image + 9, &showing, word_52b302(), pointer_active_word(), &g.buttons,
                                   ea_screen_slot(), &g.inverse, stage_word(), &plus };
    const GraphicsMenuOps ops = {
        move_to, load_art, no_inverse, music_feed, shape, palette_set, poll, mark, in_rect, select_picture, blit,
        picture_shape, select_default, hide, nothing, flip, show, buttons, styles, fade_in, deadline_set, sound,
        nothing, ticks, activate, key, click, off, music, net_sync, deadline_passed, fade_out, free_handle_inverse,
        free_bank, ctx, plus_text, plus_text_width, plus_panel, plus_save,
        /* port: the screen's own background, its shape with the palette that goes with it, and its title. The race's
         * pause menu gets the same ones as Options. */
        own_bgnd, own_bgnd_shape, own_title_text, own_put_back,
        /* port: the letters go through the draw that restores the clean background under them */
        own_plus_text,
        /* port: whether the display is already dark, so the screen skips a fade of a display Options has already
         * faded out rather than scaling zeros for seventeen vblanks */
        display_dark, plus_arrow_size, plus_arrow };
    input_function_keys(&showing);
    input_page_keys(!race);
    /* port: graphics_own.c, the port's own screen, from Options and from the race's pause menu alike */
    graphics_own(&st, &ops);
    /* the screen put its own palette on and faded the display out as it left; the one that brought it up goes
     * back, so whatever is on screen afterwards is drawn the way its own caller means */
    text_build_styles_from(caller_palette);
    input_page_keys(0);
    input_function_keys(NULL);
    opt_out();
    /* port: the screen's settings changed (Done): the window takes them now, not at the next start */
    const PortSettings now = port_settings_load();
    if (mode == 0 && (was.format != now.format || was.scale_integer != now.scale_integer
                      || was.fullscreen != now.fullscreen || was.refresh_hz != now.refresh_hz)) {
        const PlatformConfig config = port_platform_config(NULL);
        if (!platform_reconfigure(&config)) port_log("graphics: the window keeps its old settings\n");
    }
}

/* 0042ae9c's and 0042c498's words: the shorts 004fa9ac (check/'s art) and 004bad00 (a highlight), the names
 * 004fa1dc, the game state kept 00524838, 004fbad8's bytes kept around a replay */
static int16_t list_check, list_highlight;
static uint8_t list_names[(GAME_LIST_NAMES + 1) * FILE_LIST_NAME];
static uint8_t game_kept[GAME_LIST_STATE];
static uint8_t replay_race[LOAD_SAVE_RACE];
static int16_t replay_kind;  /* config.dat's short 00524e9f, aligned while 0042c498 runs */

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
                         pointer_active_word(), &g.buttons, image + 9, (char *)image + 0x3b, game_state, game_kept,
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
/* 004275f8 for a replay (0042c498's eax 2): config.dat's name 00524e2b, played as kind 00524e9f asks (1 the
 * replay from the start, 0044f534(4); 2 the highlights, 5), the player's file else the CD's; in the menus' window
 * with their volumes and no game state to write */
static void replay(void *ctx, int32_t mode, const char *path)
{
    enum { NAME_AT = 0x3b, NAME = 0x14, FROM_START = 4, HIGHLIGHTS = 5 };
    (void)ctx, (void)mode, (void)path;
    char name[NAME + 1] = { 0 };
    memcpy(name, config_current()->image + NAME_AT, NAME);
    size_t size = 0;
    uint8_t *file = save_files_read_replay(name, &size);
    if (file == NULL) {
        port_log("load_save: no replay %s\n", name);
        return;
    }
    const TrackViewerMenus menus = { 1, 1, 1, 0, 0, image_short(0x73), image_short(0x75), image_short(0x79),
                                     image_short(0x71), NULL, 0, 0, 0, 0, 0, 0, 0, NULL, -1,
                                     config_current()->image + OPTIONS_AT };  /* the time of day and the flags: the replay's setup */
    menu_audio_close();
    track_viewer_drive_menus(&menus);
    if (!track_viewer_drive_replay_data(resource_root(), file, size, replay_kind == 2 ? HIGHLIGHTS : FROM_START))
        port_log("load_save: the replay %s could not play\n", name);
    menu_audio_open();
    platform_free_file(file);
}
static void no_mode_set(void *ctx, int32_t w, int32_t h, int32_t bits) { (void)ctx, (void)w, (void)h, (void)bits; }

/* 0042c498 from Options (mode 0) on config.dat's image, its shorts copied around it to aligned ones */
static void load_save(void *ctx, int32_t mode)
{
    ConfigDat *const config = config_current();
    level_to_image();
    opt_in();
    int16_t variant = image_short(0x93);
    replay_kind = image_short(0xaf);
    volumes_sync();
    LoadSaveState st = {
        resource_german(), g.tournament, menu_words.link_game, image_short(0x6d), pointer_active(), &g.buttons,
        ea_screen_slot(), &g.inverse, &list_check, &variant, &list_highlight, &replay_kind, replay_race,
        menu_audio_silent(), g.volumes, KEPT_VOLUMES, image_short(0x7f),
        (const char *)config->image + 9, (const char *)config->image + 0x3b };
    const LoadSaveOps ops = {
        move_to, load_art, no_inverse, music_feed, palette_clear, shape, palette_set, poll, mark, in_rect,
        select_picture, blit, select_default, hide, nothing, picture_shape, nothing, flip, show, buttons, fade_in,
        styles, deadline_set, sound, ticks, activate, key, click, game_list, save_box, replay_box,
        free_handle_inverse, free_bank, music_fade_out, fade_out, music_end, picture_free, sound_free, clear_one,
        clip, replay, nothing, set_palette, no_mode_set, picture_new, sound_load, music_pick, off, music,
        deadline_passed, ctx };
    int16_t *const was = pointer_bind_active(&st.pointer_active);
    load_save_menu(mode, &st, &ops);
    volumes_sync();
    pointer_bind_active(was);
    memcpy(config->image + 0x93, &variant, sizeof variant), memcpy(config->image + 0xaf, &replay_kind, sizeof replay_kind);
    opt_out();
    g.unlock_level = (int16_t)config_dat_unlock_level(config);
    races_from_state();
}

/* load_save_menu2 (0042d894) from 00403760 (mode 1, check/'s art) on config.dat's image, as load_save; its tournament's races (the
 * game state's +0x592..+0x5a0, Lost Vegas's +0x5a2) copied to aligned shorts */
static void load_save2(void *ctx, int32_t mode)
{
    level_to_image();
    enum { RACES_AT = 0x592, EXTRA_AT = 0x5a2, RACES = 8 };
    ConfigDat *const config = config_current();
    opt_in();
    int16_t races[RACES], extra;
    memcpy(races, game_state + RACES_AT, sizeof races), memcpy(&extra, game_state + EXTRA_AT, sizeof extra);
    replay_kind = image_short(0xaf);
    volumes_sync();
    LoadSave2State st = {
        resource_german(), g.tournament, menu_words.link_game, image_short(0x6d), pointer_active(), &g.buttons, races,
        extra, g.lost_vegas, ea_screen_slot(), &g.inverse, &list_check, &list_highlight, &replay_kind, menu_audio_silent(),
        g.volumes, KEPT_VOLUMES, image_short(0x7f), (const char *)config->image + 9,
        (const char *)config->image + 0x3b };
    const LoadSaveOps ops = {
        move_to, load_art, no_inverse, music_feed, palette_clear, shape, palette_set, poll, mark, in_rect,
        select_picture, blit, select_default, hide, nothing, picture_shape, nothing, flip, show, buttons, fade_in,
        styles, deadline_set, sound, ticks, activate, key, click, game_list, save_box, replay_box,
        free_handle_inverse, free_bank, music_fade_out, fade_out, music_end, picture_free, sound_free, clear_one,
        clip, replay, nothing, set_palette, no_mode_set, picture_new, sound_load, music_pick, off, music,
        deadline_passed, ctx };
    int16_t *const was = pointer_bind_active(&st.pointer_active);
    load_save_menu2(mode, &st, &ops);
    volumes_sync();
    pointer_bind_active(was);
    memcpy(config->image + 0xaf, &replay_kind, sizeof replay_kind);
    opt_out();
    g.unlock_level = (int16_t)config_dat_unlock_level(config);
    races_from_state();
}
static void save_config(void *ctx) { (void)ctx; config_save(); }

/* check_options (00403760) on config.dat's image (the sub-screens' shorts taken from it first): the options after a race, mode
 * 0 from 0040b648, 1 from 0040c710 */
int32_t glue_check_options(int32_t mode)
{
    ConfigDat *const config = config_current();
    opt_out();
    const CheckOptionsOps ops = {
        move_to, load_art, no_inverse, shape, palette_set, poll, mark, in_rect, select_picture, blit, select_default,
        hide, picture_shape, flip, show, buttons, fade_in, deadline_set, sound, nothing, ticks, activate, key, click,
        fade_out, free_handle_inverse, free_bank, audio, graphics, controls, load_save, load_save2, music_fade_to,
        off, music, deadline_passed, no_wait, music_feed, save_config, NULL };
    const int16_t language = (int16_t)resource_german();
    const CheckOptionsState st = { config->image, &menu_words.link_game, &language, pointer_active_word(), &g.buttons,
                                   ea_screen_slot(), &g.inverse, stage_word() };
    const int32_t done = check_options(mode, &st, &ops);
    opt_out();
    return done;
}

/* 00447564's option screens from the race's pause menu, on config.dat's image: 004015dc with eax 2 (audio), 00423d28
 * with eax 1 (graphics, no resolution) */
void glue_race_option(uint32_t address)
{
    if (address == 0x4015dc) audio(NULL, 2);
    else if (address == 0x423d28) graphics(NULL, 1);
}

static void after_options(void *ctx, int32_t mode) { (void)ctx; glue_check_options(mode); }

enum { REPLAY_FLAG_AT = 0xaf };

/* 0040c710's words on config.dat's image and the menus' game state; the cars' points (0051ce24) kept
 * in a buffer of their own: the port's replay leaves car_points */
static AfterRaceState after_race_state(void)
{
    static uint8_t cars[AFTER_RACE_CARS * AFTER_RACE_CAR];
    uint8_t *const image = config_current()->image;
    replay_kind = image_short(REPLAY_FLAG_AT);
    volumes_sync();
    return (AfterRaceState){
        &menu_words.link_game, stage_word(), pointer_active_word(), &g.buttons, ea_screen_slot(), &g.inverse,
        &credits_idle, &menu_words.redraw, &replay_kind, (char *)image + 0x3b, menu_audio_silent(), g.volumes,
        KEPT_VOLUMES, image_short(0x7f), image_short(0x95), image_short(0x6d),
        game_state, cars };
}

/* 0040a5d8's ops: one thread, so the lines' lock is always free; no joystick, no keyboard layouts to switch */
static int32_t lines_free(void *ctx, uint32_t lock) { (void)ctx, (void)lock; return 1; }
static void lines_done(void *ctx, uint32_t lock) { (void)ctx, (void)lock; }
static int32_t no_joystick(void *ctx) { (void)ctx; return 0; }
static void restore(void *ctx, int32_t s, int32_t x, int32_t y) { (void)ctx; ea_restore(s, x, y); }
/* 0047380c: 004737d0 less the shape's own origin, which a picture's shape (all chat shows) has at 0, 0 */
static void show_at(void *ctx, int32_t s, int32_t x, int32_t y) { (void)ctx; ea_restore(s, x, y); }
static void chat_send(void *ctx, int32_t link, const uint8_t *message, int32_t size)
{
    (void)ctx;
    link_send((uint32_t)link, message, size);
}
/* 004735dc draws chat's screen straight onto the shown page (it never flips): here onto the back buffer, which is
 * shown before the pointer goes over it */
static void chat_shown(void *ctx) { (void)ctx, video_flip(), pointer_show(); }
static void chat_in(void *ctx, int32_t link, const uint8_t *message, int32_t size)
{
    (void)ctx, (void)size;
    link_chat_in((uint32_t)link, message);
}

/* 0040a5d8 on the link game's lines */
static void chat(void *ctx)
{
    static const ChatOps ops = {
        load_art, shape, palette_set, select_picture, blit, blit_masked, select_default, picture_shape, blit_word, flip,
        buttons, fade_in, nothing, styles, picture_new, grab, glue_drain, keys_pending, music, nothing, move_to, mark,
        net_service, in_rect, sound, lines_free, style, restore, shadowed, show_at, pump, lines_done, key_read,
        no_joystick, width, click, ticks, chat_send, chat_in, select_default, nothing, chat_shown, buttons, picture_free, nothing,
        fade_out, free_handle_inverse, free_bank, NULL };
    static const uint32_t lines_lock = 0;
    static const int32_t no_key = 0;
    const int16_t german = (int16_t)resource_german();
    ChatState st = { frontend_player_name, &menu_words.mode_menu, &link_game.chat[0][0], &link_game.chat_line,
                     &link_game.chat_arrived, &lines_lock, &no_key, &no_key, &german, &menu_words.link_lost,
                     &link_game.flags_a[RACE_NET_STAGE_FLAG], &link_game.unknown_52b318, &menu_words.redraw,
                     pointer_active_word(), &g.buttons, ea_screen_slot() };
    (void)ctx;
    chat_screen(&st, &ops);
}

static AfterRaceOps after_race_ops(void (*options)(void *, int32_t), void *ctx)
{
    return (AfterRaceOps){
        move_to, load_art, no_inverse, music_feed, palette_clear, shape, palette_set, poll, mark, net_service, in_rect,
        chat_waiting, select_picture, blit, blit_masked, select_default, hide, picture_shape, flip, show, buttons, fade_in,
        styles, deadline_set, sound, nothing, ticks, activate, key, click, music_fade_out, fade_out, music_end,
        free_bank, free_handle_inverse, picture_free, sound_free, clear_one, clip, replay, nothing, set_palette,
        no_mode_set, no_mode, picture_new, sound_load, music_pick, options, standings, circuit_results,
        segment_results, track_results, chat, off, credits_tick, deadline_passed, ctx };
}

/* 0040c710 for 00427ae0 */
static void after_race(void *ctx)
{
    AfterRaceState st = after_race_state();
    const AfterRaceOps ops = after_race_ops(after_options, ctx);
    credits_last = timer_ticks();
    after_race_menu(&st, &ops);
    volumes_sync();
    memcpy(config_current()->image + REPLAY_FLAG_AT, &replay_kind, sizeof replay_kind);
}

/* 00410b7c and 00410c18: the link's deadline and the wait for the others (link_central.h) */
static void deadline(void *ctx, int32_t ticks) { (void)ctx, link_deadline(ticks); }
static int32_t all_came(void *ctx, int32_t stage) { (void)ctx; return link_wait((int16_t)stage); }

/* 0040b648 for 00427ae0: the checkpoint menu between segments, 0 to stop the tournament's run */
static int32_t next_segment(void *ctx)
{
    int16_t *const over = &link_game.flags_a[RACE_NET_STAGE_FLAG];  /* 0050a5b6: read, and cleared once the others came */
    NextSegmentState st = { after_race_state(), over, &menu_words.link_lost, (int16_t)resource_german() };
    st.menu.link_over = over;
    const NextSegmentOps ops = { after_race_ops(after_options, ctx), glue_yes_no, glue_banner, deadline, all_came };
    credits_last = timer_ticks();
    const int32_t go_on = next_segment_menu(&st, &ops);
    volumes_sync();
    memcpy(config_current()->image + REPLAY_FLAG_AT, &replay_kind, sizeof replay_kind);
    return go_on;
}

/* port: the Extras row opens the EXTRAS screen over this screen's picture; Done writes extras.cfg and the unlock
 * level back into config.dat, and extras_apply re-reads it for Control Central (the cheats, mirror, ghosts). */
static void options_extras(void *ctx)
{
    (void)ctx;
    ConfigDat *const config = config_current();
    glue_extras_screen((int16_t)config_dat_unlock_level(config));
    g.unlock_level = (int16_t)config_dat_unlock_level(config);
    extras_apply();
}

/* port: the Options rows' text (Customize game) soft like the art: the screen's own picture put back under it
 * first, since the row is redrawn over itself when the selection moves */
static void options_plus_text(void *ctx, int32_t style_id, const char *text, int32_t x, int32_t y)
{
    enum { MARGIN = 2, SHADOW_ROOM = 6 };
    static int32_t bank;
    (void)ctx;
    if (bank == 0) bank = ea_bank_load("option/options.qfs");
    const uint8_t *const bgnd = bank != 0 ? ea_shape(bank, "bgnd") : NULL;
    const int left = x - MARGIN, top = y - MARGIN;
    const int w = text_width(text) + MARGIN + SHADOW_ROOM, h = text_line_height() + MARGIN + SHADOW_ROOM;
    uint8_t *const cover = malloc(CONFIG_BGND_SHAPE_HEADER + (size_t)w * h);
    if (bgnd != NULL && cover != NULL && config_bgnd_shape(bgnd + CONFIG_BGND_SHAPE_HEADER, left, top, w, h, cover) != 0)
        ea_blit_own(cover);
    free(cover);
    /* the styles from the bank's palette: the first draw comes before the fade puts it on screen */
    const uint8_t *const pal = bank != 0 ? ea_shape(bank, "!pal") : NULL;
    uint8_t rgb[768];
    const FshEntry e = { .type = pal != NULL ? pal[0] : FSH_TYPE_BITMAP, .payload = pal != NULL ? pal + 16 : NULL,
                         .payload_size = sizeof rgb };
    if (pal != NULL && fsh_palette_rgb(&e, rgb) == FSH_OK) text_build_styles_from(rgb);
    text_select((TextStyle)style_id);
    text_draw_smooth(text, x, y, 1, 1);
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
        music_fade_out, music_end, music_start, credits, music_pick, opt_car_flags, music_fade_to, off, music, net_sync,
        deadline_passed, ctx, options_extras, options_plus_text, plus_text_width, plus_panel };
    OptionsMenuState st = {
        &opt[OPT_TRANSMISSION], &opt[OPT_AUTOMATIC], &opt[OPT_ABS], &opt[OPT_TRACTION], &opt[OPT_RACE_TYPE],
        &opt[OPT_TRACK], &opt[OPT_OPPONENT], &opt[OPT_CAR], &opt[OPT_NET_CAR], &opt[OPT_LAPS], &opt[OPT_DIFFICULTY],
        &opt[OPT_MUSIC_VOLUME], &menu_words.link_game, pointer_active_word(), &g.buttons, ea_screen_slot(), &g.inverse,
        word_52b302(), stage_word(), &menu_words.redraw };
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
    races_from_state();
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
    /* the original's settings are config.dat's own words, so tournament_reset's variant = 0 clears 00524e83 */
    config_dat_new_session(config_current());
}

/* 00408724 on Control Central's words */
static void showcase(void *ctx)
{
    const ShowcaseGlue in = { &g.player_car, &g.race_type, &g.track, pointer_active_word(), &g.buttons, car_flags, ctx };
    glue_showcase(&in);
}

static int central_up;  /* the race settings live in g while Control Central runs */

/* A window closed in the menus (port: the original quits only through them): the race settings into config.dat's
 * image as on leaving Control Central, while it runs */
void glue_settings_to_config(void)
{
    if (!central_up) return;
    const RaceSettings s = settings_of();
    config_dat_set_race(config_current(), &s);
    level_to_image();
}

int32_t glue_central(void)
{
    static const ControlCentralOps OPS = {
        pump, palette_clear, move_to, music, load_art, picture_new, sound_load, no_inverse, music_feed, mark, shape,
        palette_set, net_service, in_rect, car_flags, type_changed, ticks, chat_waiting, select_picture, blit, blit_masked,
        blit_word, picture_shape, width, style, shadowed, frame, free_bank, load_bank, text_at, select_default, flip,
        show, buttons, fade_in, nothing, styles, sound, deadline_set, deadline_passed, key, scancode, cheat_step,
        key_down, activate, net_start, click, kick, music_fade_out, fade_out, music_end, music_start, showcase,
        music_pick, step_race_type, track_forward, track_backward, nothing, standings, track_select, art_free, options,
        chat, glue_yes_no, glue_drain, glue_banner, pre_race, sound_free, picture_free, flush, race, off,
        plus_text, NULL, 1, pad_buttons, palette_colour };
    load();
    ControlCentralState st = {
        &g.race_type, &g.opponent, &g.track, &g.player_car, g.other_car, &g.net_car, &g.laps, g.difficulty,
        menu_words.mode_menu, g.volumes, menu_words.link_game, link_view_only(), &menu_words.redraw, (int16_t)resource_german(), 0,
        &g.tournament, &menu_words.link_lost, g.races, menu_audio_kept(), (uint32_t *)&menu_cheats.keys_506,
        &g.unlock_level, &g.unlock_m, &g.unlock_r, &g.lost_vegas, &g.vegas_cheat, &g.net_player, &g.net_player_was,
        g.was, g.art, &g.variant_m, &g.variant_r, menu_words.single, menu_words.intro_seen, &g.nomercy, &g.warrior,
        &g.explode, menu_audio_silent(), &g.inverse, &g.nitro, &g.net_hover, &g.net_self, &g.net_second, &g.letter_held,
        link_humans(), link_names(), pointer_active_word(), &g.buttons, ea_screen_slot(), "", keys, &warrior_rival_flag, &warrior_keep_flag };
    const LinkMenu link_menu = { &g.race_type, &g.track, &g.laps, &g.difficulty, &g.net_car, &g.nomercy, &g.tournament,
                                 &g.player_car, g.other_car, &g.variant_m, &g.variant_r };
    link_central_bind(&link_menu);
    extras_apply();   /* port: the switches are on from the first key, not after the first car change */
    central_up = 1, central_state = &st;
    volumes_sync();
    const int32_t result = control_central(&st, &OPS);
    volumes_sync();
    central_up = 0, central_state = NULL;
    link_central_bind(NULL);
    extras_apply();   /* port: 00432ff4's start clears the cheats; the switches come back for the race */
    video_set_overlay(NULL, 0, 0);
    const RaceSettings s = settings_of();
    config_dat_set_race(config_current(), &s);
    level_to_image();
    return result;
}

/* --drive's race (port only, development): config.dat's settings with the track, its segment, the car and the race
 * type asked (-1 keeps config.dat's), run as Control Central's Race runs it: 00427ae0, so the race ends as the
 * original's does (the car slows to a stop, then the results screens) */
void glue_drive_race(int32_t track, int32_t segment, int32_t car, int32_t race_type, int32_t variant_m,
                     int32_t variant_r)
{
    glue_game_reset();  /* frontend_loop's 00466ae4 before Control Central: the names, the controls, the counts */
    const int16_t run = (int16_t)segment;  /* 00524e83, which 00427ae0 keeps as 0052b276: the run's file */
    memcpy(config_current()->image + SEGMENT_AT, &run, sizeof run);
    RaceSettings s = settings_of();
    if (track >= 0) s.track = track, s.variant = segment;  /* g.variant: the same short */
    s.variant_m = variant_m, s.variant_r = variant_r;  /* 004fbad8 / 004fbadc, as the cheats and track select set them */
    const char *const time = getenv("TNFS_DRIVE_TIME");  /* port: the track screen's time of day, 0..2 */
    if (time != NULL && track >= 0 && track < 9) time_of_day[track] = atoi(time);
    if (car >= 0) s.player_car = car;
    if (race_type >= 0) s.race_type = race_type;
    settings_put(&s);
    race(NULL);
    const RaceSettings after = settings_of();
    config_dat_set_race(config_current(), &after);
}
