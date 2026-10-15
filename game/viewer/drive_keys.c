/* --drive's key commands (drive_keys.h). */
#include "viewer/drive_keys.h"

#include "viewer/drive_audio.h"
#include "game/race/input_queue.h"
#include "game/race/race_chase.h"
#include "game/race/race_detail.h"
#include "game/race/race_loop.h"
#include "game/race/race_sound.h"

#include <string.h>

enum {
    FOLLOW_KIND = 4,          /* 004381f8's Alt + Ctrl + n: 00404530(004f801c, car n, 4) */
    HIGH_RESOLUTION = 1,      /* 004fa0c0: 640 x 480 */
    W513730_UNKNOWN = 0,      /* 00513730: not ported; anything but 5 lets the hold key act */
    TIMER_8D8_UNKNOWN = 0     /* 004bb8d8: not ported; 0 keeps the reset (Shift + keypad 1) off */
};

typedef struct {
    DriveKeys *keys;
    DriveInput *in;
    RaceState *race;
    RaceViews *views;
} KeysCtx;

static void follow(void *ctx, int32_t slot)
{
    KeysCtx *c = ctx;
    if (slot >= 0 && slot < RACE_CARS)
        race_chase_follow(c->race, &c->race->chase, c->race->cars[slot], FOLLOW_KIND);
}

static void viewed_changed(void *ctx)
{
    race_sound_start_engine(((KeysCtx *)ctx)->race);
}

static void follow_own(void *ctx)
{
    KeysCtx *c = ctx;
    race_chase_cycle(c->race, &c->race->chase);
}

static void own_changed(void *ctx)
{
    race_loop_clear_51371c(&((KeysCtx *)ctx)->keys->w51371c);
}

static void no_send(void *ctx, int32_t to, const int16_t *words, int32_t size)
{
    (void)ctx, (void)to, (void)words, (void)size;
}

static void reset(void *ctx, int32_t player)
{
    KeysCtx *c = ctx;
    const InputSendOps send = { NULL, no_send };
    /* 004fbb0c taken as 1 in a local game, as drive_input does */
    const InputStandInWords w = { (int16_t)c->race->race_type, c->race->replay, c->race->playback, c->race->player, 1 };
    if (player >= 0 && player < RACE_PLAYERS) input_stand_in(player, c->in->stand_in_block, c->race->players, &w, &send);
}

static void f1(void *ctx)
{
    KeysCtx *c = ctx;
    if (c->race->chase.camera != NULL) race_view_next(c->views, c->race->chase.camera, c->race->window_size);
}

static void screen(void *ctx, int32_t kind)
{
    KeysCtx *c = ctx;
    if (c->race->chase.camera != NULL) race_view_size(c->views, c->race->chase.camera, &c->race->window_size, kind);
}

static void detail(void *ctx, int32_t value)
{
    race_detail_request(value, &((KeysCtx *)ctx)->keys->detail_pending);
}

/* 0046463c / 00464604: the music at once / ramped */
static void music_now(void *user, int16_t volume) { (void)user, drive_audio_music_volume_now(volume); }
static void music_ramped(void *user, int16_t volume) { (void)user, drive_audio_music_volume(volume); }

/* 00443768 / 0044379c: F5, the sound off and on */
static void sound_off(void *ctx) { race_sound_mute(((KeysCtx *)ctx)->race, music_now, NULL); }
static void sound_on(void *ctx) { race_sound_unmute(((KeysCtx *)ctx)->race, music_ramped, NULL); }

void drive_keys_start(DriveKeys *k)
{
    memset(k, 0, sizeof *k);
    k->detail_pending = -1;  /* 004c5360: nothing asked for */
}

void drive_keys_frame(DriveKeys *k, DriveInput *in, RaceState *race, RaceViews *views)
{
    KeysCtx c = { k, in, race, views };
    const RaceKeysOps ops = { &c, follow, viewed_changed, follow_own, own_changed, reset, f1, screen, detail, sound_off,
                              sound_on };
    const RaceKeysInputs inputs = { in->keys, in->codes[INPUT_KEY_HOLD], 0, 0, race->humans, race->phase,
                                    W513730_UNKNOWN, race->race_type, race->replay, race->playback, TIMER_8D8_UNKNOWN, race->player, 0,
                                    HIGH_RESOLUTION };
    k->state.viewed = race->viewed;
    k->state.view_changed = race->view_taken;
    race_keys(&ops, &k->state, &inputs);
    race->viewed = k->state.viewed;
    race->view_taken = k->state.view_changed;
}
