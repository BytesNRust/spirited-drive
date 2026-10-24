/* --drive's replay (drive_replay.h): the ported replay functions on the
 * sim's race, each op the port's function for the call the original makes. */
#include "viewer/drive_replay.h"

#include "viewer/drive_audio.h"
#include "game/race/car_held.h"
#include "game/race/car_scenery.h"
#include "game/race/crash_trail.h"
#include "game/race/race_chase.h"
#include "game/race/race_replay.h"
#include "game/race/race_replay_image.h"
#include "game/race/race_sound.h"
#include "game/race/dash_controls.h"
#include "game/race/replay_controls.h"
#include "game/race/race_view_pick.h"
#include "game/sys/menu_key.h"
#include "game/sys/pointer.h"
#include "game/sys/port_log.h"
#include "game/sys/video.h"
#include "game/race/skid_marks.h"
#include "viewer/ported_view.h"
#include "platform/platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    REQUEST_DIRECTED = 2,   /* 00513738: 0044f188 with 2, the director's cuts */
    REQUEST_RACE_ON = 7,    /* 00513738: 0044f090, the race on from where it stood */
    PHASE_RACING = 1,       /* 004c5390 */
    CAMERA_KEYS_NONE = 0    /* 004fb7b4: the camera keys (004381f8), none here */
};

static RaceState *race_of(void *ctx) { return &((DriveReplay *)ctx)->sim->race; }

static RaceReplayWords words(const RaceState *race)
{
    const RaceReplayWords w = { race->unknown_4fbb0c, race->playback, race->humans, race->clock };
    return w;
}

/* 0046463c / 00464604: the music at once / ramped (as drive_keys' F5) */
static void music_now(void *user, int16_t volume) { (void)user, drive_audio_music_volume_now(volume); }
static void music_ramped(void *user, int16_t volume) { (void)user, drive_audio_music_volume(volume); }

static void mute(void *ctx) { race_sound_mute(race_of(ctx), music_now, NULL); }
static void unmute(void *ctx) { race_sound_unmute(race_of(ctx), music_ramped, NULL); }
static void keep_scenery(void *ctx) { race_scenery_keep(race_of(ctx)); }
static void channels_stop(void *ctx) { race_sound_channels_stop(race_of(ctx)); }
static void trail_clear(void *ctx) { crash_trail_clear(&race_of(ctx)->trail); }
static void notices_off(void *ctx) { race_stop_notices_clear(race_of(ctx)); }
static void marks_back(void *ctx) { (void)ctx, skid_reset(ported_view_skids()); }
static void road_yaw(void *ctx) { race_chase_road_yaw(race_of(ctx), &race_of(ctx)->chase); }
static void cycle_view(void *ctx) { race_chase_cycle(race_of(ctx), &race_of(ctx)->chase); }

static void jump(void *ctx, int32_t clock, int32_t until)
{
    DriveReplay *r = ctx;
    const RaceReplayJumpOps ops = { r, keep_scenery, channels_stop, trail_clear, notices_off, marks_back, road_yaw };
    race_replay_jump(&r->sim->race, &r->dir, &ops, clock, until);
}

static void restore_car(void *ctx, int32_t human, int32_t minute, int32_t index)
{
    DriveReplay *r = ctx;
    race_replay_restore_car(&r->sim->race, human, minute, index, r->recording, REPLAY_RECORDING_SIZE);
}

static void seek(void *ctx, int32_t clock, int32_t mark)
{
    const RaceReplaySeekOps ops = { ctx, restore_car };
    race_replay_seek(race_of(ctx), &ops, clock, mark);
}

