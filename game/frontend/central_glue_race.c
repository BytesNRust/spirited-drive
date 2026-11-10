/* The Control Central glue's race start, tournament steps and credits (central_glue.c's third part). */
#include "game/frontend/central_glue_private.h"

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
    RaceSettings s = cg_settings_of();
    tournament_reset(cg_game_state, &s);
    cg_settings_put(&s);
    cg_extras_apply();   /* port: the tournament's start clears the variants; the switch comes back */
}
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
    const WinMovieState st = { cg_game_state, cg_last_drive.player, cg_last_drive.racers, link_view_only(), cg_last_drive.dropped, cg_g.race_type,
                               cg_g.track, cg_g.player_car, cg_g.lost_vegas, ea_screen_slot() };
    win_movie(&st, &(const CopStopOps){ picture_new, screen_file, picture_free, pick, movie, ctx });
}
static void points(void *ctx) { (void)ctx; tournament_points(cg_game_state, cg_car_points); }
static int32_t champion(void *ctx, int32_t player) { (void)ctx; return tournament_champion(cg_game_state, player); }

/* the short 004c5cc4 while 00427ae0 runs */
static int32_t race_warrior;

/* 00432e48 at a tournament won: the unlocks raised to config.dat's level, as 00427ae0 just set it */
static void raise_unlocks(void *ctx)
{
    (void)ctx;
    Unlocks u = cg_unlocks_of();
    u.warrior = race_warrior;
    unlocks_raise(&u, config_dat_unlock_level(config_current()));
    cg_unlocks_put(&u);
    race_warrior = cg_g.warrior;
}

/* 00427ae0 (menu_race.c) on config.dat's image and the menus' game state: alone, this machine's player 0; in a link
 * game, the link game's words */
static void race_finished(void *ctx);
void cg_race(void *ctx)
{
    (void)ctx;
    cg_extras_apply();   /* port: a tournament's start (central_select.c) clears rally, explode, warrior and turbo */
    ConfigDat *const config = config_current();
    const RaceSettings before = cg_settings_of();
    config_dat_set_race(config, &before);
    cg_level_to_image();
    ea_select_default();
    ea_screen_release();  /* Control Central freed it; a picture it left is freed here, not lost from the pool */
    static int32_t ghost[MENU_RACE_GHOST];
    static const int16_t no_link = 0;
    static const int32_t player = 0, font = 0, host = 0;
    int16_t link_done = 0;
    race_warrior = cg_g.warrior;
    const int linked = menu_words.link_game != 0;
    const MenuRaceState st = {
        config->image, cg_game_state, cg_word_52b302(), cg_race_times, ghost,
        linked ? &menu_words.link_game : &no_link, linked ? &menu_words.link_lost : &no_link, pointer_active_word(),
        linked ? &menu_words.single : &player, &font, ea_screen_slot(), &race_warrior, &cg_g.unlock_m, &cg_g.unlock_r,
        &cg_g.lost_vegas, linked ? &menu_words.intro_seen : &host, linked ? &link_game.flags_a[7] : &link_done };
    const MenuRaceOps ops = {
        no_mode, hide, nothing, clip, nothing, cg_drive, no_mode, nothing, set_palette, no_two, no_three, no_mode,
        nothing, music_pick, cops, won, sound_load, race_picture, cg_circuit_results, cg_segment_results, cg_standings, cg_track_results,
        points, cg_after_race, cg_next_segment, reset_tournament, champion, raise_unlocks, game_reset, ctx, race_finished };
    menu_race(&st, &ops);
    cg_g.warrior = (int16_t)race_warrior;
    int32_t *const slot = ea_screen_slot();
    if (*slot != 0) ea_picture_free(*slot);  /* Control Central makes its own next */
    *slot = 0;
    RaceSettings after = before;
    if (config_dat_get_race(config, &after)) cg_settings_put(&after);
    cg_g.unlock_level = (int16_t)config_dat_unlock_level(config);
    cg_races_from_state();
    cg_extras_apply();   /* port: get_race zeroes the variants; the next race from the same Control Central keeps them */
}

/* port: a race run to its end (not one given up or lost): the first one of an install is counted, if the switch is On */
static void race_finished(void *ctx)
{
    (void)ctx;
    port_stats_event(STATS_FIRST_RACE);
}

/* 004bb8dc: counted up by the timer (0043b9e8); here by the ticks gone between two of 004166e0's music calls */
int32_t cg_credits_idle;
uint32_t cg_credits_last;
void cg_credits_tick(void *ctx)
{
    (void)ctx;
    const uint32_t now = timer_ticks();
    cg_credits_idle += (int32_t)(now - cg_credits_last);
    cg_credits_last = now;
    menu_audio_music_service();
}

static void credits_extra(void *ctx)
{
    (void)ctx;
    credits_extra_show(TNFS_VERSION, credits_service);
}

/* 004166e0 from Options */
void cg_credits(void *ctx, int32_t mode)
{
    (void)mode;
    const CreditsOps ops = { load_art, shape, cg_palette_set, music_feed, select_picture, blit, select_default,
                             picture_shape, blit_at, nothing, flip, fade_in, cg_credits_tick, read_buttons, cg_key, fade_out,
                             free_handle_inverse, music_fade_out, music_end, free_bank, mark, ctx, credits_extra };
    const CreditsState st = { ea_screen_slot(), pointer_active_word(), &cg_g.buttons, stage_word(), &cg_credits_idle };
    cg_credits_last = timer_ticks();
    credits_show(&st, &ops);
}

