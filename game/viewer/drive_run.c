/* --drive's loop: race_run's calls on the sim (00455a5c), the window, TNFS_RUN's headless run, the screenshots and a link
 * race's messages. */
#include "game/sys/port_log.h"
#include "viewer/drive_autopilot.h"
#include "viewer/track_viewer.h"
#include "viewer/drive_session.h"
#include "viewer/drive_light.h"
#include "game/gfx/blit.h"
#include "game/sys/fixed_math.h"
#include "game/race/ai_lanes.h"
#include "game/race/car_draw_setup.h"
#include "game/race/race_camera.h"
#include "game/race/race_chase.h"
#include "game/race/race_replay.h"
#include "game/race/start_keys.h"
#include "viewer/drive_audio.h"
#include "viewer/drive_clock.h"
#include "viewer/drive_interp.h"
#include "viewer/race_stats.h"
#include "viewer/light_edit.h"
#include "viewer/spray_v4_draw.h"
#include "game/sys/port_extras.h"
#include "game/sys/port_profile.h"

#include "game/race/race_follow.h"
#include "game/sys/port_controls.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static RaceRunWords *drive_link_run;  /* the running race's words (004c5598) the inputs' messages write */
static int light_edit_started;         /* a headless run's screenshot starts the editor itself (write_drive_ppm) */
static LightEdit light_edit;          /* port (light_edit): the live light editor (light_edit.h) */
static RaceStats race_stats;          /* port: F12's overlay, its figures and its switch (race_stats.h) */

enum { KEY_F1 = 0x3b, KEY_F2 = 0x3c, KEY_F3 = 0x3d, KEY_F9 = 0x43 };

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

/* the race's steps a second (drive_clock.h: 3 base ticks of 90 Hz). A refresh of 30 is those steps drawn once each, no frame between them: a real cap of 30 fps. */
enum { DRIVE_STEP_HZ = 30 };

void drive_run_refresh_set(int32_t hz)
{
    drive_interp.refresh_hz = hz;
}

/* One step; at the race's end the results. */
/* port, for the checks: TNFS_TRACE_PLAYER=N prints slot 0's car every N race ticks (a tick once in a row; a
 * replay's jumps and seeks print a tick again when they come back to it), so a race and its replay can be compared */
static void trace_player(void)
{
    static int every = -1;
    static int32_t last = -1;
    if (every < 0) {
        const char *v = getenv("TNFS_TRACE_PLAYER");
        every = v != NULL && atoi(v) > 0 ? atoi(v) : 0;
    }
    const RaceState *race = &drive_sim.race;
    const Car *car = race->cars[0];
    if (every == 0 || car == NULL || race->tick % every != 0 || race->tick == last) return;
    last = race->tick;
    printf("trace %d %d: node %d x %d y %d z %d yaw %d speed %d\n", race->replay, race->tick, car->node, car->x, car->y,
           car->z, car->yaw, car->speed);
}

static int32_t drive_steps;  /* port: the sim steps run so far (drive_tick's calls), for the stats overlay's steps a second */

