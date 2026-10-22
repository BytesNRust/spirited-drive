/* --drive's loop: race_run's calls on the sim (00455a5c), the window, TNFS_RUN's headless run, the screenshots and a link
 * race's messages. */
#include "viewer/track_viewer.h"
#include "viewer/drive_session.h"
#include "game/gfx/blit.h"
#include "game/race/car_draw_setup.h"
#include "game/race/race_camera.h"
#include "game/race/race_chase.h"
#include "game/race/race_replay.h"
#include "viewer/drive_audio.h"
#include "viewer/drive_clock.h"
#include "viewer/drive_interp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static RaceRunWords *drive_link_run;  /* the running race's words (004c5598) the inputs' messages write */

enum { KEY_F1 = 0x3b, KEY_F2 = 0x3c, KEY_F9 = 0x43 };

/* 004c5360 asked for by F3 (004381f8) or the menu's Esc (004557f4): the frame's detail step takes it */
static void take_detail(int32_t *pending)
{
    if (*pending != -1) drive_frame.pending_segments = *pending, *pending = -1;
}

/* port: the poses of the last two steps and when the newer was run, for the frames drawn between steps
 * (drive_interp.h); only in a window, never in the headless runs */
static struct {
    DriveInterpPose prev, cur;
    uint32_t cur_ms;
    int on;
    int refresh_hz;             /* the frames a second drawn between steps; 60 until the port's settings give it */
    uint32_t next_frame_ms;
} drive_interp;

/* One step; at the race's end the results. */
static void drive_tick(uint32_t input)
{
    race_sim_tick(&drive_sim, input);
    drive_cars.ticked = 1;
    drive_interp.prev = drive_interp.cur;
    drive_interp_capture(&drive_interp.cur, &drive_sim);
    drive_interp.cur_ms = platform_ticks_ms();
    if (race_sim_over(&drive_sim)) drive_end();
}

/* The window's race: 00455a5c (race_run) on --drive. Its setup and
 * teardown calls are start_drive's / free_drive's work, done there (no-ops
 * here); the start's three ticks run on the sim; the services poll the
 * window and run the timer at 30 a second (60 race ticks, two a frame);
 * a frame is the keys and race_sim_tick; a draw is render_drive and the
 * present. */
typedef struct {
    const Track *t;
    uint8_t *pixels;
    RaceRunWords *w;
    uint32_t base_ms;
    int quit;
    int32_t headless_ticks;     /* TNFS_RUN with --screenshot: a frame due each pass, no window, this many frames */
    int32_t late, unknown_4fb78c, store_clock;  /* 004fb790, 004fb78c, 004fb7a0: the network's, unread here */
    int32_t timer_written;      /* the timer run_service last wrote (-1 none yet) */
    DriveClock clock;           /* port: the steps by real time on 004fbabc's level (drive_clock.h) */
} DriveRun;

enum { DRIVE_IDLE_MS = 1 };
static DriveClock drive_clock = { 3, 3 };  /* port: level 1 until a race starts; the running race's (DriveRun.clock), for the frames between its steps */

static void run_none(void *ctx) { (void)ctx; }
static void run_none_i(void *ctx, int32_t a) { (void)ctx, (void)a; }
static void present_blended(DriveRun *r);

static void run_service(void *ctx)
{
    DriveRun *r = ctx;
    if (r->headless_ticks > 0) {
        const int last = r->w->done >= r->headless_ticks;
        if (last) r->w->running = 0;
        r->w->timer = last ? r->w->done : r->w->done + 1;
        return;
    }
    drive_audio_pump();
    if (!platform_poll_events()) r->quit = 1;
    for (PlatformKey key; (key = platform_next_key()) != PLATFORM_KEY_NONE;)  /* Esc is the menu's (004557f4) */
        if (key == PLATFORM_KEY_ESCAPE && drive_pause.menu == NULL) r->quit = 1;
    if (r->quit) r->w->running = 0;
    const uint32_t now = platform_ticks_ms();
    /* the timer written elsewhere (the start zeroes it): count on from its value */
    if (r->w->timer != r->timer_written) r->base_ms = now - drive_clock_ms_of(&r->clock, r->w->timer);
    r->w->timer = drive_clock_steps(&r->clock, now - r->base_ms);
    r->timer_written = r->w->timer;
    if (r->w->timer > r->w->done) return;
    /* port: no step due: a frame between steps when one is due on the refresh rate */
    if (drive_interp.on && drive_interp.cur.valid && r->w->running && (int32_t)(now - drive_interp.next_frame_ms) >= 0)
        present_blended(r);
    else platform_sleep_ms(DRIVE_IDLE_MS);
}

