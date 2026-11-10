#include "game/frontend/central_glue_private.h"

CentralGlobals cg_g = { .was = { 0x14, 0x14, 0x14, 0x14, 0x14, 0x14 } };  /* 004bafb4..'s data: no art shown yet */

enum { MOVIES_AT = 0x5b8 };                        /* 0052b2fc: 2, the movies offered (the original's default) */
uint8_t cg_game_state[GAME_STATE_SIZE] = { [MOVIES_AT] = 2 };  /* 0052b2c0..: the tournament's */

/* 0052b2fc: the movies the game offers (the showcase's video page, the arrest's and the wins' movies); the port's
 * no_movies only skips the intro (intro_glue.c) */
int16_t glue_movies(void)
{
    return (int16_t)(cg_game_state[MOVIES_AT] | cg_game_state[MOVIES_AT + 1] << 8);
}
static uint8_t keys[0x100];                       /* 004c693c: by scancode, refreshed at each key (00464bd0) */
uint8_t cg_screen_palette[PALETTE_PUT_BYTES];

RaceSettings cg_settings_of(void)
{
    RaceSettings s = { cg_g.race_type, cg_g.track, cg_g.variant_m, cg_g.variant_r, cg_g.player_car, cg_g.opponent, cg_g.laps,
                       cg_g.difficulty, cg_g.car_flag_a, cg_g.car_flag_b, cg_g.variant, { 0 }, cg_g.net_car };
    for (int i = 0; i < 8; i++) s.other_car[i] = cg_g.other_car[i];
    return s;
}

void cg_settings_put(const RaceSettings *s)
{
    cg_g.race_type = (int16_t)s->race_type, cg_g.track = (int16_t)s->track, cg_g.player_car = (int16_t)s->player_car;
    cg_g.variant_m = s->variant_m, cg_g.variant_r = s->variant_r, cg_g.opponent = (int16_t)s->opponent;
    cg_g.laps = (int16_t)s->laps, cg_g.difficulty = (int16_t)s->difficulty, cg_g.variant = (int16_t)s->variant;
    cg_g.car_flag_a = (int16_t)s->car_flag_a, cg_g.car_flag_b = (int16_t)s->car_flag_b, cg_g.net_car = (int16_t)s->unknown_524e9d;
    for (int i = 0; i < 8; i++) cg_g.other_car[i] = (int16_t)s->other_car[i];
}

/* The short 0052ad40 is config.dat's (00524df0 + 0x5f50): the level a cheat raised (central_keys.c) back into the image
 * before a path that reads it from there, and before the image is saved (0042a504) */
void cg_level_to_image(void) { config_dat_set_unlock_level(config_current(), cg_g.unlock_level); }

/* 0052b2d6..0052b2e8, the tournament's races by track that 00432ff4 sums: the menus' game state's (+0x592) */
void cg_races_from_state(void) { memcpy(cg_g.races, cg_game_state + 0x592, sizeof cg_g.races); }

/* The short 0052b302: set while 00427ae0 runs a stage (menu_race.c), the link game's in a link game; the option
 * screens read it for their art and, while clear, keep a link game in step (004104d0) */
static int16_t solo_52b302;
int16_t *cg_word_52b302(void) { return menu_words.link_game != 0 ? &link_game.unknown_52b302 : &solo_52b302; }

Unlocks cg_unlocks_of(void) { return (Unlocks){ cg_g.unlock_m, cg_g.unlock_r, cg_g.lost_vegas, cg_g.hidden, cg_g.warrior }; }

void cg_unlocks_put(const Unlocks *u)
{
    cg_g.unlock_m = u->variant_m, cg_g.unlock_r = u->variant_r, cg_g.lost_vegas = u->lost_vegas, cg_g.hidden = u->hidden;
    cg_g.warrior = (int16_t)u->warrior;
}

static FrontendSession session_of(void)
{
    FrontendSession f = { cg_g.unlock_level, cg_g.vegas_cheat, menu_words.link_game, cg_g.net_self, cg_g.net_player,
                          menu_words.intro_seen, cg_g.nomercy, { 0 }, cg_g.tournament, resource_german(), 0 };
    for (int i = 0; i < 6; i++) f.unknown_4bafb4[i] = cg_g.was[i];
    return f;
}

static void session_put(const FrontendSession *f)
{
    cg_g.vegas_cheat = f->unknown_4baf1c, cg_g.net_self = (int16_t)f->unknown_4faa56, cg_g.net_player = (int16_t)f->other_index;
    cg_g.nomercy = (int16_t)f->unknown_4c5cc2, cg_g.tournament = (int16_t)f->tournament_on;
    for (int i = 0; i < 6; i++) cg_g.was[i] = (int16_t)f->unknown_4bafb4[i];
}

/* port: the extras the player left on, over the flags the original owns (central.h names each address) */
static void extras_flags(ExtrasFlags *f)
{
    f->unlock_m = &cg_g.unlock_m;
    f->unlock_r = &cg_g.unlock_r;
    f->variant_m = &cg_g.variant_m;
    f->variant_r = &cg_g.variant_r;
    f->explode = &cg_g.explode;
    f->nitro = &cg_g.nitro;
    f->vegas_cheat = &cg_g.vegas_cheat;
    f->lost_vegas = &cg_g.lost_vegas;
    f->warrior = &cg_g.warrior;
    f->track_flags = (uint32_t *)&menu_cheats.keys_506;
}

