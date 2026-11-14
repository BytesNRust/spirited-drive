/* The Control Central glue's best times, results and standings screens (central_glue.c's second part). */
#include "game/frontend/central_glue_private.h"

/* 004fa0cc..: the segments' top speeds and totals (menu_race fills them); 0051dccc and 0051dcec..: 0045b6c8's;
 * the shorts 0051ce24 + 0x1d2 x player, the cars' points (0045aec8's) */
int32_t cg_race_times[MENU_RACE_TIMES];
static int32_t finish[GAME_STATE_PLAYERS];
static TournamentOrderWork order_work;
int16_t cg_car_points[GAME_STATE_PLAYERS];

/* 0045bad8, 0045be50, 0045c2d0 and 0045b908 on config.dat's image and the menus' game state, this machine's player
 * 0; 0045b73c, 0045b6c8 on the game state */
/* the records the finishing player takes are this machine's (004fbb08: the slot in a link game, 0 alone) */
/* the records a results screen shows: its image is config.dat's with the race's table in its track block, refreshed after
 * each check so that the row a check set is the row shown (records_glue_shown_as) */
static uint8_t shown_image[RECORD_CHECK_IMAGE];
static ResultsMarks shown_marks;   /* port: the cheats of the same table, refreshed with it */
static int32_t records_checked(RouteOp op, int32_t *times)
{
    const int32_t answer = records_glue_route(op, config_current()->image, cg_game_state, menu_words.single, times);
    records_glue_shown_as(op, shown_image, config_current()->image, &shown_marks);
    return answer;
}
static int32_t records(void *ctx) { (void)ctx; return records_checked(ROUTE_SEGMENT, NULL); }
static int32_t totals(void *ctx) { (void)ctx; return records_checked(ROUTE_TOTALS, cg_race_times); }
static int32_t top_record(void *ctx) { (void)ctx; return records_checked(ROUTE_TOP, NULL); }
static int32_t lap_record(void *ctx) { (void)ctx; return records_checked(ROUTE_LAP, NULL); }
static int32_t leads(void *ctx) { (void)ctx; return tournament_leads(cg_game_state, menu_words.single); }
static void results_order(void *ctx) { (void)ctx; tournament_results_order(cg_game_state, finish, &order_work); }

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

/* 0045cea0, 0045de08 or 0045eff4 after a race, on the drive's results and config.dat's image, on 00427ae0's screen
 * picture */
static void results(ResultsShow *screen_of, const TrackViewerResults *r, int32_t mode)
{
    static const char *const CAR_NAMES[RESULTS_CARS] = { "RX-7", "NSX", "SUPRA", "911", "ZR-1", "VIPER",
                                                         "512TR", "DIABLO", "PACK", "CLOCK", "CLASS", "WARRIOR" };
    const int32_t screen = ea_screen();
    records_glue_shown_as(ROUTE_SEGMENT, shown_image, config_current()->image, &shown_marks);
    ResultsInputs in = { cg_game_state, shown_image, r->dropped, menu_words.single, r->place, r->rival_place,
                               r->rival, r->rival_car, resource_german(), resource_kmh(), 0, screen, ea_picture_shape(screen),
                               CAR_NAMES, cg_g.lost_vegas, cg_g.variant_r, r->player_place, r->shown_place, finish, &shown_marks };
    ResultsState st = { cg_g.inverse, cg_screen_palette, 0 };
    int x, y;
    pointer_position(&x, &y);
    ResultsCtx c = { { pointer_active(), 0, x, y, x, y }, &in, &st, NULL };
    const ResultsOps ops = {
        records, cg_flush, move_to, load_art, no_inverse, music_feed, shape, fade_in, styles, cg_poll_pointer, in_rect,
        select_picture, blit, style, shadowed, cg_fit, time_text, cg_average_text, speed_text, set_palette, nothing,
        select_default, hide, nothing, blit_word, nothing, flip, show_dialog, cg_mark_at, buttons, track_music, track_play,
        deadline_set, sound, ticks, activate_dialog, cg_key, click, off_dialog, music, cg_deadline_passed, track_stop, track_free, fade_out,
        free_bank, free_handle_inverse, blit_masked, totals, top_record, leads, best_totals, lap_record,
        results_order, lap_times, &c };
    c.ops = &ops;
    cg_stage_in = &in.unknown_50a5b6;
    screen_of(mode, &in, &st, &c.p, &ops);
    cg_stage_in = NULL;
}

/* 004275f8 for 00427ae0: the drive on argv[1]'s track (its stem in capitals) in the menus' window, the options from
 * config.dat (track_viewer.h's TrackViewerMenus; Options' 0 is on, 1 off, 2 none); its end (0052b2fe = 00509d6c,
 * 0 when not over) and results kept for 0045cea0 */