static void run_screen_mode(void *ctx, int32_t w, int32_t h, int32_t bits) { (void)ctx, (void)w, (void)h, (void)bits; }
static int32_t run_net_wait(void *ctx, int32_t kind) { (void)ctx; return drive_link_on ? drive_link.wait(kind) : 1; }
static void run_clip(void *ctx, int32_t l, int32_t r, int32_t t, int32_t b) { (void)ctx, (void)l, (void)r, (void)t, (void)b; }
/* 00474c8c: the recording's buffer is drive_replay_start's (drive_replay.c, made before the race runs), so these
 * calls make none. */
static uint8_t *run_no_recording(void *ctx, int32_t size) { (void)ctx, (void)size; return NULL; }

/* 0044edd8 on the race's words; the ones without a home in the port (0052b8d4..0052b8f0, 005120c4, 005120c8,
 * 00513718, 00513730, 00513888..98, 004c5388, 005137e4) are this call's own. */
static void run_replay_start(void *ctx)
{
    (void)ctx;
    RaceState *race = &drive_sim.race;
    RaceReplayStart w = { 0 };
    w.view_taken = race->view_taken, w.w4fbb0c = race->unknown_4fbb0c, w.playback = race->playback;
    w.mark_countdown = race->replay_mark_countdown, w.unknown_512f78 = race->unknown_512f78;
    w.unknown_512f7c = race->unknown_512f7c, w.unknown_513728 = race->unknown_513728;
    w.unknown_51372c = race->unknown_51372c, w.unknown_51371c = race->unknown_51371c;
    w.note_second = race->note_second, w.notes = race->notes, w.camera_roll_add = race->camera_roll_add;
    race_replay_start(&w, run_no_recording, NULL);
    race->view_taken = w.view_taken, race->replay_mark_countdown = w.mark_countdown;
    race->unknown_512f78 = w.unknown_512f78, race->unknown_512f7c = w.unknown_512f7c;
    race->unknown_513728 = w.unknown_513728, race->unknown_51372c = w.unknown_51372c;
    race->unknown_51371c = w.unknown_51371c, race->note_second = w.note_second;
    race->camera_roll_add = w.camera_roll_add;
}

/* 00457594: the distance steps and the buffer have no reader in the port yet (its car drawing keeps its own);
 * the skid marks are ported_view's. */
static void run_car_draw_setup(void *ctx)
{
    (void)ctx;
    static CarDrawSetup words;
    car_draw_setup(&words, ported_view_skids(), run_no_recording, NULL);
}

/* 00464604: the music's volume ramped to */
static void run_music_volume(void *user, int16_t volume) { (void)user, drive_audio_music_volume(volume); }

static void run_last_grey(void *ctx) { (void)ctx, race_palette_last_grey(drive_hud_palette_write, &drive_hud); }  /* 00446918 */

/* 00456244 on the music setting (00524e63: the CONFIG default 1, a track of the twelve drawn) at the music volume
 * (00524e65: 100); 00464428 plays the track */
enum { DRIVE_MUSIC_SETTING = 1, DRIVE_MUSIC_VOLUME = 100, DRIVE_MUSIC_TRACKS = 12 };
static const char *const drive_music_names[DRIVE_MUSIC_TRACKS] = {  /* 004c55ac */
    "rock1.as4", "rock2.as4", "rock3.as4", "rock4.as4", "rock5.as4", "rock6.as4",
    "techno1.as4", "techno2.as4", "techno3.as4", "techno4.as4", "techno5.as4", "techno6.as4"
};
static int32_t run_music_rand(void *ctx, int32_t n) { (void)ctx; return race_rand_below(&drive_sim.race, n); }
static void run_music_play(void *ctx, int32_t track)
{
    (void)ctx;
    if (track >= 0 && track < DRIVE_MUSIC_TRACKS) drive_audio_music_play(drive_music_names[track]);
}
static void run_music_fill(void *ctx) { (void)ctx, drive_audio_music_fill(); }  /* 004640d0 */
static void run_music_start(void *ctx)
{
    const DriveRun *r = ctx;
    drive_audio_music_set(r->w->music_on, drive_menus_on ? (int16_t)drive_menus.music_volume : DRIVE_MUSIC_VOLUME, r->w->music_started);
    const RaceMusicOps ops = { ctx, run_music_rand, run_music_play };
    race_music_start(&ops, r->w->music_on);
}