/* port: the Warrior-as-rival switch is a separate flag (no equivalent in the original's globals), read by the
 * opponent arrows through ControlCentralState.warrior_rival. Kept up to date with the mode's file by extras_apply. */
static ExtrasSession extras_session;
static int16_t warrior_rival_flag, warrior_keep_flag;   /* and the Warrior kept as the player's car */

/* port: called after every place the original clears the flags, so a switch stays on across races and the
 * tournament. With the mode's file absent this reads the defaults and changes nothing. */
void cg_extras_apply(void)
{
    ExtrasFlags f;
    extras_flags(&f);
    const PortExtras e = port_extras_load();
    /* port: the level, the switches and the cheats typed in the session decide what is on: a switch turned off in
     * CUSTOMIZE takes its flag away, a cheat typed stays for the session and is never written to the mode's file (the
     * switch is the player's default, not a veto on the cheat); Mirror and Rally both on stay both on */
    extras_settle(&e, cg_g.unlock_level, &f, &extras_session);
    /* cheat_codes writes the globals, so they are read back and put into g again after the reapply */
    cg_g.unlock_m = *f.unlock_m, cg_g.unlock_r = *f.unlock_r, cg_g.variant_m = *f.variant_m, cg_g.variant_r = *f.variant_r;
    cg_g.explode = *f.explode, cg_g.nitro = *f.nitro, cg_g.vegas_cheat = *f.vegas_cheat, cg_g.lost_vegas = *f.lost_vegas;
    cg_g.warrior = *f.warrior;
    menu_cheats.keys_506 = (int32_t)*f.track_flags;
    if (e.no_assists) extras_no_assists(&cg_g.car_flag_a, &cg_g.car_flag_b);   /* after 00465ec8 */
    warrior_rival_flag = (int16_t)(e.warrior_rival != 0);
    warrior_keep_flag = (int16_t)(e.warrior != 0);
}

/* 00465ec8, 00432974, 00432c40, 00432d48 and 00432720 on the shorts */
static void car_flags(void *ctx)
{
    (void)ctx;
    RaceSettings s = cg_settings_of();
    race_settings_car_flags(&s);
    cg_settings_put(&s);
    cg_extras_apply();   /* port: after 00465ec8 / 00432974 */
}

static void type_changed(void *ctx)
{
    (void)ctx;
    RaceSettings s = cg_settings_of();
    Unlocks u = cg_unlocks_of();
    FrontendSession f = session_of();
    race_settings_type_changed(&s, &u, &f);
    cg_settings_put(&s), cg_unlocks_put(&u), session_put(&f);
    cg_extras_apply();   /* port: the type change clears the variants */
}

static void track_forward(void *ctx)
{
    (void)ctx;
    RaceSettings s = cg_settings_of();
    const Unlocks u = cg_unlocks_of();
    race_settings_track_forward(&s, &u);
    cg_settings_put(&s);
}

static void track_backward(void *ctx)
{
    (void)ctx;
    RaceSettings s = cg_settings_of();
    const Unlocks u = cg_unlocks_of();
    race_settings_track_backward(&s, &u);
    cg_settings_put(&s);
}

static int quit_tournament(int german, void *ctx)
{
    return glue_yes_no(ctx, german != 0 ? "Wettkampf beenden ?" : "Quit tournament ?");
}

static void step_race_type(void *ctx, int32_t cg_key)
{
    RaceSettings s = cg_settings_of();
    Unlocks u = cg_unlocks_of();
    FrontendSession f = session_of();
    race_settings_type_step(&s, &u, &f, cg_game_state, config_current(), cg_key, quit_tournament, ctx);
    cg_settings_put(&s), cg_unlocks_put(&u), session_put(&f);
}

/* 00432884 on the art's handles */
static void art_free(void *ctx)
{
    ControlCentralArt a;
    void **const slot[7] = { &a.art_4bafc0, &a.art_4bafc4, &a.art_4bafc8, &a.art_4bafcc, &a.art_4bafd0, &a.art_4bafd4,
                             &a.art_4bafd8 };
    for (int i = 0; i < 7; i++) *slot[i] = (void *)(intptr_t)cg_g.art[i];
    FrontendSession f = session_of();
    const ControlCentralArtOps ops = { free_handle, nothing, ctx };
    control_central_art_free(&a, &f, &ops);
    for (int i = 0; i < 7; i++) cg_g.art[i] = (int32_t)(intptr_t)*slot[i];
    session_put(&f);
}

/* Control Central holds 0052b316 and 004f81d2 as values, but in the original they are live words that 004104d0
 * changes (its 004465e4 rebuilds the seats as machines join): read again after each call while Central runs */
static ControlCentralState *central_state;
static void net_service(void *ctx)
{
    (void)ctx, link_sync();
    if (central_state != NULL) central_state->view_only = link_view_only(), central_state->players = link_humans();
}
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

/* config.dat's short at 00524df0 + at */
int16_t cg_image_short(int at)
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
    if (memcmp(cg_g.volumes, volumes_read, sizeof cg_g.volumes) != 0) memcpy(at, cg_g.volumes, sizeof cg_g.volumes);
    memcpy(cg_g.volumes, at, sizeof cg_g.volumes);
    memcpy(volumes_read, cg_g.volumes, sizeof volumes_read);
}

