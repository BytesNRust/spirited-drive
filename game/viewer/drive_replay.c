/* --drive's replay (drive_replay.h): the ported replay functions on the
 * sim's race, each op the port's function for the call the original makes. */
#include "viewer/drive_replay.h"

#include "viewer/drive_audio.h"
#include "game/race/car_held.h"
#include "game/race/car_scenery.h"
#include "game/race/crash_trail.h"
#include "game/race/race_chase.h"
#include "game/race/race_replay.h"
#include "game/race/race_sound.h"
#include "game/race/skid_marks.h"
#include "viewer/ported_view.h"

#include <stdlib.h>

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
    const RaceReplayWords w = words(&r->sim->race);
    race_replay_play(&w, r->sim->race.inputs, r->recording, REPLAY_RECORDING_SIZE);
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
    const RaceReplayWords w = words(&r->sim->race);
    race_replay_record(&w, r->sim->race.inputs, r->recording, REPLAY_RECORDING_SIZE);
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
static void load(void *ctx) { (void)ctx; }

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

/* 00471460 / 0044f55c: the replay panel, the frontend's menu library (no button pressed) */
static void panel_draw(void *ctx) { (void)ctx; }
static void panel_input(void *ctx) { (void)ctx; }

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
    race_replay_update(&r->sim->race, &r->dir, &ops, CAMERA_KEYS_NONE);
    if (r->pace != NULL) *r->pace = r->dir.unknown_4c53a0;
}

static uint8_t *allocate(void *ctx, int32_t size)
{
    DriveReplay *r = ctx;
    r->recording = calloc(1, (size_t)size);
    return r->recording;
}

int drive_replay_start(DriveReplay *r, RaceSim *sim, RaceViews *views)
{
    *r = (DriveReplay){ .sim = sim, .views = views };
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

/* 0044eef8's save (0044d420) left out: no menu reads the file back */
static void save_none(void *ctx) { (void)ctx; }
static void free_recording(void *ctx, uint8_t *block) { (void)ctx, free(block); }

void drive_replay_end(DriveReplay *r)
{
    if (r->sim == NULL) return;
    RaceState *race = &r->sim->race;
    const RaceReplayEndOps ops = { r, save_none, free_recording };
    race_replay_end(&ops, race->unknown_4fbb0c, race->playback, race->end_state, r->recording);
    if (race->unknown_4fbb0c == 0 || race->playback == 1) free(r->recording);  /* 0044eef8 leaves it then */
    r->recording = NULL;
    r->sim->replay = NULL;
    r->sim->replay_user = NULL;
}