static void run_sound_setup(void *ctx) { (void)ctx, race_sound_setup(&drive_sim.race, run_music_volume, NULL); }  /* 00443b64 */

static void run_draw_setup(void *ctx) { (void)ctx, drive_frame_setup(&drive_sim.race); }  /* 0044d2c4 */

static void run_queue_reset(void *ctx, InputQueue *q)
{
    DriveRun *r = ctx;
    input_queue_reset(q, &r->late, &r->unknown_4fb78c);
}

/* A queue's clock as a frame count: the original polls on every second 004bb8d8 (0043b970), its clock (2 a poll)
 * keeping pace with 004c5590; the drive polls each frame, its clock twice the frames run */
static int32_t clock_frames(int32_t clock) { return clock / 2; }

static void run_queue_prime(void *ctx, InputQueue *q)
{
    DriveRun *r = ctx;
    input_queue_prime(q, drive_sim.race.unknown_4fb7d4, r->w->wait_ticks, &r->store_clock, &r->w->replay_due);
    if (drive_link_on) r->w->replay_due = clock_frames(r->w->replay_due);
}
/* 00410250: a local game has no links and sends nothing, a link race through the link; 004fbb0c taken as 1 in a
 * local game, as drive_input does */
static void run_send_none(void *ctx, int32_t to, const int16_t *words, int32_t size)
{
    (void)ctx;
    if (drive_link_on) drive_link.send(to, words, size);
}
static int32_t run_linked(void *ctx, int32_t player) { (void)ctx, (void)player; return 1; }
/* 0041019c for the link's handlers: the player stood in, sent round */
void drive_link_stand_in(int16_t player)
{
    const RaceState *race = &drive_sim.race;
    const InputStandInWords w = { (int16_t)race->race_type, race->replay, race->playback, race->player, drive_link.host };
    const InputSendOps send = { NULL, run_send_none };
    if (player >= 0 && player < RACE_PLAYERS) input_stand_in(player, drive_input.stand_in_block, drive_sim.race.players, &w, &send);
}

static void run_stand_in_start(void *ctx)
{
    const InputStandInStartWords w = { drive_link_on ? drive_link.host : 1, drive_sim.race.playback, drive_sim.race.racers,
                                       drive_sim.race.humans };
    const InputStandInStartOps ops = { ctx, run_send_none, run_linked };
    input_stand_in_start(drive_input.stand_in_block, drive_sim.race.players, &w, &ops);
}
/* 00443be0: --drive loads no sound banks (audio output waits, option B), so only the channels stop */
static RaceSoundBanks drive_banks;

static void run_sound_end(void *ctx) { (void)ctx, race_sound_end(&drive_sim.race, &drive_banks, drive_audio_bank_ops()); }
static void run_sound_banks(void *ctx) { (void)ctx, race_sound_load_banks(&drive_sim.race, drive_audio_load, &drive_banks); }  /* 00443924 */
static void run_start_voices(void *ctx) { (void)ctx, race_sound_start_voices(&drive_sim.race); }  /* 00443820 */
static void run_engines_start(void *ctx) { (void)ctx, race_sound_start_engine(&drive_sim.race); }  /* 00444394 */
/* 0043cbe0 / 0043cbf4 / 0043cbcc: the controls' forces (no force hooks in --drive) */
static void run_forces_on(void *ctx) { (void)ctx, race_forces_enable(&drive_sim.race); }
static void run_forces_clear(void *ctx) { (void)ctx, race_forces_clear(&drive_sim.race); }
static void run_forces_off(void *ctx) { (void)ctx, race_forces_disable(&drive_sim.race); }
static void run_laps(void *ctx, int32_t nodes, int32_t laps) { (void)ctx, (void)nodes, (void)laps; }  /* race_sim_start_field's */
static void run_tick(void *ctx) { (void)ctx, race_sim_tick_only(&drive_sim); }

static uint32_t screenshot_input(uint32_t i, uint32_t ticks);