TrackViewerResults cg_last_drive;
void cg_drive(void *ctx, int32_t argc, char *const *argv)
{
    (void)ctx, (void)argc;
    char name[8] = { 0 };
    for (size_t i = 0; i + 1 < sizeof name && argv[1][i] != 0; i++)
        name[i] = (char)(argv[1][i] >= 'a' && argv[1][i] <= 'z' ? argv[1][i] - 0x20 : argv[1][i]);
    /* 004275f8: 004fbadc on track 3 races "tr5" (004ae1e0); port: with both variants on — g's own pair, the state
     * being raced, not the mode's file — the rally's own track is the one raced, the mirror keeping the geometry
     * (variant_rule.h) */
    const char *const stem = rally_track_stem(name, cg_g.variant_m, cg_g.variant_r, cg_g.variant_m != 0 && cg_g.variant_r != 0);
    if (stem != name) memcpy(name, stem, 4);
    const TrackViewerMenus menus = { cg_image_short(0x97) == 0, cg_image_short(0x99) == 0, cg_image_short(0x8f) == 0, cg_g.laps,
                                     race_head_to_head_model((int16_t)cg_g.opponent), cg_image_short(0x73), cg_image_short(0x75), cg_image_short(0x79),
                                     cg_image_short(0x71), cg_game_state,
                                     cg_g.track >= 0 && cg_g.track < 9 ? cg_time_of_day[cg_g.track] : 1, cg_g.variant_m, cg_g.variant_r,
                                     (uint32_t)menu_cheats.keys_506, cg_g.explode, cg_g.nitro, port_extras_load().replay_cameras,
                                     (const char *)config_current()->image, cg_g.track,
                                     config_current()->image + OPTIONS_AT };
    records_glue_begin(config_current()->image, cg_g.race_type, cg_game_state + 0x592, cg_g.variant_m, cg_g.variant_r, cg_g.explode, cg_g.nitro,
                       (menu_cheats.keys_506 & 8) != 0);   /* port: which table its records go to */
    menu_audio_close();  /* the drive opens the device itself (drive_audio_open); its pause menu uses the menus' layer */
    track_viewer_drive_menus(&menus);
    if (menu_words.link_game != 0) link_race_begin((const char *)config_current()->image, cg_chat);
    if (!track_viewer_drive(resource_root(), name, race_head_to_head_model(cg_g.player_car), 1, cg_g.race_type, 0, NULL))
        port_log("control_central: the race on %s could not start\n", name);
    link_race_end();
    menu_audio_open();
    cg_last_drive = track_viewer_drive_results();
    const int16_t raced = cg_last_drive.over ? cg_last_drive.end_request : 0;
    memcpy(cg_game_state + 0x5ba, &raced, sizeof raced);
}
void cg_segment_results(void *ctx, int32_t mode) { (void)ctx; results(results_screen, &cg_last_drive, mode); }
void cg_track_results(void *ctx, int32_t ask) { (void)ctx; results(track_results_screen, &cg_last_drive, ask); }
void cg_circuit_results(void *ctx, int32_t ask) { (void)ctx; results(circuit_results_screen, &cg_last_drive, ask); }

void cg_standings(void *ctx, int32_t segment)
{
    (void)ctx;
    const int32_t screen = ea_screen();
    StandingsInputs in = { cg_game_state, finish, cg_car_points, cg_image_short(0x95), cg_image_short(0x93),
                                 cg_image_short(0xb1), resource_german(), cg_g.unlock_level, 0, screen,
                                 ea_picture_shape(screen), records_glue_cheats(ROUTE_SEGMENT, config_current()->image),
                                 records_glue_line(ROUTE_SEGMENT, config_current()->image) };
    StandingsState st = { cg_g.inverse, cg_screen_palette };
    int x, y;
    pointer_position(&x, &y);
    DialogPointer p = { pointer_active(), 0, x, y, x, y };
    const StandingsOps ops = {
        cg_flush, move_to, load_art, no_inverse, music_feed, shape, set_palette, set_palette, nothing, styles,
        cg_poll_pointer, hide, nothing, in_rect, select_picture, select_default, blit, blit_masked, blit_word, style,
        glue_boxed, time_text, results_order, nothing, nothing, flip, show_dialog, cg_mark_at, buttons, fade_in,
        fade_out_bank, deadline_set, sound, ticks, activate_dialog, cg_key, click, off_dialog, music, cg_deadline_passed,
        free_bank,
        free_handle_inverse, &p };
    cg_stage_in = &in.unknown_50a5b6;
    standings_screen(segment, &in, &st, &p, &ops);
    cg_stage_in = NULL;
}