void cg_palette_set(void *ctx, uint8_t *rgb, int32_t first, int32_t count, const uint8_t *src, int32_t mode)
{
    (void)ctx;
    palette_put(rgb, first, count, src, mode, cg_screen_palette);
}
/* 00464a30: both pages black (screen_clear_both) */
void cg_flush(void *ctx)
{
    (void)ctx;
    for (int i = 0; i < 2; i++) ea_select_default(), video_clear_back(), video_flip();
}

/* The short 0050a5b6 is the link's stage word (0050a5ac[5], race_net_on_stage): a client's becomes 1 when the host
 * reaches 00410c18(1), and every screen of the menus leaves on it. The original's service thread keeps it current; here
 * each screen's loop calls its deadline op first, which serves the link and, for the screens that took the word by
 * value in their inputs, writes it there */
int *cg_stage_in;  /* the running screen's inputs' copy of it (results_screen.h, standings.h) */
int32_t cg_deadline_passed(void *ctx)
{
    (void)ctx;
    if (menu_words.link_game != 0) {
        link_service();
        if (cg_stage_in != NULL) *cg_stage_in = *stage_word();
    }
    return timer_deadline_passed();
}
int32_t cg_key(void *ctx)
{
    (void)ctx;
    for (int code = 0; code < CHEAT_KEY_FLAGS; code++) keys[code] = platform_key_down(code) ? 1 : 0;
    return input_get_key();
}

/* 004687d0's pointer: ctx is its DialogPointer */
void cg_poll_pointer(void *ctx)
{
    DialogPointer *const p = ctx;
    int x, y;
    pointer_sync_from_os();
    pointer_position(&x, &y);
    p->x = x, p->y = y;
}
void cg_mark_at(void *ctx, DialogPointer *p)
{
    (void)ctx;
    p->mark_x = (int16_t)p->x, p->mark_y = (int16_t)p->y;
    pointer_mark();
}

/* 004fbae0: a dword per track; 00437540 sets the nine to 1 (midday) as the game starts (00481f27) */
int32_t cg_time_of_day[10] = { 1, 1, 1, 1, 1, 1, 1, 1, 1 };

/* 00465ec8 on config.dat's image, which 004687d0 changes */
static void image_car_flags(void *ctx)
{
    (void)ctx;
    ConfigDat *const config = config_current();
    RaceSettings s = cg_settings_of();
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
    const RaceSettings before = cg_settings_of();
    config_dat_set_race(config, &before);
    const int32_t screen = ea_screen();
    const TrackSelectInputs in = { "", resource_kmh(), cg_g.variant_m, cg_g.variant_r, cg_g.lost_vegas, screen,
                                   ea_picture_shape(screen) };
    /* port: the weather row, and with the PS1 look the time-of-day item on its psx_time; both written to port.cfg
     * when the screen leaves them changed (the race reads them there, as it does today) */
    int weather = port_weather(), psx_time = port_psx_time();
    int reverse = reverse_track_picked();   /* port: the mode's file's reverse offers the row; the pick is the session's, not saved */
    const PortExtras extras = port_extras_load();
    TrackSelectPlus plus = { cg_g.unlock_m, cg_g.unlock_r, &cg_g.variant_m, &cg_g.variant_r,
                             extras.variant_time,   /* port: variants as entries */
                             port_settings_profile() == PROFILE_RESTOMOD, &weather, port_psx_light() ? &psx_time : NULL, extras.reverse ? &reverse : NULL };
    const int was_weather = weather, was_psx_time = psx_time;
    TrackSelectState st = { config->image + (0x524df9 - 0x524df0), cg_time_of_day, cg_g.inverse, cg_screen_palette, &plus };
    int x, y;
    pointer_position(&x, &y);
    DialogPointer p = { pointer_active(), 0, x, y, x, y };
    const TrackSelectOps ops = {
        cg_flush, move_to, load_bank, no_inverse, music_feed, shape, set_palette, nothing, styles, cg_poll_pointer, hide,
        in_rect, select_picture, select_default, blit, blit_masked, blit_word, style, shadowed, nothing, nothing, flip,
        show_dialog, cg_mark_at, buttons, track_music, fade_in, track_play, deadline_set, sound, ticks, activate_dialog,
        cg_key, click, track_stop, track_free, fade_out, free_bank, free_handle_inverse, best_times, image_car_flags,
        off_dialog, music,
        cg_deadline_passed, music_fade_out, music_end, &p };
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
        after.variant_m = cg_g.variant_m, after.variant_r = cg_g.variant_r;   /* port: as the screen left them (get_race zeroes them) */
        cg_settings_put(&after);
    }
}

void cg_fit(void *ctx, const char *text, int32_t x, int32_t y, int32_t width)
{
    (void)ctx;
    static const TextFitOps ops = { fit_width, fit_ramp, fit_draw, NULL };
    text_fit_shadowed(text, x, y, width, &ops);
}
void cg_average_text(void *ctx, char *out, int32_t time, int32_t distance)
{
    (void)ctx;
    stat_average_speed_text(out, resource_kmh(), time, distance);
}