/* A frame number from the environment, -1 when unset */
static int32_t env_frame(const char *name)
{
    const char *v = getenv(name);
    return v != NULL ? (int32_t)strtol(v, NULL, 10) : -1;
}

/* Headless, F9 down on the frames TNFS_REPLAY_AT and TNFS_REPLAY_BACK name,
 * once each (race_run's frame count stands while a replay seeks) */
static int env_press(int32_t frame)
{
    static int fired[2];
    static const char *const NAMES[2] = { "TNFS_REPLAY_AT", "TNFS_REPLAY_BACK" };
    for (int k = 0; k < 2; k++)
        if (!fired[k] && env_frame(NAMES[k]) == frame) return fired[k] = 1;
    return 0;
}

static void run_frame(void *ctx, int32_t frame)
{
    DriveRun *r = ctx;  /* race_sim_tick runs 0045597c with the race's own tick */
    if (r->headless_ticks > 0) {
        drive_replay_key(&drive_replay, env_press(frame));
        drive_tick(screenshot_input((uint32_t)frame, (uint32_t)r->headless_ticks));
        return;
    }
    drive_input_read_keys(&drive_input);
    drive_audio_music_pump();  /* 0045597c: 00464240 with 00524e63 set, before the schedule */
    drive_pause.detail = drive_keys.state.detail;
    drive_pause_frame(&drive_pause, &drive_input, &drive_sim.race, &drive_hud, (int16_t)drive_sim.race.tick);
    take_detail(&drive_pause.detail_pending);
    if (drive_pause.paused_ms != 0) {  /* the timer stood still */
        r->base_ms += drive_pause.paused_ms;
        drive_pause.paused_ms = 0;
    }
    if (drive_pause.quit) r->quit = 1, r->w->running = 0;
    drive_keys_frame(&drive_keys, &drive_input, &drive_sim.race, &drive_views);
    take_detail(&drive_keys.detail_pending);
    ported_view_set_sky_off(drive_keys.state.w4fb786);
    ported_view_set_tags_hidden(drive_keys.state.hidden);
    drive_replay_key(&drive_replay, drive_input.keys[KEY_F9] != 0);
    drive_bumper_keys(&drive_bumper, drive_input.keys[KEY_F1] != 0, drive_input.keys[KEY_F2] != 0);
    drive_tick(drive_input_tick(&drive_input, &drive_sim.race));
}

static void present_drive(const Track *t, uint8_t *pixels)
{
    PlatformColor palette[256];
    for (int i = 0; i < 256; i++)  /* the police screen writes the palette (00476460) */
        palette[i] = (PlatformColor){ t->art.palette[3 * i], t->art.palette[3 * i + 1], t->art.palette[3 * i + 2] };
    int width;
    platform_present(drive_output(pixels, &width), palette);
}

/* port: render_drive with the cars and the eye between the last two steps, by the time since the newer one; the
 * newer step as it is at a jump, before the race runs (0044cc74's 00513e60) and on its first frame */
static void render_blended(const Track *t, uint8_t *pixels)
{
    if (!drive_interp.on || drive_frame.unknown_513e60 != 0 || drive_frame.restart != 0
        || drive_interp_snap(&drive_interp.prev, &drive_interp.cur)) {
        render_drive(t, pixels);
        return;
    }
    const uint32_t elapsed = platform_ticks_ms() - drive_interp.cur_ms;
    const int32_t alpha = drive_interp_alpha((uint64_t)elapsed * DRIVE_CLOCK_BASE_HZ * 3,
                                             1000u * (uint32_t)drive_clock_round_ticks(&drive_clock));
    DriveInterpPose saved;
    drive_interp_apply(&drive_sim, &drive_interp.prev, &drive_interp.cur, alpha, &saved);
    render_drive(t, pixels);
    drive_interp_restore(&drive_sim, &saved);
}

static void present_blended(DriveRun *r)
{
    render_blended(r->t, r->pixels);
    present_drive(r->t, r->pixels);
    drive_interp.next_frame_ms = platform_ticks_ms() + 1000u / (uint32_t)drive_interp.refresh_hz;
}

static void run_draw(void *ctx)
{
    DriveRun *r = ctx;
    if (r->headless_ticks != 0) {
        render_drive(r->t, r->pixels);
        return;
    }
    present_blended(r);
}