static void drive_tick(uint32_t input)
{
    start_clock_fill(&drive_sim.race.start_clock, drive_input.ticks);  /* 004bb8e0, the timer: every race, cheat or not */
    race_sim_tick(&drive_sim, input);
    drive_steps++;
    trace_player();
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
    if (drive_interp.on && drive_interp.refresh_hz > DRIVE_STEP_HZ && drive_interp.cur.valid && r->w->running && (int32_t)(now - drive_interp.next_frame_ms) >= 0)
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
/* port (dev, env only): TNFS_START_NODE=N starts the player car on the road at node N (the road's centre, heading along it, standing), to
 * capture a place the grid is far from: a tunnel, a bridge. Unset, the race starts as it always does. */
/* a car at a (fractional) node of the road, its heading the road's */
static void place_car(Car *car, RaceState *race, double where)
{
    const int32_t node = (int32_t)((uint32_t)where % race->tri->ring_nodes);
    car->node = node;
    car->progress = node;
    const char *const side = getenv("TNFS_START_SIDE");   /* port (dev, env only): TNFS_START_SIDE=M puts the car M metres (negative: left) across the road from its centre line, for the captures at the road's edge */
    car->ai_target_lateral = side != NULL ? (int32_t)(strtod(side, NULL) * 65536.0) : 0;
    car->steer = 0;
    ai_snap_to_road(car, race);
    car->yaw = (int32_t)((uint32_t)fx_atan2(car->matrix[6], car->matrix[8]) << 8);   /* as car_prepare takes the heading from the matrix */
    TriNode a, b;
    const double fraction = where - floor(where);
    if (fraction > 0 && tri_node(race->tri, (uint32_t)node, &a) == TRI_OK && tri_node(race->tri, ((uint32_t)node + 1) % race->tri->ring_nodes, &b) == TRI_OK) {
        car->x += (int32_t)(fraction * ((double)b.position.x - a.position.x));
        car->y += (int32_t)(fraction * ((double)b.position.y - a.position.y));
        car->z += (int32_t)(fraction * ((double)b.position.z - a.position.z));
    }
}

/* port (dev, env only): TNFS_POLICE_NEAR=N, in a --drive of race type 1 (the open road with its police car, police mode on): the police car (cars[2]) is put N nodes
 * behind the player (6 for "1"; a negative N: that many ahead) and its chase started the exe's own way (ai_police.c: flags_175 bit 2 and the race's chase word 004c5cd8), so the chase view shows
 * it behind and the look back, its lights going */
static void police_near(RaceState *race, double player_node)
{
    const char *v = getenv("TNFS_POLICE_NEAR");
    Car *const cop = race->cars[2];
    if (v == NULL || cop == NULL || race->unknown_4fbab8 == 0) return;
    const int behind = atoi(v) == 1 || atoi(v) == 0 ? 6 : atoi(v);   /* a negative N puts it ahead, for a picture from the chase view */
    cop->zone = race->cars[0]->zone;
    ai_respawn(cop, race, (int32_t)player_node - behind, 1, 0, 0);   /* the exe's own placement of the police car (ai_manager.c place_police_car) */
    cop->flags_51a |= 4;      /* active */
    cop->flags_175 |= 4;      /* chasing */
    cop->flags_175 &= (uint8_t)~0x20;   /* not given up */
    cop->flags_176[0] &= (uint8_t)~8;   /* not lost */
    race->unknown_4c5cd8 = 1;
    port_log("TNFS_POLICE_NEAR: the police car at node %d, %d behind the player, chasing\n", cop->node, behind);
}

/* port (dev, env only): TNFS_POLICE_NEAR's chase is started once, and the police AI drops it within a few ticks (flags_175 bit 2), which stops its lights; kept each tick the lights keep flashing for the captures */
static void police_keep(RaceState *race)
{
    Car *const cop = race->cars[2];
    if (getenv("TNFS_POLICE_NEAR") == NULL || cop == NULL || race->unknown_4fbab8 == 0) return;
    cop->flags_175 |= 4;
}

/* port (dev, env only): TNFS_RIVAL_BEHIND=N, in a --drive with a rival (--racers 2 or more, or TNFS_OPPONENT): the first rival (cars[1]) is put N nodes behind the player on the road,
 * active, so its headlights light the player's car (the headlight cone's captures). Negative N: that many ahead. Unset, or no rival, nothing changes. */
static void rival_behind(RaceState *race, double player_node)
{
    const char *v = getenv("TNFS_RIVAL_BEHIND");
    Car *const rival = race->cars[1];
    if (v == NULL || rival == NULL || race->car_count < 2) return;
    ai_respawn(rival, race, (int32_t)player_node - atoi(v), 1, 0, 1);
    rival->flags_51a |= 4;   /* active */
    port_log("TNFS_RIVAL_BEHIND: the rival at node %d, %d behind the player\n", rival->node, atoi(v));
}

static void start_at_node(RaceState *race)
{
    const char *at = getenv("TNFS_START_NODE");
    Car *car = race->cars[0];
    if (car == NULL || race->tri == NULL || race->tri->ring_nodes == 0) return;
    if (at == NULL && getenv("TNFS_POLICE_NEAR") != NULL) {   /* the police next to where the player already is */
        police_near(race, (double)car->node);
        return;
    }
    if (at == NULL) {
        rival_behind(race, (double)car->node);
        return;
    }
    const double where = strtod(at, NULL);   /* a fraction goes on along the road: a car between two nodes */
    place_car(car, race, where);
    const char *const turn = getenv("TNFS_START_TURN");
    if (turn != NULL && turn[0] == '1') {   /* port (dev, env only): TNFS_START_TURN=1 turns the car round (half a turn about the vertical), for the headlight cone's U-turn captures */
        for (int row = 0; row < 3; row++) car->matrix[3 * row] = -car->matrix[3 * row], car->matrix[3 * row + 2] = -car->matrix[3 * row + 2];
        car->yaw = (int32_t)((uint32_t)car->yaw + 0x800000u) & 0xffffff;
    }
    if (turn != NULL && turn[0] == '2') {   /* port (dev, env only): TNFS_START_TURN=2 turns it a quarter, across the road */
        for (int row = 0; row < 3; row++) {
            const int32_t a = car->matrix[3 * row], c = car->matrix[3 * row + 2];
            car->matrix[3 * row] = c, car->matrix[3 * row + 2] = -a;
        }
        car->yaw = (int32_t)((uint32_t)car->yaw + 0x400000u) & 0xffffff;
    }
    port_log("TNFS_START_NODE: the player car at node %d\n", car->node);
    police_near(race, where);
    rival_behind(race, where);
}

static void run_laps(void *ctx, int32_t nodes, int32_t laps) { (void)ctx, (void)nodes, (void)laps; start_at_node(&drive_sim.race); }  /* race_sim_start_field's laps; the dev start */
static void run_tick(void *ctx) { (void)ctx, race_sim_tick_only(&drive_sim); }

static uint32_t screenshot_input(uint32_t i, uint32_t ticks);

/* A frame number from the environment, -1 when unset */
static int32_t env_frame(const char *name)
{
    const char *v = getenv(name);
    return v != NULL ? (int32_t)strtol(v, NULL, 10) : -1;
}

/* Headless, F3 down on the frames TNFS_F3_AT lists (comma-separated), for the captures of the view distances */
static int f3_env_press(int32_t frame)
{
    const char *v = getenv("TNFS_F3_AT");
    if (v == NULL) return 0;
    for (;;) {
        char *end = NULL;
        const long at = strtol(v, &end, 10);
        if (end == v) return 0;
        if (at == frame) return 1;
        if (*end != ',') return 0;
        v = end + 1;
    }
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

/* Test harness: TNFS_REPLAY_BUTTONS="frame:which,..." presses a replay control on each frame named (drive_replay_press) */
static void env_buttons(int32_t frame)
{
    const char *v = getenv("TNFS_REPLAY_BUTTONS");
    while (v != NULL && *v != 0) {
        char *end = NULL;
        const long at = strtol(v, &end, 10);
        if (end == v || *end != ':') return;
        const long which = strtol(end + 1, &end, 10);
        if (at == frame) drive_replay_press(&drive_replay, (int)which);
        if (*end != ',') return;
        v = end + 1;
    }
}

static void run_frame(void *ctx, int32_t frame)
{
    DriveRun *r = ctx;  /* race_sim_tick runs 0045597c with the race's own tick */
    if (r->headless_ticks > 0) {
        drive_replay_key(&drive_replay, env_press(frame));
        drive_tick(screenshot_input((uint32_t)frame, (uint32_t)r->headless_ticks));
        return;
    }
    env_buttons(frame);
    drive_input_read_keys(&drive_input);
    start_keys_fill_key(drive_sim.race.start_keys, drive_input.keys, drive_sim.race.start_input, drive_sim.race.playback,
                        drive_sim.race.port_nitro_start, drive_input.nitro_key);  /* the exe's keyboard handler's flags, read each step */
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
    race_stats_key_save(&race_stats, drive_input.keys[RACE_STATS_KEY] != 0);  /* port: F12, written to the mode's file on a toggle */
    {   /* port (light_edit): F10 holds the race where it is, F11 copies the node's line, the file is read again when it changes */
        const Car *followed = drive_sim.race.cars[race_slot(drive_sim.race.player)];
        const uint32_t now = platform_ticks_ms();
        light_edit_keys(&light_edit, drive_input.keys, followed != NULL ? (uint32_t)followed->node : 0, now);
        if (light_edit_poll(&light_edit, now)) drive_wet_reload();   /* the places file is watched too: the dry zones follow it */
        static uint32_t held_at;   /* the real time the hold last counted */
        if (light_edit.on && light_edit.frozen) {   /* the clock stands still while the race does */
            if (held_at == 0) held_at = now;
            r->base_ms += now - held_at;
            held_at = now;
            return;
        }
        held_at = 0;
    }
    take_detail(&drive_keys.detail_pending);
    ported_view_set_sky_off(drive_keys.state.w4fb786);
    ported_view_set_tags_hidden(drive_keys.state.hidden);
    drive_replay_key(&drive_replay, drive_input.keys[KEY_F9] != 0);
    drive_bumper_keys(&drive_bumper, drive_input.keys[KEY_F1] != 0 && port_profile_key_free((Profile)port_settings_profile(), "bumper_mirror"),
                      drive_input.keys[KEY_F2] != 0 && port_profile_key_free((Profile)port_settings_profile(), "bumper_radar") && port_host_allows_radar_and_camera());   /* port (modes): only where the mode leaves them free */
    drive_tick(drive_input_tick(&drive_input, &drive_sim.race));
}

/* port: the PS1 light's ambient at the car the dash is of (drive_light.h) */
static void light_dash_node(void)
{
    const Car *car = drive_sim.race.cars[race_slot(drive_sim.race.player)];
    float ratio[3];
    drive_light_dash_ratio(ported_view_car_ratio(car, ratio) ? ratio : NULL);
}

/* port (gauge_light = 5): a palette for each colour the dash paints (drive_wide_lit_colours), `palette` lit through it; how many */
static int lit_colour_palettes(const uint8_t *palette, uint8_t out[PLATFORM_LIT_SLOTS][768])
{
    uint8_t rgb[PLATFORM_LIT_SLOTS][3];
    const int n = drive_wide_lit_colours(rgb, PLATFORM_LIT_SLOTS);
    int made = 0;
    for (int i = 0; i < n; i++)
        if (!drive_light_palette_lit_colour(palette, rgb[i], out[made])) break;
        else made++;
    return made == n ? n : 0;
}

/* port: the PS1 light's time of day tints the 3D's pixels only (drive_light.h), the picture as the window shows it */
static int write_drive_ppm(const char *path, const uint8_t *shown, int width, const uint8_t *palette)
{
    const int height = drive_output_height();
    {   /* port (light_edit): a screenshot of a race with the editor on shows its lines, which checks them headless */
        const Car *followed = drive_sim.race.cars[race_slot(drive_sim.race.player)];
        if (!light_edit_started) light_edit_start(&light_edit, light_edit_wanted()), light_edit_started = 1;
        shown = light_edit_compose(&light_edit, shown, width, height, palette, followed != NULL ? (uint32_t)followed->node : 0);
    }
    light_dash_node();   /* the dash's ratio before the mask is asked for (drive_light_needs_split) */
    const uint8_t *mask = drive_light_world_mask(shown, (size_t)width * (size_t)height);
    if (mask == NULL) return track_viewer_write_ppm(path, shown, width, height, palette);
    uint8_t world[768], half[768], lit[768], colours[PLATFORM_LIT_SLOTS][768];
    light_dash_node();
    drive_light_palette(palette, world);
    drive_light_palette_half(palette, half);
    const int n = lit_colour_palettes(palette, colours);
    return track_viewer_write_ppm_split(path, shown, width, height, palette, world, half, drive_light_palette_lit(palette, lit) ? lit : NULL, mask,
                                        n > 0 ? colours[0] : NULL, n);
}

static void present_drive(const Track *t, uint8_t *pixels)
{
    PlatformColor palette[256], world[256], half[256];
    static PlatformColor lit[256];   /* port (gauge_light): kept for platform_set_lit_palette */
    static PlatformColor lit_colour[PLATFORM_LIT_SLOTS][256];   /* port (gauge_light = 5): one per painted colour, kept for platform_set_lit_colours */
    uint8_t colour_palettes[PLATFORM_LIT_SLOTS][768];
    uint8_t world_palette[768], half_palette[768], lit_palette[768];
    light_dash_node();
    drive_light_palette(t->art.palette, world_palette);   /* port: the PS1 light's time of day, for the 3D's pixels */
    drive_light_palette_half(t->art.palette, half_palette);   /* and half of it for the dash's art */
    for (int i = 0; i < 256; i++) {  /* the police screen writes the palette (00476460) */
        palette[i] = (PlatformColor){ t->art.palette[3 * i], t->art.palette[3 * i + 1], t->art.palette[3 * i + 2] };
        world[i] = (PlatformColor){ world_palette[3 * i], world_palette[3 * i + 1], world_palette[3 * i + 2] };
        half[i] = (PlatformColor){ half_palette[3 * i], half_palette[3 * i + 1], half_palette[3 * i + 2] };
    }
    if (drive_light_palette_lit(t->art.palette, lit_palette)) {
        for (int i = 0; i < 256; i++) lit[i] = (PlatformColor){ lit_palette[3 * i], lit_palette[3 * i + 1], lit_palette[3 * i + 2] };
        platform_set_lit_palette(lit);
    } else platform_set_lit_palette(NULL);
    const int colours = lit_colour_palettes(t->art.palette, colour_palettes);
    for (int k = 0; k < colours; k++)
        for (int i = 0; i < 256; i++)
            lit_colour[k][i] = (PlatformColor){ colour_palettes[k][3 * i], colour_palettes[k][3 * i + 1], colour_palettes[k][3 * i + 2] };
    platform_set_lit_colours(colours > 0 ? (const PlatformColor (*)[256])lit_colour : NULL, colours);
    int width;
    const uint8_t *shown = drive_output(pixels, &width);
    /* port: the stats overlay (race_stats.h) on a copy of the frame, after the frame is composed and before it is shown */
    race_stats_sample(&race_stats, platform_ticks_ms());
    const RaceStatsView view = { .figures = race_stats.meter.figures, .refresh_hz = drive_interp.refresh_hz,
                                 .interp = drive_interp.on, .width = width, .height = drive_output_height(),
                                 .segments = ported_view_draw_segments(), .modern = ported_view_modern(),
                                 .filter = ported_view_filter_on(), .fog = ported_view_fog_on() };
    shown = race_stats_compose(&race_stats, shown, width, drive_output_height(), t->art.palette, &view);
    {   /* port (light_edit) */
        const Car *followed = drive_sim.race.cars[race_slot(drive_sim.race.player)];
        shown = light_edit_compose(&light_edit, shown, width, drive_output_height(), t->art.palette, followed != NULL ? (uint32_t)followed->node : 0);
    }
    const uint8_t *mask = drive_light_world_mask(shown, (size_t)width * (size_t)drive_output_height());   /* the overlay is 2D */
    if (mask != NULL) platform_present_split(shown, palette, world, half, mask);
    else platform_present(shown, palette);
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
    const int scaled = drive_wide.scale > 1;   /* port (render scale): the framebuffer that many times larger for the race */
    if (scaled) {
        const PlatformConfig big = { platform_w * drive_wide.scale, SCREEN_H * drive_wide.scale, config.title, config.scale_integer,
                                     config.fullscreen, config.refresh_hz };
        if (!platform_reconfigure(&big)) {
            port_log("drive: no render scale, the framebuffer keeps its size\n");
            drive_wide_scale_off();
            drive_wide_size(platform_w);
        }
    }
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
    drive_replay.pace = &w.unknown_4c53a0, drive_replay.timer = &w.timer, drive_replay.due = &w.due, drive_replay.done = &w.done;
    DriveRun r = { t, pixels, &w, platform_ticks_ms(), 0, 0, 0, 0, 0, -1, drive_clock_of_level(DRIVE_CLOCK_LEVEL) };
    /* 004275f8: 004fb7d8 bit 3 (the Turbo cheat) puts the timer on level 3 (0043bb44) */
    if (drive_sim.race.track_flags & TRACK_FLAG_TURBO) r.clock = drive_clock_of_level(DRIVE_CLOCK_TURBO_LEVEL);
    drive_clock = r.clock;
    light_edit_start(&light_edit, light_edit_wanted()), light_edit_started = 1;   /* port (light_edit) */
    race_stats_start(&race_stats, port_extras_race_on(RACE_SWITCH_STATS));  /* port: the overlay as the mode's file has it */
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
    platform_text_input(false);   /* port: no accent chooser over the held horn key (platform.h) */
    race_run(&ops, &drive_sim.race, &w);
    platform_text_input(true);
    drive_sim.service = NULL, drive_sim.service_user = NULL, drive_sim.timer = NULL;
    drive_end();
    drive_link_run = NULL;
    drive_results.end_state = drive_sim.race.end_state;  /* also a replay left before its race was over */
    drive_menu.words = NULL, drive_menu.music_start = NULL, drive_menu.music_user = NULL;
    drive_replay.pace = drive_replay.timer = drive_replay.due = drive_replay.done = NULL;
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
    if (drive_wide.scale > 1 && drive_menus_on) {   /* the menus go on at their size */
        const PlatformConfig menus = port_platform_config(NULL);
        if (!platform_reconfigure(&menus)) port_log("drive: the framebuffer keeps the race's size\n");
    }
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
    drive_replay.pace = &w.unknown_4c53a0, drive_replay.timer = &w.timer, drive_replay.due = &w.due, drive_replay.done = &w.done;
    DriveRun r = { t, pixels, &w, 0, 0, (int32_t)ticks, 0, 0, 0, -1, drive_clock_of_level(DRIVE_CLOCK_LEVEL) };
    RaceRunOps ops = *drive_run_ops();
    ops.ctx = &r;
    race_run(&ops, &drive_sim.race, &w);
    if (drive_autopilot_on()) {   /* port: test harness only: the racers' finish ticks, as the window run prints them (track_viewer.c's "finish:") */
        printf("finish:");
        for (int slot = 0; slot < drive_sim.race.racers && slot < RACE_PLAYERS; slot++) printf(" %d=%d", slot, (int)drive_sim.race.players[slot].finish);
        printf("\n");
    }
    drive_replay.pace = drive_replay.timer = drive_replay.due = drive_replay.done = NULL;
    render_drive(t, pixels);
    int width;
    const uint8_t *shown = drive_output(pixels, &width);
    return write_drive_ppm(screenshot, shown, width, t->art.palette);
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

enum { SPIN_TICKS = 150, SPIN_BRAKE_TICKS = 30 };   /* TNFS_SCRIPT=spin: the last 150 ticks, the handbrake the first 30 */

/* The screenshots' input on step i of `ticks`: full throttle, a tap of
 * 0x1000 while the race tick is 320..327, and over the last 20 steps the
 * lock TNFS_STEER=left / right asks for. */
static uint32_t screenshot_input(uint32_t i, uint32_t ticks)
{
    const Car *own = drive_sim.race.cars[drive_sim.race.player];
    if (drive_autopilot_on() && own != NULL)   /* port: test harness only (drive_autopilot.h): the headless run is driven as the window run is */
        return drive_autopilot_word(own, &drive_sim.race, drive_input.own_specs);
    const char *script = getenv("TNFS_SCRIPT");
    if (script != NULL && strcmp(script, "spin") == 0 && i + SPIN_TICKS >= ticks) {   /* full lock left, the handbrake for a second */
        const uint32_t into = i + SPIN_TICKS - ticks;
        return (into < SPIN_BRAKE_TICKS ? 0x8000u : 0u) | 0u;
    }
    if (script != NULL && strcmp(script, "brake") == 0 && i + 40 >= ticks) return 0x20u | (0x3fu << 6);   /* TNFS_SCRIPT=brake: the brake held over the last 40 steps (the lamps' captures) */
    const char *steer_env = getenv("TNFS_STEER");
    const uint32_t lock = steer_env == NULL ? 0x20 : strcmp(steer_env, "left") == 0 ? 0 : 0x3f;
    const int tap = drive_sim.race.tick >= 320 && drive_sim.race.tick < 328;
    return (i + 20 >= ticks ? lock : 0x20) | (tap ? 0x1000u : 0);
}

/* Headless: `ticks` steps in drive at full throttle, then a screenshot (taken at the run's end, where the car has driven to: TNFS_START_NODE is not the frame's node; the eye node of the last frame is on the "tick" line, and of each TNFS_SHOTS shot on its line)
 * (TNFS_VIEW=N: from view kind N; TNFS_STEER=left / right: full lock at the
 * end; TNFS_NOTICE=1 / 2: that police stop notice up for the last 10 ticks,
 * TNFS_NOTICE=1off / 2off: and down again for the last 5; TNFS_FRAMES=N:
 * whole frames over the last N ticks, so what builds up while drawing, the
 * skid marks, does). */
/* TNFS_SHOTS=prefix:every: a picture (prefix_tick.ppm) every `every` ticks of the last frames, with the eye's node and the car's and the camera's yaw
 * (the follow camera's tests). The eye node in the line is the frame's: TNFS_START_NODE only says where the car starts, and the car drives on at full throttle. */
static void spin_shot(const Track *t, uint8_t *pixels, uint32_t i, uint32_t ticks)
{
    const char *spec = getenv("TNFS_SHOTS");
    const char *colon = spec != NULL ? strrchr(spec, ':') : NULL;
    if (colon == NULL || atoi(colon + 1) <= 0 || i % (uint32_t)atoi(colon + 1) != 0) return;
    const Car *car = drive_sim.race.cars[0];
    int32_t off = (int32_t)(((uint32_t)drive_sim.race.chase.angles[1] -
                             (uint32_t)race_node(&drive_sim.race, drive_sim.race.chase.node).orientation * 0x400u) & 0xffffff);
    if (off > 0x800000) off -= 0x1000000;
    printf("shot %u: eye node %d, car yaw %d, camera yaw %d (a turn is 16777216), camera %.0f degrees off the road, speed %.1f\n", i, (int)drive_sim.race.chase.node,
           car->yaw & 0xffffff, drive_sim.race.chase.angles[1] & 0xffffff, off * 360.0 / 16777216.0, car->speed / 65536.0);
    char name[512];
    snprintf(name, sizeof name, "%.*s_%05u.ppm", (int)(colon - spec), spec, i);
    int width;
    const uint8_t *shown = drive_output(pixels, &width);
    write_drive_ppm(name, shown, width, t->art.palette);   /* as the final screenshot and the window: the time of day's tint too */
    (void)ticks;
}

/* TNFS_FRAME_TIMES=1: the whole frames' render times (the follow camera's cost) */
enum { FRAME_TIMES = 4096 };
static double frame_us[FRAME_TIMES];
static int frame_count;
static double spray_sum[5];   /* the frames' live, puffs, big, strips, texels (spray v4) */
static long spray_max_texels;
static int spray_frames;

static int compare_us(const void *a, const void *b)
{
    const double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

static void frame_times_print(void)
{
    if (getenv("TNFS_FRAME_TIMES") == NULL || frame_count == 0) return;
    double sum = 0;
    for (int i = 0; i < frame_count; i++) sum += frame_us[i];
    qsort(frame_us, (size_t)frame_count, sizeof frame_us[0], compare_us);
    printf("frames %d: mean %.0f us, p50 %.0f us, p95 %.0f us, max %.0f us\n", frame_count, sum / frame_count,
           frame_us[frame_count / 2], frame_us[(int)(frame_count * 0.95)], frame_us[frame_count - 1]);
    if (spray_frames > 0 && spray_sum[0] + spray_sum[1] > 0)   /* spray v4 on: what each frame drew, so a cost regression shows in the log */
        printf("spray v4 per frame (mean of %d): live %.0f, puffs %.1f (big %.1f), strips %.1f, texels %.0f (max %ld)\n", spray_frames,
               spray_sum[0] / spray_frames, spray_sum[1] / spray_frames, spray_sum[2] / spray_frames, spray_sum[3] / spray_frames,
               spray_sum[4] / spray_frames, spray_max_texels);
}

static void spray_stats_add(void)
{
    const SprayV4Stats *s = spray_v4_stats();
    spray_sum[0] += s->live, spray_sum[1] += s->puffs, spray_sum[2] += s->big, spray_sum[3] += s->strips, spray_sum[4] += (double)s->texels;
    if (s->texels > spray_max_texels) spray_max_texels = s->texels;
    spray_frames++;
}

int drive_screenshot(const Track *t, uint8_t *pixels, uint32_t ticks, const char *screenshot)
{
    drive_wide_size(screen_width(drive_screen_format()));
    start_at_node(&drive_sim.race);   /* port (dev, env only): TNFS_START_NODE, as the window run takes it */
    const char *view = getenv("TNFS_VIEW");
    const char *follow = getenv("TNFS_FOLLOW_CAR");   /* port (dev, env only): the chase view follows this slot's car (2: the police car) */
    Car *const followed = follow != NULL && atoi(follow) > 0 && atoi(follow) < RACE_CARS && drive_sim.race.cars[atoi(follow)] != NULL
                              ? drive_sim.race.cars[atoi(follow)] : drive_sim.race.cars[0];
    if (view != NULL) race_chase_follow(&drive_sim.race, &drive_sim.race.chase, followed, atoi(view) % CHASE_KINDS);
    const char *frames_env = getenv("TNFS_FRAMES");
    const uint32_t frames = frames_env == NULL ? 0 : (uint32_t)atoi(frames_env);
    const int f3_keys = getenv("TNFS_F3_AT") != NULL;  /* the captures' F3 presses: the race's keys, as the window run reads them */
    /* port (dev, env only): TNFS_LOOK_BACK_AT=from:to holds the look back key (B, or the player's) over those ticks, for the captures */
    int32_t look_from = -1, look_to = -1;
    const char *look_env = getenv("TNFS_LOOK_BACK_AT");
    if (look_env != NULL) sscanf(look_env, "%d:%d", &look_from, &look_to);
    const PortControls look_port = port_controls_load();
    /* port (dev, env only): TNFS_VIEW_AT=tick:kind takes the chase camera to that kind at that tick, as a view key would (the cameras' change, for the captures) */
    int32_t view_at = -1, view_to = 0;
    const char *view_at_env = getenv("TNFS_VIEW_AT");
    if (view_at_env != NULL) sscanf(view_at_env, "%d:%d", &view_at, &view_to);
    /* port (dev, env only): TNFS_FRAME_SKIP=n draws one tick in n of the last TNFS_FRAMES (the last one always), as a slow machine does: the ticks between have no frame (the trails must lay them, spray and dust) */
    const char *skip_env = getenv("TNFS_FRAME_SKIP");
    const uint32_t frame_skip = skip_env != NULL && atoi(skip_env) > 1 ? (uint32_t)atoi(skip_env) : 1;
    const char *dump_path = getenv("TNFS_AUDIO_DUMP");   /* port (dev, env only): the race's sound mixed offline to this WAV (drive_audio.c) */
    const int dumping = dump_path != NULL && drive_audio_offline_open(&drive_sim.race, drive_cars.data_root, dump_path);
    if (dumping) {
        race_sound_setup(&drive_sim.race, run_music_volume, NULL);
        race_sound_load_banks(&drive_sim.race, drive_audio_load, &drive_banks);
        race_sound_start_voices(&drive_sim.race);
    }
    for (uint32_t i = 0; i < ticks; i++) {
        drive_replay_key(&drive_replay, env_press((int32_t)i));
        if (view_at >= 0 && (int32_t)i == view_at) race_chase_follow(&drive_sim.race, &drive_sim.race.chase, followed, view_to % CHASE_KINDS);
        if (look_from >= 0) {
            const int32_t key = port_controls_look_back_key(&look_port);
            drive_input.keys[key] = (int32_t)i >= look_from && (int32_t)i < look_to;
            drive_sim.race.chase.port_look_back = race_look_back_held(drive_sim.race.port_follow_camera, drive_input.keys, (int32_t)sizeof drive_input.keys, key);
            race_look_back_view(&drive_sim.race);
        }
        if (f3_keys) {
            drive_input.keys[KEY_F3] = f3_env_press((int32_t)i) ? 1 : 0;
            drive_keys_frame(&drive_keys, &drive_input, &drive_sim.race, &drive_views);
            drive_input.keys[KEY_F3] = 0;
            take_detail(&drive_keys.detail_pending);
        }
        drive_tick(screenshot_input(i, ticks));
        police_keep(&drive_sim.race);
        if (dumping) drive_audio_offline_tick();
        const char *notice = getenv("TNFS_NOTICE");
        if (notice != NULL && (notice[0] == '1' || notice[0] == '2') && i + 10 >= ticks) {
            drive_sim.race.stop_notice[notice[0] - '1'] = !(notice[1] == 'o' && i + 5 >= ticks);
            render_drive(t, pixels);  /* whole frames, so the screen comes and goes as on screen */
            continue;
        }
        if (i + frames >= ticks && (ticks - 1 - i) % frame_skip == 0) {
            struct timespec t0, t1;
            clock_gettime(CLOCK_MONOTONIC, &t0);
            render_drive(t, pixels);
            clock_gettime(CLOCK_MONOTONIC, &t1);
            if (frame_count < FRAME_TIMES) spray_stats_add();
            if (frame_count < FRAME_TIMES) frame_us[frame_count++] = (t1.tv_sec - t0.tv_sec) * 1000000.0 + (t1.tv_nsec - t0.tv_nsec) / 1000.0;
            spin_shot(t, pixels, i, ticks);
            continue;
        }
        drive_hud_strip(&drive_hud, &drive_sim.race);  /* the HUD keeps drawing every frame */
        ported_view_stream(&t->tri, &t->art, chase_camera(&drive_sim.race.chase, t->tri.ring_nodes).node);  /* and 0043f540 */
    }
    if (dumping) drive_audio_offline_close();
    frame_times_print();
    const Car *car = drive_sim.race.cars[0];
    const RaceChase *chase = &drive_sim.race.chase;
    printf("tick %d (phase %d): node %d, speed %.1f m/s, gear %d; view kind %d, eye node %d (%.1f, %.1f, %.1f)\n",
           drive_sim.race.tick, drive_sim.race.phase, car->node, car->speed / 65536.0, car->gear, chase->kind, chase->node,
           chase->position[0] / 65536.0, chase->position[1] / 65536.0, chase->position[2] / 65536.0);
    render_drive(t, pixels);
    if (skip_env != NULL) {   /* port (dev, env only): the trails' continuity, for the frame-skip test */
        int rain_ticks, dust_ticks;
        ported_view_trail_stats(&rain_ticks, &dust_ticks);
        printf("trail ticks: rain %d dust %d\n", rain_ticks, dust_ticks);
    }
    printf("segments %d\n", (int)ported_view_draw_segments());  /* the F12 overlay's "View n segments" (drive_run.c) */
    int width;
    const uint8_t *shown = drive_output(pixels, &width);
    return write_drive_ppm(screenshot, shown, width, t->art.palette);
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