static void play(void *ctx)
{
    DriveReplay *r = ctx;
    RaceState *race = &r->sim->race;
    const RaceReplayWords w = words(race);
    race_replay_play(&w, race->inputs, r->recording, REPLAY_RECORDING_SIZE);
    if (race->port_replay_launch == 0 || race->start_input == 0) return;   /* the setup's stamp: an older replay's words are its own */
    for (int32_t i = 0; i < w.humans && i < RACE_PLAYERS; i++) {   /* port: the NITRO start's N, out of the word */
        const int launching = race_replay_launch_split(&race->inputs[i]);
        if (i == race->player) race->start_keys[0] = (uint8_t)launching;
    }
}

static void camera(void *ctx, int32_t word) { race_replay_camera(race_of(ctx), word); }

static void step(void *ctx)
{
    const RaceReplayStepOps ops = { ctx, seek, play, camera, cycle_view };
    race_replay_step(race_of(ctx), &ops);
}

static void next_cut(void *ctx)
{
    DriveReplay *r = ctx;
    const RaceReplayCutOps ops = { r, jump, step };
    race_replay_next_cut(&r->sim->race, &r->dir, &ops);
}

enum { HUD_WHOLE = 2 };  /* 004f9a28 in HudState.unknown_4f9a20 (004f9a20..) */

/* 0041e358: 004f9a28 = 1, the screen drawn whole again (00421160 in the car: the replay's panel drawn over) */
static void hud_redraw(void *ctx)
{
    DriveReplay *r = ctx;
    if (r->hud != NULL) r->hud->unknown_4f9a20[HUD_WHOLE] = 1;
}

static void reselect(void *ctx)
{
    DriveReplay *r = ctx;
    RaceState *race = &r->sim->race;
    if (race->chase.camera != NULL) race_view_reselect(r->views, race->chase.camera, race->window_size);
}

static void frame(void *ctx)
{
    DriveReplay *r = ctx;
    const RaceReplayFrameOps ops = { r, step, next_cut, jump, hud_redraw, reselect };
    race_replay_frame(&r->sim->race, &r->dir, &ops);
}

/* 004397f4: --drive's race_sim_tick has put the frame's control word in place already */
static void input_tick(void *ctx, int32_t clock) { (void)ctx, (void)clock; }

static void record_inputs(void *ctx)
{
    DriveReplay *r = ctx;
    const RaceState *race = &r->sim->race;
    const RaceReplayWords w = words(race);
    uint32_t inputs[RACE_PLAYERS];
    memcpy(inputs, race->inputs, sizeof inputs);
    if (race->port_replay_launch != 0 && race->start_input != 0 && race->player >= 0 && race->player < RACE_PLAYERS)
        inputs[race->player] = race_replay_launch_word(inputs[race->player], race->start_keys[0] != 0);   /* port: N */
    race_replay_record(&w, inputs, r->recording, REPLAY_RECORDING_SIZE);
}

static void save_car(void *ctx, int32_t slot, int32_t index)
{
    DriveReplay *r = ctx;
    RaceState *race = &r->sim->race;
    if (slot >= 0 && slot < RACE_CARS && race->cars[slot] != NULL)
        race_replay_save_minute(race, race->cars[slot], index, r->recording, REPLAY_RECORDING_SIZE);
}

static void record(void *ctx)
{
    const RaceReplayTickOps ops = { ctx, input_tick, record_inputs, save_car, seek };
    race_replay_tick(race_of(ctx), &ops);
}

/* 0044d514: the saved replay is read back only on the menus' requests (4, 5, 8) */
/* 0044d514 on the .RPL in memory: the C library's calls read it in order (past its end, zeros) */
static void *file_open(void *ctx, const char *name, const char *mode)
{
    DriveReplay *r = ctx;
    (void)name, (void)mode;
    r->file_at = 0;
    return r->file != NULL ? r : NULL;
}

static void file_read(void *ctx, void *buffer, uint32_t size, uint32_t count, void *file)
{
    DriveReplay *r = ctx;
    const size_t n = (size_t)size * count;
    if (file == NULL || r->file_at > REPLAY_FILE_SIZE || n > REPLAY_FILE_SIZE - r->file_at) {
        memset(buffer, 0, n);
        return;
    }
    memcpy(buffer, r->file + r->file_at, n);
    r->file_at += n;
}