/* 0041ec0c (004b95b4 0, a link race's start): the screen filled with 00473140(0xff000000) on the race's palette,
 * the sync shape (00513970) at its own place, shown */
static void run_wait_screen(void *ctx)
{
    DriveRun *r = ctx;
    const RacePalette *p = drive_hud.palette;
    const PaletteMatch m = { p != NULL ? p->inverse : NULL, p != NULL ? p->screen : NULL, p != NULL ? p->enabled : NULL };
    memset(r->pixels, palette_match_rgb(&m, 0xff000000u), SCREEN_W * SCREEN_H);
    const QuadTarget screen = { r->pixels, SCREEN_W, 0, 0, SCREEN_W, SCREEN_H };
    if (drive_hud.art.sync != NULL) blit_shape_placed(&screen, drive_hud.art.sync);
    drive_wide_centre(r->pixels);
    if (r->headless_ticks == 0) present_drive(r->t, r->pixels);
}

static const RaceRunOps *drive_run_ops(void);

int drive_window(const Track *t, uint8_t *pixels)
{
    const PlatformConfig config = port_platform_config("TNFS drive");
    if (!drive_menus_on && !platform_init(&config)) return 0;
    int platform_w = SCREEN_W, platform_h = SCREEN_H;
    platform_framebuffer_size(&platform_w, &platform_h);
    drive_wide_size(platform_w);  /* port: the race as wide as the platform the menus (or this window) opened */
    if (drive_audio_open(&drive_sim.race, drive_cars.data_root) && drive_menus_on) {  /* 004275f8's words for the followed slot */
        RaceEntry *followed = &drive_sim.race.entries[race_slot(drive_sim.race.player)];
        followed->sound_volume = drive_menus.sound_volume, followed->music_volume = drive_menus.music_volume;
        followed->sound_mode = drive_menus.sound_mode;
    }
    drive_menu = (DriveMenu){ &drive_sim.race, &drive_replay, &drive_hud, &drive_pause.keys, NULL, NULL, NULL, 0, 0,
                              drive_link_on ? &drive_link : NULL, 1 };
    if (drive_menu_open(&drive_menu, drive_cars.data_root))
        drive_pause.menu = drive_menu_run, drive_pause.menu_user = &drive_menu;
    RaceRunWords w = { 0 };
    w.queues = drive_input.queues;
    w.wait_ticks = drive_link_on ? link_wait_ticks : 0;
    w.music_on = drive_menus_on ? (int16_t)drive_menus.music : DRIVE_MUSIC_SETTING;
    w.unknown_4c53a0 = 0;
    drive_replay.pace = &w.unknown_4c53a0, drive_replay.timer = &w.timer, drive_replay.due = &w.due;
    DriveRun r = { t, pixels, &w, platform_ticks_ms(), 0, 0, 0, 0, 0, -1, drive_clock_of_level(DRIVE_CLOCK_LEVEL) };
    /* 004275f8: 004fb7d8 bit 3 (the Turbo cheat) puts the timer on level 3 (0043bb44) */
    if (drive_sim.race.track_flags & TRACK_FLAG_TURBO) r.clock = drive_clock_of_level(DRIVE_CLOCK_TURBO_LEVEL);
    drive_clock = r.clock;
    memset(&drive_interp, 0, sizeof drive_interp);
    drive_interp.on = 1;
    const char *hz = getenv("TNFS_REFRESH_HZ");  /* port: port.cfg's rate; the variable overrides it for the checks */
    drive_interp.refresh_hz = hz != NULL && atoi(hz) >= 30 && atoi(hz) <= 240 ? atoi(hz) : port_settings_load().refresh_hz;
    drive_input.store_clock = &r.store_clock, drive_input.late = &r.late, drive_input.timer = &w.timer;
    drive_link_run = &w;
    RaceRunOps ops = *drive_run_ops();
    ops.ctx = &r;
    drive_menu.words = &w, drive_menu.music_start = run_music_start, drive_menu.music_user = &r;
    if (drive_link_on) drive_sim.service = run_service, drive_sim.service_user = &r, drive_sim.timer = &w.timer;
    drive_sim.wait_ticks = w.wait_ticks;
    race_run(&ops, &drive_sim.race, &w);
    drive_sim.service = NULL, drive_sim.service_user = NULL, drive_sim.timer = NULL;
    drive_end();
    drive_link_run = NULL;
    drive_results.end_state = drive_sim.race.end_state;  /* also a replay left before its race was over */
    drive_menu.words = NULL, drive_menu.music_start = NULL, drive_menu.music_user = NULL;
    drive_replay.pace = drive_replay.timer = drive_replay.due = NULL;
    while (!drive_menus_on && !r.quit && platform_poll_events()) {  /* the race over: its last picture until Escape */
        for (PlatformKey key; (key = platform_next_key()) != PLATFORM_KEY_NONE;)
            if (key == PLATFORM_KEY_ESCAPE) r.quit = 1;
        render_drive(t, pixels);
        present_drive(t, pixels);
        drive_audio_pump();
        platform_wait_vblank();
    }
    drive_pause.menu = NULL;
    if (drive_menu.ready) drive_menu_close(&drive_menu);
    drive_audio_close();
    if (!drive_menus_on) platform_shutdown();
    return 1;
}