/* 004676d8 from the track screen on config.dat's image and its records; ctx is the track screen's pointer */
static int32_t best_times(void *ctx)
{
    uint8_t *const image = config_current()->image;
    const int32_t screen = ea_screen();
    const BestTimesInputs in = { "", resource_german(), cg_g.lost_vegas, screen, ea_picture_shape(screen),
                                 image + (0x524f8f - 0x524df0) };
    BestTimesState st = { image + (0x524df9 - 0x524df0), cg_g.inverse, cg_screen_palette,
                          records_glue_best_plus(image, cg_g.variant_m, cg_g.variant_r) };   /* port: the tables with cheats */
    const BestTimesOps ops = {
        move_to, load_art, no_inverse, music_feed, shape, cg_flush, set_palette, nothing, styles, cg_poll_pointer, cg_mark_at,
        in_rect, select_picture, blit, blit_masked, blit_word, style, cg_fit, time_text, speed_text, shadowed,
        select_default, hide, nothing, nothing, flip, show_dialog, buttons, fade_in, deadline_set, sound, ticks,
        activate_dialog, cg_key, click, image_car_flags, fade_out, free_handle_inverse, free_bank, load_bank, off_dialog,
        music,
        cg_deadline_passed, music_fade_out, music_end, ctx, records_glue_toggle, plus_text, plus_text_width };   /* port: the cheats item */
    return best_times_screen(&in, &st, ctx, &ops);
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
static void own_note_text(void *ctx, const char *text, int32_t x, int32_t y);
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
    ControlsPlus plus = { &port, 1, port_settings_profile() != PROFILE_RESTOMOD };   /* port: the three pages, the exe's left out (input_read.c reads the keys and the pad together) */
    controls_out();
    const ControlsMenuState st = { controls_kept, &german, &drive_joy_state()->present, &no_joystick_b, &no_calibration,
                                   pointer_active_word(), &cg_g.buttons, screen, &cg_g.inverse, cg_word_52b302(), &quit,
                                   &plus };
    const ControlsMenuOps ops = {
        picture_new, cg_flush, palette_clear, move_to, load_art, no_inverse, music_feed, shape, cg_palette_set, fade_in,
        styles, poll, mark, in_rect, style, select_picture, blit, picture_shape, grab, key_name, shadowed, set_palette,
        select_default, flip, show, deadline_set, sound, nothing, blit_at, ticks, buttons, activate, cg_key, click, clash,
        glue_boxed, capture, cg_deadline_passed, music, fade_out, free_handle_inverse, no_mode, off, net_sync,
        picture_free, free_bank, ctx, plus_text, plus_text_width, plus_panel, controls_plus_save, pad_raw,
        central_page_arrow_size, central_page_arrow,
        /* port: the pages 2 and 3 over the clean background, with the title and the group headers as CUSTOMIZE's */
        controls_bgnd, glue_own_bgnd_shape, own_plus_text, own_title_text, own_header_text,
        /* port: the keyboard page's capture and key names, and the pages' subtitle */
        plus_key_down, plus_key_name, own_subtitle_text, own_note_text };
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
static int32_t audio_effects(void *ctx) { (void)ctx; return cg_image_short(0x79); }

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
        &german, &sound_on, &voice_4a, &voice, &audio_pointer_x, pointer_active_word(), &cg_g.buttons, ea_screen_slot(),
        &cg_g.inverse, &small_stream, cg_word_52b302(), stage_word() };
    const AudioMenuOps ops = {
        music_fade_out, music_end, audio_track_music, picture_new, move_to, load_art, no_inverse, music_feed, shape,
        cg_palette_set, poll, mark, audio_clock, audio_stop_4a, audio_4a_done, sound, in_rect, audio_volume,
        select_picture, blit, blit_at, audio_blit_at, audio_clip, picture_shape, select_default, hide, nothing, flip,
        show, audio_buttons, styles, fade_in, deadline_set, cg_deadline_passed, nothing, music, nothing, blit_masked,
        ticks, audio_activate, cg_key, cg_key, click, music_cut, audio_voice, audio_effects, music_fade_to,
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
void glue_plus_text_scaled(void *ctx, int32_t style_id, const char *text, int32_t x, int32_t y, int32_t num, int32_t den)
{
    (void)ctx;
    text_select((TextStyle)style_id);
    text_draw_smooth(text, x, y, num, den);
}
int32_t glue_plus_text_width_scaled(void *ctx, const char *text, int32_t num, int32_t den)
{
    (void)ctx;
    return text_width_scaled(text, num, den);
}
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
    memcpy(cg_screen_palette, own_palette, sizeof cg_screen_palette);
    cg_palette_set(ctx, cg_screen_palette, 0, 0x100, own_palette, 0x24 /* FSH_TYPE_PALETTE_8BIT */);
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

/* port: a small note over the Graphics-style background: own_plus_text's cover, at 3/5 of the size */
static void own_note_text(void *ctx, const char *text, int32_t x, int32_t y)
{
    enum { NUM = 3, DEN = 5, MARGIN = 2, SHADOW_ROOM = 6 };
    if (own_shape != NULL) {
        const int w = text_width_scaled(text, NUM, DEN) + MARGIN + SHADOW_ROOM, h = text_line_height() * NUM / DEN + MARGIN + SHADOW_ROOM;
        uint8_t *const cover = malloc(CONFIG_BGND_SHAPE_HEADER + (size_t)w * h);
        if (cover != NULL && config_bgnd_shape(own_pixels, x - MARGIN, y - MARGIN, w, h, cover) != 0) ea_blit_own(cover);
        free(cover);
    }
    glue_plus_text_scaled(ctx, 0, text, x, y, NUM, DEN);
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
    central_page_arrow(ctx, right, lit, x, y);
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
    const int cg_race = mode != 0;
    PortSettings port = port_settings_load();
    const PortSettings was = port;
    int desktop_w = 0, desktop_h = 0;
    const int has_desktop = platform_desktop_size(&desktop_w, &desktop_h);
    GraphicsPlus plus = { &port, has_desktop ? screen_format_nearest(desktop_w, desktop_h) : SCREEN_4_3, cg_race, port.f3_ultra };
    opt_in();
    /* after opt_in: it writes Options' unsaved edits into the image, which Cancel must keep */
    port_settings_was = port;
    memcpy(config_current_kept_image, config_current()->image + 9, GRAPHICS_KEPT);
    /* port: the palette that brought this screen up, kept: it fades the whole display out as it closes (00423d28's
     * close_screen), so the styles rebuilt after it must be built from this copy rather than from what is left on the
     * display -- which is that palette faded down to black, and a ramp built from black is not the caller's menu. */
    uint8_t caller_palette[PALETTE_PUT_BYTES];
    memcpy(caller_palette, cg_screen_palette, sizeof caller_palette);
    const GraphicsMenuState st = { config_current()->image + 9, &showing, cg_word_52b302(), pointer_active_word(), &cg_g.buttons,
                                   ea_screen_slot(), &cg_g.inverse, stage_word(), &plus };
    const GraphicsMenuOps ops = {
        move_to, load_art, no_inverse, music_feed, shape, cg_palette_set, poll, mark, in_rect, select_picture, blit,
        picture_shape, select_default, hide, nothing, flip, show, buttons, styles, fade_in, deadline_set, sound,
        nothing, ticks, activate, cg_key, click, off, music, net_sync, cg_deadline_passed, fade_out, free_handle_inverse,
        free_bank, ctx, plus_text, plus_text_width, plus_panel, plus_save,
        /* port: the screen's own background, its shape with the palette that goes with it, and its title. The race's
         * pause menu gets the same ones as Options. */
        own_bgnd, own_bgnd_shape, own_title_text, own_put_back,
        /* port: the letters go through the draw that restores the clean background under them */
        own_plus_text,
        /* port: whether the display is already dark, so the screen skips a fade of a display Options has already
         * faded out rather than scaling zeros for seventeen vblanks */
        display_dark, central_page_arrow_size, central_page_arrow };
    input_function_keys(&showing);
    input_page_keys(!cg_race);
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

static void game_load(void *ctx, const char *name) { (void)ctx; game_file_load(name, cg_game_state, &save_files_ops); }

/* 0042ae9c on config.dat's image and the menus' game state */
static int32_t game_list(void *ctx, int32_t mode)
{
    uint8_t *const image = config_current()->image;
    GameListState st = { resource_german(), list_check, list_highlight, ea_screen(), "", "", list_names,
                         pointer_active_word(), &cg_g.buttons, image + 9, (char *)image + 0x3b, cg_game_state, game_kept,
                         image + CONTROLS_AT, &cg_g.lost_vegas, &cg_g.tournament };
    const GameListOps ops = {
        styles, picture_new, picture_shape, select_picture, grab, load_art, music_feed, no_cwd, no_chdir, find_first,
        find_next, clip, move_to, ticks, in_rect, sound, deadline_set, shape, audio_blit_at, style, cg_fit, masked_at,
        select_default, nothing, blit_word, blit_at, nothing, flip, show, cg_deadline_passed, nothing, buttons,
        activate, music, cg_key, click, glue_yes_no, remove_save, glue_banner_wait, game_load, music_fade_to,
        redraw_pointer, off, blit_masked, picture_free, free_bank, ctx };
    return game_list_screen(mode, &st, &ops);
}
static void save_box(void *ctx, char *name)
{
    (void)ctx;
    glue_save_box(name, config_current()->image, cg_game_state, (int16_t)cg_g.lost_vegas);
}
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
    const TrackViewerMenus menus = { 1, 1, 1, 0, 0, cg_image_short(0x73), cg_image_short(0x75), cg_image_short(0x79),
                                     cg_image_short(0x71), NULL, 0, 0, 0, 0, 0, 0, 0, NULL, -1,
                                     config_current()->image + OPTIONS_AT };  /* the time of day and the flags: the replay's setup */
    menu_audio_close();
    track_viewer_drive_menus(&menus);
    if (!track_viewer_drive_replay_data(resource_root(), file, size, replay_kind == 2 ? HIGHLIGHTS : FROM_START))
        port_log("load_save: the replay %s could not play\n", name);
    menu_audio_open();
    platform_free_file(file);
}

/* 0042c498 from Options (mode 0) on config.dat's image, its shorts copied around it to aligned ones */
static void load_save(void *ctx, int32_t mode)
{
    ConfigDat *const config = config_current();
    cg_level_to_image();
    opt_in();
    int16_t variant = cg_image_short(0x93);
    replay_kind = cg_image_short(0xaf);
    volumes_sync();
    LoadSaveState st = {
        resource_german(), cg_g.tournament, menu_words.link_game, cg_image_short(0x6d), pointer_active(), &cg_g.buttons,
        ea_screen_slot(), &cg_g.inverse, &list_check, &variant, &list_highlight, &replay_kind, replay_race,
        menu_audio_silent(), cg_g.volumes, KEPT_VOLUMES, cg_image_short(0x7f),
        (const char *)config->image + 9, (const char *)config->image + 0x3b };
    const LoadSaveOps ops = {
        move_to, load_art, no_inverse, music_feed, palette_clear, shape, cg_palette_set, poll, mark, in_rect,
        select_picture, blit, select_default, hide, nothing, picture_shape, nothing, flip, show, buttons, fade_in,
        styles, deadline_set, sound, ticks, activate, cg_key, click, game_list, save_box, replay_box,
        free_handle_inverse, free_bank, music_fade_out, fade_out, music_end, picture_free, sound_free, clear_one,
        clip, replay, nothing, set_palette, no_mode_set, picture_new, sound_load, music_pick, off, music,
        cg_deadline_passed, ctx };
    int16_t *const was = pointer_bind_active(&st.pointer_active);
    load_save_menu(mode, &st, &ops);
    volumes_sync();
    pointer_bind_active(was);
    memcpy(config->image + 0x93, &variant, sizeof variant), memcpy(config->image + 0xaf, &replay_kind, sizeof replay_kind);
    opt_out();
    cg_g.unlock_level = (int16_t)config_dat_unlock_level(config);
    cg_races_from_state();
}

/* load_save_menu2 (0042d894) from 00403760 (mode 1, check/'s art) on config.dat's image, as load_save; its tournament's races (the
 * game state's +0x592..+0x5a0, Lost Vegas's +0x5a2) copied to aligned shorts */
static void load_save2(void *ctx, int32_t mode)
{
    cg_level_to_image();
    enum { RACES_AT = 0x592, EXTRA_AT = 0x5a2, RACES = 8 };
    ConfigDat *const config = config_current();
    opt_in();
    int16_t races[RACES], extra;
    memcpy(races, cg_game_state + RACES_AT, sizeof races), memcpy(&extra, cg_game_state + EXTRA_AT, sizeof extra);
    replay_kind = cg_image_short(0xaf);
    volumes_sync();
    LoadSave2State st = {
        resource_german(), cg_g.tournament, menu_words.link_game, cg_image_short(0x6d), pointer_active(), &cg_g.buttons, races,
        extra, cg_g.lost_vegas, ea_screen_slot(), &cg_g.inverse, &list_check, &list_highlight, &replay_kind, menu_audio_silent(),
        cg_g.volumes, KEPT_VOLUMES, cg_image_short(0x7f), (const char *)config->image + 9,
        (const char *)config->image + 0x3b };
    const LoadSaveOps ops = {
        move_to, load_art, no_inverse, music_feed, palette_clear, shape, cg_palette_set, poll, mark, in_rect,
        select_picture, blit, select_default, hide, nothing, picture_shape, nothing, flip, show, buttons, fade_in,
        styles, deadline_set, sound, ticks, activate, cg_key, click, game_list, save_box, replay_box,
        free_handle_inverse, free_bank, music_fade_out, fade_out, music_end, picture_free, sound_free, clear_one,
        clip, replay, nothing, set_palette, no_mode_set, picture_new, sound_load, music_pick, off, music,
        cg_deadline_passed, ctx };
    int16_t *const was = pointer_bind_active(&st.pointer_active);
    load_save_menu2(mode, &st, &ops);
    volumes_sync();
    pointer_bind_active(was);
    memcpy(config->image + 0xaf, &replay_kind, sizeof replay_kind);
    opt_out();
    cg_g.unlock_level = (int16_t)config_dat_unlock_level(config);
    cg_races_from_state();
}

/* check_options (00403760) on config.dat's image (the sub-screens' shorts taken from it first): the options after a race, mode
 * 0 from 0040b648, 1 from 0040c710 */
int32_t glue_check_options(int32_t mode)
{
    ConfigDat *const config = config_current();
    opt_out();
    const CheckOptionsOps ops = {
        move_to, load_art, no_inverse, shape, cg_palette_set, poll, mark, in_rect, select_picture, blit, select_default,
        hide, picture_shape, flip, show, buttons, fade_in, deadline_set, sound, nothing, ticks, activate, cg_key, click,
        fade_out, free_handle_inverse, free_bank, audio, graphics, controls, load_save, load_save2, music_fade_to,
        off, music, cg_deadline_passed, no_wait, music_feed, save_config, NULL };
    const int16_t language = (int16_t)resource_german();
    const CheckOptionsState st = { config->image, &menu_words.link_game, &language, pointer_active_word(), &cg_g.buttons,
                                   ea_screen_slot(), &cg_g.inverse, stage_word() };
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

enum { REPLAY_FLAG_AT = 0xaf };

/* 0040c710's words on config.dat's image and the menus' game state; the cars' points (0051ce24) kept
 * in a buffer of their own: the port's replay leaves car_points */
static AfterRaceState after_race_state(void)
{
    static uint8_t cars[AFTER_RACE_CARS * AFTER_RACE_CAR];
    uint8_t *const image = config_current()->image;
    replay_kind = cg_image_short(REPLAY_FLAG_AT);
    volumes_sync();
    return (AfterRaceState){
        &menu_words.link_game, stage_word(), pointer_active_word(), &cg_g.buttons, ea_screen_slot(), &cg_g.inverse,
        &cg_credits_idle, &menu_words.redraw, &replay_kind, (char *)image + 0x3b, menu_audio_silent(), cg_g.volumes,
        KEPT_VOLUMES, cg_image_short(0x7f), cg_image_short(0x95), cg_image_short(0x6d),
        cg_game_state, cars };
}

static void chat_send(void *ctx, int32_t link, const uint8_t *message, int32_t size)
{
    (void)ctx;
    link_send((uint32_t)link, message, size);
}
/* 004735dc draws chat's screen straight onto the shown page (it never flips): here onto the back buffer, which is
 * shown before the pointer goes over it */
static void chat_in(void *ctx, int32_t link, const uint8_t *message, int32_t size)
{
    (void)ctx, (void)size;
    link_chat_in((uint32_t)link, message);
}

/* 0040a5d8 on the link game's lines */
void cg_chat(void *ctx)
{
    static const ChatOps ops = {
        load_art, shape, cg_palette_set, select_picture, blit, blit_masked, select_default, picture_shape, blit_word, flip,
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
                     pointer_active_word(), &cg_g.buttons, ea_screen_slot() };
    (void)ctx;
    chat_screen(&st, &ops);
}

static AfterRaceOps after_race_ops(void (*options)(void *, int32_t), void *ctx)
{
    return (AfterRaceOps){
        move_to, load_art, no_inverse, music_feed, palette_clear, shape, cg_palette_set, poll, mark, net_service, in_rect,
        chat_waiting, select_picture, blit, blit_masked, select_default, hide, picture_shape, flip, show, buttons, fade_in,
        styles, deadline_set, sound, nothing, ticks, activate, cg_key, click, music_fade_out, fade_out, music_end,
        free_bank, free_handle_inverse, picture_free, sound_free, clear_one, clip, replay, nothing, set_palette,
        no_mode_set, no_mode, picture_new, sound_load, music_pick, options, cg_standings, cg_circuit_results,
        cg_segment_results, cg_track_results, cg_chat, off, cg_credits_tick, cg_deadline_passed, ctx };
}

/* 0040c710 for 00427ae0 */
void cg_after_race(void *ctx)
{
    AfterRaceState st = after_race_state();
    const AfterRaceOps ops = after_race_ops(after_options, ctx);
    cg_credits_last = timer_ticks();
    after_race_menu(&st, &ops);
    volumes_sync();
    memcpy(config_current()->image + REPLAY_FLAG_AT, &replay_kind, sizeof replay_kind);
}

/* 0040b648 for 00427ae0: the checkpoint menu between segments, 0 to stop the tournament's run */
int32_t cg_next_segment(void *ctx)
{
    int16_t *const over = &link_game.flags_a[RACE_NET_STAGE_FLAG];  /* 0050a5b6: read, and cleared once the others came */
    NextSegmentState st = { after_race_state(), over, &menu_words.link_lost, (int16_t)resource_german() };
    st.menu.link_over = over;
    const NextSegmentOps ops = { after_race_ops(after_options, ctx), glue_yes_no, glue_banner, deadline, all_came };
    cg_credits_last = timer_ticks();
    const int32_t go_on = next_segment_menu(&st, &ops);
    volumes_sync();
    memcpy(config_current()->image + REPLAY_FLAG_AT, &replay_kind, sizeof replay_kind);
    return go_on;
}

/* port: the Extras row opens the EXTRAS screen over this screen's picture; Done writes the mode's file and the unlock
 * level back into config.dat, and extras_apply re-reads it for Control Central (the cheats, mirror, ghosts). */
static void options_extras(void *ctx)
{
    (void)ctx;
    ConfigDat *const config = config_current();
    glue_extras_screen((int16_t)config_dat_unlock_level(config));
    cg_g.unlock_level = (int16_t)config_dat_unlock_level(config);
    cg_extras_apply();
}

static void options(void *ctx)
{
    ConfigDat *const config = config_current();
    const RaceSettings before = cg_settings_of();
    config_dat_set_race(config, &before);
    opt_out();
    const OptionsMenuOps ops = {
        move_to, load_art, no_inverse, music_feed, shape, cg_palette_set, styles, poll, mark, in_rect, select_picture,
        blit, select_default, hide, nothing, picture_shape, nothing, flip, show, buttons, fade_in, deadline_set, sound,
        ticks, activate, cg_key, click, fade_out, free_handle_inverse, free_bank, audio, graphics, controls, load_save,
        music_fade_out, music_end, music_start, cg_credits, music_pick, opt_car_flags, music_fade_to, off, music, net_sync,
        cg_deadline_passed, ctx, options_extras, central_options_plus_text, plus_text_width, plus_panel };
    OptionsMenuState st = {
        &opt[OPT_TRANSMISSION], &opt[OPT_AUTOMATIC], &opt[OPT_ABS], &opt[OPT_TRACTION], &opt[OPT_RACE_TYPE],
        &opt[OPT_TRACK], &opt[OPT_OPPONENT], &opt[OPT_CAR], &opt[OPT_NET_CAR], &opt[OPT_LAPS], &opt[OPT_DIFFICULTY],
        &opt[OPT_MUSIC_VOLUME], &menu_words.link_game, pointer_active_word(), &cg_g.buttons, ea_screen_slot(), &cg_g.inverse,
        cg_word_52b302(), stage_word(), &menu_words.redraw };
    options_menu(&st, &ops);
    opt_in();
    RaceSettings after = before;
    if (config_dat_get_race(config, &after)) cg_settings_put(&after);
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
    cg_settings_put(&s);
    cg_g.unlock_level = (int16_t)config_dat_unlock_level(config);
    cg_races_from_state();
}

/* 00466ae4 before Control Central: the game state for a new tournament (the controls, the names: config.dat's for
 * player 0, the computer players' for the rest), the race settings read from config.dat and put back to it */
void glue_game_reset(void)
{
    load();
    RaceSettings s = cg_settings_of();
    FrontendSession f = session_of();
    tournament_new(cg_game_state, &s, config_current(), &f);
    cg_settings_put(&s);
    session_put(&f);
    config_dat_set_race(config_current(), &s);
    /* the original's settings are config.dat's own words, so tournament_reset's variant = 0 clears 00524e83 */
    config_dat_new_session(config_current());
}

/* 00408724 on Control Central's words */
static void showcase(void *ctx)
{
    const ShowcaseGlue in = { &cg_g.player_car, &cg_g.race_type, &cg_g.track, pointer_active_word(), &cg_g.buttons, car_flags, ctx };
    glue_showcase(&in);
}

static int central_up;  /* the race settings live in g while Control Central runs */

/* A window closed in the menus (port: the original quits only through them): the race settings into config.dat's
 * image as on leaving Control Central, while it runs */
void glue_settings_to_config(void)
{
    if (!central_up) return;
    const RaceSettings s = cg_settings_of();
    config_dat_set_race(config_current(), &s);
    cg_level_to_image();
}

int32_t glue_central(void)
{
    static const ControlCentralOps OPS = {
        pump, palette_clear, move_to, music, load_art, picture_new, sound_load, no_inverse, music_feed, mark, shape,
        cg_palette_set, net_service, in_rect, car_flags, type_changed, ticks, chat_waiting, select_picture, blit, blit_masked,
        blit_word, picture_shape, width, style, shadowed, frame, free_bank, load_bank, text_at, select_default, flip,
        show, buttons, fade_in, nothing, styles, sound, deadline_set, cg_deadline_passed, cg_key, scancode, cheat_step,
        key_down, activate, net_start, click, kick, music_fade_out, fade_out, music_end, music_start, showcase,
        music_pick, step_race_type, track_forward, track_backward, nothing, cg_standings, track_select, art_free, options,
        cg_chat, glue_yes_no, glue_drain, glue_banner, pre_race, sound_free, picture_free, cg_flush, cg_race, off,
        plus_text, NULL, 1, pad_buttons, palette_colour };
    load();
    ControlCentralState st = {
        &cg_g.race_type, &cg_g.opponent, &cg_g.track, &cg_g.player_car, cg_g.other_car, &cg_g.net_car, &cg_g.laps, cg_g.difficulty,
        menu_words.mode_menu, cg_g.volumes, menu_words.link_game, link_view_only(), &menu_words.redraw, (int16_t)resource_german(), 0,
        &cg_g.tournament, &menu_words.link_lost, cg_g.races, menu_audio_kept(), (uint32_t *)&menu_cheats.keys_506,
        &cg_g.unlock_level, &cg_g.unlock_m, &cg_g.unlock_r, &cg_g.lost_vegas, &cg_g.vegas_cheat, &cg_g.net_player, &cg_g.net_player_was,
        cg_g.was, cg_g.art, &cg_g.variant_m, &cg_g.variant_r, menu_words.single, menu_words.intro_seen, &cg_g.nomercy, &cg_g.warrior,
        &cg_g.explode, menu_audio_silent(), &cg_g.inverse, &cg_g.nitro, &cg_g.net_hover, &cg_g.net_self, &cg_g.net_second, &cg_g.letter_held,
        link_humans(), link_names(), pointer_active_word(), &cg_g.buttons, ea_screen_slot(), "", keys, &warrior_rival_flag, &warrior_keep_flag };
    const LinkMenu link_menu = { &cg_g.race_type, &cg_g.track, &cg_g.laps, &cg_g.difficulty, &cg_g.net_car, &cg_g.nomercy, &cg_g.tournament,
                                 &cg_g.player_car, cg_g.other_car, &cg_g.variant_m, &cg_g.variant_r };
    link_central_bind(&link_menu);
    cg_extras_apply();   /* port: the switches are on from the first key, not after the first car change */
    central_up = 1, central_state = &st;
    volumes_sync();
    const int32_t result = control_central(&st, &OPS);
    volumes_sync();
    central_up = 0, central_state = NULL;
    link_central_bind(NULL);
    cg_extras_apply();   /* port: 00432ff4's start clears the cheats; the switches come back for the race */
    video_set_overlay(NULL, 0, 0);
    const RaceSettings s = cg_settings_of();
    config_dat_set_race(config_current(), &s);
    cg_level_to_image();
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
    RaceSettings s = cg_settings_of();
    if (track >= 0) s.track = track, s.variant = segment;  /* g.variant: the same short */
    s.variant_m = variant_m, s.variant_r = variant_r;  /* 004fbad8 / 004fbadc, as the cheats and track select set them */
    const char *const time = getenv("TNFS_DRIVE_TIME");  /* port: the track screen's time of day, 0..2 */
    if (time != NULL && track >= 0 && track < 9) cg_time_of_day[track] = atoi(time);
    if (car >= 0) s.player_car = car;
    if (race_type >= 0) s.race_type = race_type;
    cg_settings_put(&s);
    cg_race(NULL);
    const RaceSettings after = cg_settings_of();
    config_dat_set_race(config_current(), &after);
}