static void file_write(void *ctx, const void *buffer, uint32_t size, uint32_t count, void *file)
{
    (void)ctx, (void)buffer, (void)size, (void)count, (void)file;
}

static int32_t file_close(void *ctx, void *file) { return (void)ctx, (void)file, 0; }

/* the blocks 0044d514 reads, then put where the port keeps them; the setup's entry words it writes are the
 * setup's copy only (the race took its entries at the start) */
static void load(void *ctx)
{
    DriveReplay *r = ctx;
    if (r->file == NULL || r->recording == NULL) return;
    static uint8_t setup[REPLAY_SETUP_SIZE], cuts[REPLAY_CUTS_SIZE], seek[REPLAY_SEEK_SIZE],
        records[REPLAY_RECORDS * REPLAY_RECORD_SIZE];
    RaceState *race = &r->sim->race;
    const RaceEntry *followed = &race->entries[race_slot(race->player)];
    int32_t loaded = 0;
    const RaceReplayLoad in = { "", setup, &race->player, race->unknown_52b30e, (int16_t)followed->music_volume,
                                (int16_t)followed->sound_volume, cuts, r->recording, seek, records, &loaded };
    const RaceReplayFileOps ops = { r, file_open, file_read, file_write, file_close };
    race_replay_load(&in, &ops);
    race_replay_image_get(race, &r->dir, cuts, seek, records);
}

static void begin(void *ctx, int32_t how)
{
    DriveReplay *r = ctx;
    const RaceReplayJumpTo ops = { r, jump };
    race_replay_begin(&r->sim->race, &r->dir, &ops, how);
}

static void to_end(void *ctx)
{
    DriveReplay *r = ctx;
    const RaceReplayToEndOps ops = { r, jump, unmute, road_yaw };
    race_replay_to_end(&r->sim->race, &r->dir, &ops);
}

/* 0044f55c's and 004718c0's calls: the menus' pointer (game/sys/pointer.h), no joystick. 0047723c, the box
 * the pointer is kept to over a panel (replay_panel.h), is not applied: the pointer goes anywhere, the whole
 * screen its range (a box a menu kept it to before, e.g. Load and Save's list's, would hold it off the bar's Done). The buttons are read without 004417f8's drawing (the menus' overlay would
 * show their screen over the race's frame); 00441990 moves the pointer and not the OS's mouse, as the menus'
 * 00464038 does */
static void any_window(void *ctx, int32_t a, int32_t b, int32_t c, int32_t d)
{
    (void)ctx, (void)a, (void)b, (void)c, (void)d;
    pointer_range(0, 0, VIDEO_WIDTH, VIDEO_HEIGHT);
}
static int16_t mouse_buttons(void *ctx) { return (void)ctx, (int16_t)pointer_buttons_unseen(); }
static int32_t in_rect(void *ctx, int32_t x0, int32_t x1, int32_t y0, int32_t y1)
{
    return (void)ctx, pointer_in_rect(x0, x1, y0, y1);
}
static int32_t no_joy(void *ctx, int32_t device, uint32_t *buttons, int32_t axis[JOY_AXES])
{
    return (void)ctx, (void)device, (void)buttons, (void)axis, 0;
}
static int32_t controls_key(void *ctx)
{
    const DriveReplay *r = ctx;
    JoyState j = { 0 };
    return menu_key(r->key_flags, &j, 0, 0, &(const JoyOps){ NULL, no_joy });  /* 00465358 */
}
static void place(void *ctx, int32_t x, int32_t y) { (void)ctx, pointer_move_to(x, y); }

/* the OS's mouse read as the window thread would have (and 00524790 set once it moves, as 00464088 sets it in the
 * menus: a race started without them has it clear) */
static void pointer_now(int *x, int *y)
{
    pointer_sync_from_os();
    pointer_activate_if_moved();
    pointer_position(x, y);
}