/* TNFS_RUN=1 with --screenshot: race_run as the window runs it, without a
 * window or a clock, until the race tick reaches `ticks`. */
int drive_run_screenshot(const Track *t, uint8_t *pixels, uint32_t ticks, const char *screenshot)
{
    drive_wide_size(screen_width(drive_screen_format()));
    RaceRunWords w = { 0 };
    w.queues = drive_input.queues;
    w.music_on = DRIVE_MUSIC_SETTING;  /* the track drawn as the window draws it (no sound here) */
    drive_replay.pace = &w.unknown_4c53a0, drive_replay.timer = &w.timer, drive_replay.due = &w.due;
    DriveRun r = { t, pixels, &w, 0, 0, (int32_t)ticks, 0, 0, 0, -1, drive_clock_of_level(DRIVE_CLOCK_LEVEL) };
    RaceRunOps ops = *drive_run_ops();
    ops.ctx = &r;
    race_run(&ops, &drive_sim.race, &w);
    drive_replay.pace = drive_replay.timer = drive_replay.due = NULL;
    render_drive(t, pixels);
    int width;
    const uint8_t *shown = drive_output(pixels, &width);
    return track_viewer_write_ppm(screenshot, shown, width, SCREEN_H, t->art.palette);
}

static const RaceRunOps *drive_run_ops(void)
{
    static const RaceRunOps ops = {
        NULL, run_service, run_screen_mode, run_none_i, run_forces_on, run_forces_clear, run_sound_setup, run_sound_banks, run_start_voices,
        run_engines_start, run_none_i,
        run_none, run_car_draw_setup, run_replay_start, run_none, run_none, run_none, run_none_i, run_net_wait, run_draw_setup, run_none, run_none,
        run_none, run_none_i, run_none, run_none, run_none, run_none, run_none, run_none, run_none, run_none, run_none,
        run_none, run_last_grey, run_none, run_none, run_music_start, run_music_fill, run_none, run_clip, run_queue_reset, run_queue_prime,
        run_none,
        run_none, run_wait_screen, run_laps, run_tick, run_none, run_stand_in_start, run_frame, run_draw, run_forces_off, run_none, run_none,
        run_none, run_none, run_none, run_none, run_none, run_none, run_none, run_none, run_none, run_sound_end };
    return &ops;
}

/* The screenshots' input on step i of `ticks`: full throttle, a tap of
 * 0x1000 while the race tick is 320..327, and over the last 20 steps the
 * lock TNFS_STEER=left / right asks for. */
static uint32_t screenshot_input(uint32_t i, uint32_t ticks)
{
    const char *steer_env = getenv("TNFS_STEER");
    const uint32_t lock = steer_env == NULL ? 0x20 : strcmp(steer_env, "left") == 0 ? 0 : 0x3f;
    const int tap = drive_sim.race.tick >= 320 && drive_sim.race.tick < 328;
    return (i + 20 >= ticks ? lock : 0x20) | (tap ? 0x1000u : 0);
}

/* Headless: `ticks` steps in drive at full throttle, then a screenshot
 * (TNFS_VIEW=N: from view kind N; TNFS_STEER=left / right: full lock at the
 * end; TNFS_NOTICE=1 / 2: that police stop notice up for the last 10 ticks,
 * TNFS_NOTICE=1off / 2off: and down again for the last 5; TNFS_FRAMES=N:
 * whole frames over the last N ticks, so what builds up while drawing, the
 * skid marks, does). */
