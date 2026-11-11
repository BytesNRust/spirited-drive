/* Control Central's glue for the saved games, the replays and the menus after a race and between segments (split from central_glue.c). */
#include "game/frontend/central_glue_private.h"

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
void cg_load_save(void *ctx, int32_t mode)
{
    ConfigDat *const config = config_current();
    cg_level_to_image();
    cg_opt_in();
    int16_t variant = cg_image_short(0x93);
    replay_kind = cg_image_short(0xaf);
    cg_volumes_sync();
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
    cg_volumes_sync();
    pointer_bind_active(was);
    memcpy(config->image + 0x93, &variant, sizeof variant), memcpy(config->image + 0xaf, &replay_kind, sizeof replay_kind);
    cg_opt_out();
    cg_g.unlock_level = (int16_t)config_dat_unlock_level(config);
    cg_races_from_state();
}

/* load_save_menu2 (0042d894) from 00403760 (mode 1, check/'s art) on config.dat's image, as cg_load_save; its tournament's races (the
 * game state's +0x592..+0x5a0, Lost Vegas's +0x5a2) copied to aligned shorts */
void cg_load_save2(void *ctx, int32_t mode)
{
    cg_level_to_image();
    enum { RACES_AT = 0x592, EXTRA_AT = 0x5a2, RACES = 8 };
    ConfigDat *const config = config_current();
    cg_opt_in();
    int16_t races[RACES], extra;
    memcpy(races, cg_game_state + RACES_AT, sizeof races), memcpy(&extra, cg_game_state + EXTRA_AT, sizeof extra);
    replay_kind = cg_image_short(0xaf);
    cg_volumes_sync();
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
    cg_volumes_sync();
    pointer_bind_active(was);
    memcpy(config->image + 0xaf, &replay_kind, sizeof replay_kind);
    cg_opt_out();
    cg_g.unlock_level = (int16_t)config_dat_unlock_level(config);
    cg_races_from_state();
}

enum { REPLAY_FLAG_AT = 0xaf };

/* 0040c710's words on config.dat's image and the menus' game state; the cars' points (0051ce24) kept
 * in a buffer of their own: the port's replay leaves car_points */
static AfterRaceState after_race_state(void)
{
    static uint8_t cars[AFTER_RACE_CARS * AFTER_RACE_CAR];
    uint8_t *const image = config_current()->image;
    replay_kind = cg_image_short(REPLAY_FLAG_AT);
    cg_volumes_sync();
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
        cg_net_service, in_rect, sound, lines_free, style, restore, shadowed, show_at, pump, lines_done, cg_key_read,
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
        move_to, load_art, no_inverse, music_feed, palette_clear, shape, cg_palette_set, poll, mark, cg_net_service, in_rect,
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
    cg_volumes_sync();
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
    cg_volumes_sync();
    memcpy(config_current()->image + REPLAY_FLAG_AT, &replay_kind, sizeof replay_kind);
    return go_on;
}