static int16_t view_buttons(void *ctx) { return mouse_buttons(ctx); }
static int32_t view_inside(void *ctx, int16_t l, int16_t r, int16_t t, int16_t b) { return in_rect(ctx, l, r, t, b); }

/* 004718c0 */
static int32_t view_mouse(void *ctx)
{
    DriveReplay *r = ctx;
    const RaceState *race = race_of(ctx);
    int x, y;
    pointer_now(&x, &y);
    r->view_mouse.present = *pointer_active_word();
    const ReplayPanelMouseInputs in = { x, y, 1, race->racers, race->humans, race->viewed, race->replay };
    const ReplayPanelMouseOps ops = { r, any_window, view_buttons, view_inside };
    return replay_panel_mouse(&r->panel->panel, r->replay_art->box, &r->view_mouse, &in, &ops);
}

/* 00471a38 on 0052b8d4 / d8 / f4 (RaceViewPick's are the panel's words) */
static int32_t view_pick(void *ctx)
{
    DriveReplay *r = ctx;
    const RaceState *race = race_of(ctx);
    ReplayPanel *p = &r->panel->panel;
    RaceViewPick pick = { p->view, p->cursor, p->redraw, r->view_last_key };
    const RaceViewPickInputs in = { race->racers, race->humans, race->viewed, race->replay, 0, 0 };
    JoyState j = { 0 };
    const int32_t hit = race_view_pick(&pick, r->key_flags, &j, &(const JoyOps){ NULL, no_joy }, &in);
    p->view = pick.view, p->cursor = pick.cursor, p->redraw = pick.redraw, r->view_last_key = pick.last_key;
    return hit;
}

/* 00404530(004f801c, 00509e1c[viewed], 4) */
static void view_follow(void *ctx, int32_t viewed)
{
    RaceState *race = race_of(ctx);
    if (viewed >= 0 && viewed < RACE_CARS) race_chase_follow(race, &race->chase, race->cars[viewed], 4);
}

static void view_engine(void *ctx) { race_sound_start_engine(race_of(ctx)); }  /* 00444394 */

/* 00471460: the view panel's input, on drive_hud's panel words (drive_panel draws it) */
static void panel_draw(void *ctx)
{
    DriveReplay *r = ctx;
    if (r->panel == NULL || r->replay_art == NULL || r->key_flags == NULL) return;
    RaceState *race = race_of(ctx);
    ReplayPanelWorld w = { race->racers, race->humans, race->viewed, race->dropped };
    const ReplayPanelInputOps ops = { r, view_mouse, view_pick, view_follow, view_engine, cycle_view, place };
    replay_panel_input(&r->panel->panel, r->replay_art, &w, &ops);
    race->viewed = w.viewed, race->dropped = w.done;
}

/* 0044f55c on the dash's words and the director's */
static void panel_input(void *ctx)
{
    DriveReplay *r = ctx;
    if (r->key_flags == NULL || r->dash == NULL) return;
    DashArt *d = r->dash;
    int32_t *w = d->unknown_513888;
    int px, py;
    pointer_now(&px, &py);
    ReplayInput in = { .high = 1, .mouse = *pointer_active_word(), .mouse_buttons = r->mouse_buttons,
                       .pointer_x = px, .pointer_y = py, .last_x = r->last_x, .last_y = r->last_y, .pressed = r->dir.unknown_513888, .cursor = w[DASH_CURSOR],
                       .redraw = r->dir.unknown_513898, .kind = r->dir.unknown_513730, .last_key = r->last_key,
                       .escape = r->key_flags[1], .timer = r->dir.timer, .request = r->dir.request, .speed = w[5],
                       .button = r->dir.button, .last_pressed = r->last_pressed, .phase = r->sim->race.phase,
                       .unknown_5138a0 = w[6] };
    for (int i = 0; i <= REPLAY_INPUT_BUTTONS && i < DASH_CONTROLS; i++) {
        const DashRect b = d->control_box[i];
        in.rects[i][0] = b.left, in.rects[i][1] = b.top, in.rects[i][2] = b.right, in.rects[i][3] = b.bottom;
        in.shapes[i] = d->control_lit[i];
    }
    replay_controls_input(&in, &(const ReplayInputOps){ r, any_window, mouse_buttons, in_rect, controls_key, place });
    r->mouse_buttons = in.mouse_buttons, r->last_x = in.last_x, r->last_y = in.last_y;
    r->dir.unknown_513888 = w[DASH_PRESSED] = in.pressed, w[DASH_CURSOR] = in.cursor;
    r->dir.unknown_513898 = w[DASH_REDRAW] = in.redraw;
    r->dir.request = in.request, r->dir.button = in.button, w[5] = in.speed, w[6] = in.unknown_5138a0;
    r->last_key = in.last_key, r->last_pressed = in.last_pressed;
}