int drive_screenshot(const Track *t, uint8_t *pixels, uint32_t ticks, const char *screenshot)
{
    drive_wide_size(screen_width(drive_screen_format()));
    const char *view = getenv("TNFS_VIEW");
    if (view != NULL) race_chase_follow(&drive_sim.race, &drive_sim.race.chase, drive_sim.race.cars[0], atoi(view) % CHASE_KINDS);
    const char *frames_env = getenv("TNFS_FRAMES");
    const uint32_t frames = frames_env == NULL ? 0 : (uint32_t)atoi(frames_env);
    for (uint32_t i = 0; i < ticks; i++) {
        drive_replay_key(&drive_replay, env_press((int32_t)i));
        drive_tick(screenshot_input(i, ticks));
        const char *notice = getenv("TNFS_NOTICE");
        if (notice != NULL && (notice[0] == '1' || notice[0] == '2') && i + 10 >= ticks) {
            drive_sim.race.stop_notice[notice[0] - '1'] = !(notice[1] == 'o' && i + 5 >= ticks);
            render_drive(t, pixels);  /* whole frames, so the screen comes and goes as on screen */
            continue;
        }
        if (i + frames >= ticks) {
            render_drive(t, pixels);
            continue;
        }
        drive_hud_strip(&drive_hud, &drive_sim.race);  /* the HUD keeps drawing every frame */
        ported_view_stream(&t->tri, &t->art, chase_camera(&drive_sim.race.chase, t->tri.ring_nodes).node);  /* and 0043f540 */
    }
    const Car *car = drive_sim.race.cars[0];
    const RaceChase *chase = &drive_sim.race.chase;
    printf("tick %d (phase %d): node %d, speed %.1f m/s, gear %d; view kind %d, eye node %d (%.1f, %.1f, %.1f)\n",
           drive_sim.race.tick, drive_sim.race.phase, car->node, car->speed / 65536.0, car->gear, chase->kind, chase->node,
           chase->position[0] / 65536.0, chase->position[1] / 65536.0, chase->position[2] / 65536.0);
    render_drive(t, pixels);
    int width;
    const uint8_t *shown = drive_output(pixels, &width);
    return track_viewer_write_ppm(screenshot, shown, width, SCREEN_H, t->art.palette);
}

/* --- a link race's messages (004bcb98[2], [3]) --- */

static void message_lock(void *ctx, uint32_t lock) { (void)ctx, (void)lock; }
static int16_t message_slot_of(void *ctx, uint32_t from) { (void)ctx; return drive_link.slot_of(from); }
static void message_send(void *ctx, int32_t to, const int16_t *words, int32_t size)
{
    (void)ctx;
    drive_link.send(to, words, size);
}

void track_viewer_drive_message(int16_t kind, uint32_t from, const uint8_t *data, int32_t size)
{
    enum { INPUTS = 2, SCHEDULE = 3, SCHEDULE_SIZE = 8 };
    if (!drive_link_on || drive_link_run == NULL) return;  /* none before the race runs, or after */
    RaceState *race = &drive_sim.race;
    if (kind == INPUTS && size >= (drive_link.host != 0 ? 4 : 2 + 2 * race->humans)) {  /* a word, or every human's */
        static int32_t unknown_4fb78c;
        InputMessageWords m = { 0, { 1, 0, drive_link.host, drive_link.slot, 0, 0 }, *drive_input.late, unknown_4fb78c,
                                drive_link_run->replay_due * 2 /* the clock again */, (int16_t)race->humans };
        static const InputMessageOps ops = { NULL, message_lock, message_lock, message_slot_of };
        input_on_message(&ops, drive_input.queues, &m, from, data);
        *drive_input.late = m.late, unknown_4fb78c = m.w4fb78c, drive_link_run->replay_due = clock_frames(m.w4c5598);
    } else if (kind == SCHEDULE && size >= SCHEDULE_SIZE) {
        uint8_t copy[SCHEDULE_SIZE];  /* 004109e4 writes the frame in place */
        memcpy(copy, data, sizeof copy);
        const RaceScheduleInWords w = { 1, drive_link.host, (int16_t)race->tick };
        const InputSendOps send = { NULL, message_send };
        race_schedule_on_message(&send, drive_pause.sched, &w, copy);
    }
}