static void scrub(void *ctx, int32_t button, int32_t held)
{
    DriveReplay *r = ctx;
    const RaceReplayScrubOps ops = { r, keep_scenery, mute };
    race_replay_scrub(&r->sim->race, &r->dir, &ops, button, held);
}

static void button(void *ctx, int32_t which, int32_t held)
{
    DriveReplay *r = ctx;
    const RaceReplayButtonOps ops = { r, mute, unmute, jump };
    race_replay_button(&r->sim->race, &r->dir, &ops, which, held);
}

static void keys(void *ctx)
{
    DriveReplay *r = ctx;
    const RaceReplayKeysOps ops = { r, load, begin, to_end, panel_draw, panel_input, scrub, button, cycle_view };
    race_replay_keys(&r->sim->race, &r->dir, &ops);
}

/* 00450020, the sim's tick's first call */
static void update(void *user)
{
    DriveReplay *r = user;
    const RaceReplayUpdateOps ops = { r, keys, record, frame, step };
    if (r->timer != NULL) r->dir.timer = *r->timer;
    if (r->due != NULL) r->dir.due = *r->due;
    const int was_racing = r->sim->race.phase == PHASE_RACING;
    race_replay_update(&r->sim->race, &r->dir, &ops, CAMERA_KEYS_NONE);
    if (r->pace != NULL) *r->pace = r->dir.unknown_4c53a0;
    if (r->timer != NULL) *r->timer = r->dir.timer;
    if (r->due != NULL) *r->due = r->dir.due;
    if (r->done != NULL)
        *r->done = race_replay_resumed_done(was_racing, r->sim->race.phase == PHASE_RACING, r->dir.timer, *r->done, r->port_resume);
}

static uint8_t *allocate(void *ctx, int32_t size)
{
    DriveReplay *r = ctx;
    r->recording = calloc(1, (size_t)size);
    return r->recording;
}

int drive_replay_start(DriveReplay *r, RaceSim *sim, RaceViews *views)
{
    *r = (DriveReplay){ .sim = sim, .views = views, .port_resume = 1 };
    RaceState *race = &sim->race;
    race->unknown_4fbb0c = 1;  /* a local game records (drive_keys and drive_input take it as 1 too) */
    RaceReplayStart s = { 0 };
    s.w4fbb0c = race->unknown_4fbb0c;
    s.playback = race->playback;
    s.notes = race->notes;
    race_replay_start(&s, allocate, r);
    if (r->recording == NULL) {
        race->unknown_4fbb0c = 0;
        return 0;
    }
    /* RaceReplayStart's words onto where the other replay functions keep them */
    race->view_taken = s.view_taken;
    race->replay_mark_countdown = s.mark_countdown;
    race->unknown_512f78 = s.unknown_512f78;
    race->unknown_512f7c = s.unknown_512f7c;
    race->unknown_513728 = s.unknown_513728;
    race->unknown_51372c = s.unknown_51372c;
    race->unknown_51371c = s.unknown_51371c;
    race->note_second = s.note_second;
    race->camera_roll_add = s.camera_roll_add;
    r->dir.unknown_513898 = s.unknown_513898;
    r->dir.frames_played = s.unknown_513718;
    r->dir.end_clock = s.unknown_5120c8;
    r->dir.unknown_513894 = s.unknown_513894;
    r->dir.unknown_513730 = s.unknown_513730;
    r->dir.unknown_513888 = s.unknown_513888;
    r->dir.unknown_4c5388 = s.unknown_4c5388;
    r->dir.unknown_5137e4 = s.unknown_5137e4;
    sim->replay = update;
    sim->replay_user = r;
    return 1;
}

void drive_replay_key(DriveReplay *r, int down)
{
    const int pressed = down && !r->key_down;
    r->key_down = down;
    if (!pressed || r->recording == NULL || r->dir.request != 0) return;
    r->dir.request = r->sim->race.phase == PHASE_RACING ? REQUEST_DIRECTED : REQUEST_RACE_ON;
}

/* 0044d420's file: written into memory, then to the player's directory at fclose */
static uint8_t *out;
static size_t out_size;
static char out_name[REPLAY_PATH_SIZE];

static void *out_open(void *ctx, const char *name, const char *mode)
{
    (void)ctx, (void)mode;
    free(out);
    out = malloc(REPLAY_FILE_SIZE), out_size = 0;
    snprintf(out_name, sizeof out_name, "%s", name);
    return out;
}

static void out_read(void *ctx, void *buffer, uint32_t size, uint32_t count, void *file)
{
    (void)ctx, (void)file;
    memset(buffer, 0, (size_t)size * count);
}

static void out_write(void *ctx, const void *buffer, uint32_t size, uint32_t count, void *file)
{
    (void)ctx;
    const size_t n = (size_t)size * count;
    if (file == NULL || out_size > REPLAY_FILE_SIZE || n > REPLAY_FILE_SIZE - out_size) return;
    memcpy(out + out_size, buffer, n);
    out_size += n;
}

static int32_t out_close(void *ctx, void *file)
{
    (void)ctx;
    const int ok = file != NULL && platform_write_user_file(out_name, out, out_size);
    if (file != NULL && !ok) port_log("drive_replay: %s not written\n", out_name);
    free(out);
    out = NULL;
    return ok ? 0 : -1;
}

/* 0044d420 on the race and the director, the replays' directory the player's ("") */
static void save(void *ctx)
{
    DriveReplay *r = ctx;
    static uint8_t setup[REPLAY_SETUP_SIZE], cuts[REPLAY_CUTS_SIZE], seek[REPLAY_SEEK_SIZE],
        records[REPLAY_RECORDS * REPLAY_RECORD_SIZE];
    RaceState *race = &r->sim->race;
    if (r->recording == NULL) return;
    race_replay_image_setup(race, r->seed, setup);
    race_replay_image_put(race, &r->dir, cuts, seek, records);
    const RaceReplaySave in = { race->unknown_4fbb0c, race->playback, "", setup, race->player, cuts, r->recording,
                                seek, records };
    race_replay_save(&in, &(const RaceReplayFileOps){ r, out_open, out_read, out_write, out_close });
}
static void free_recording(void *ctx, uint8_t *block) { (void)ctx, free(block); }

void drive_replay_end(DriveReplay *r)
{
    if (r->sim == NULL) return;
    RaceState *race = &r->sim->race;
    const RaceReplayEndOps ops = { r, save, free_recording };
    race_replay_end(&ops, race->unknown_4fbb0c, race->playback, race->end_state, r->recording);
    if (race->unknown_4fbb0c == 0 || race->playback == 1) free(r->recording);  /* 0044eef8 leaves it then */
    r->recording = NULL;
    r->sim->replay = NULL;
    r->sim->replay_user = NULL;
}
