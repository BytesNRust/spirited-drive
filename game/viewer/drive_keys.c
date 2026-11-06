/* --drive's key commands (drive_keys.h). */
#include "viewer/drive_keys.h"

#include "viewer/drive_audio.h"
#include "viewer/drive_joy.h"
#include "viewer/track_viewer.h"
#include "game/race/input_queue.h"
#include "game/race/race_chase.h"
#include "game/race/race_detail.h"
#include "game/race/race_follow.h"
#include "game/race/race_loop.h"
#include "game/race/race_sound.h"
#include "game/sys/port_extras.h"

#include <string.h>

enum {
    FOLLOW_KIND = 4,          /* 004381f8's Alt + Ctrl + n: 00404530(004f801c, car n, 4) */
    HIGH_RESOLUTION = 1,      /* 004fa0c0: 640 x 480 */
    W513730_UNKNOWN = 0       /* 00513730: not ported; anything but 5 lets the hold key act */
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

/* 00445e00: nothing alone, through the link in a link race */
static void send_stand_in(void *ctx, int32_t to, const int16_t *words, int32_t size)
{
    const KeysCtx *c = ctx;
    if (c->in->link != NULL) c->in->link->send(to, words, size);
}

static void reset(void *ctx, int32_t player)
{
    KeysCtx *c = ctx;
    const InputSendOps send = { c, send_stand_in };
    /* 004fbb0c taken as 1 in a local game, as drive_input does; the link's in a link race */
    const int32_t host = c->in->link != NULL ? c->in->link->host : 1;
    const InputStandInWords w = { (int16_t)c->race->race_type, c->race->replay, c->race->playback, c->race->player, host };
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

static int port_f3_ultra;   /* port_f3_ultra */
static int port_view_ultra; /* the menu's Ultra: port.cfg's view_ultra, or TNFS_VIEW_ULTRA */

void drive_keys_port(int f3_ultra, int view_ultra)
{
    port_f3_ultra = f3_ultra != 0;
    port_view_ultra = view_ultra != 0;
}

void drive_keys_race_begins(DriveKeys *k)
{
    k->state.ultra = port_view_ultra != 0 && k->state.detail == 0;
}

void drive_keys_start(DriveKeys *k)
{
    const RaceKeysState was = k->state;
    const int loaded = k->loaded;
    memset(k, 0, sizeof *k);
    race_keys_start(&k->state, loaded ? &was : NULL);
    /* port: the car markers as the mode's file has them (On is the exe's, hidden 0), not as the last race left them */
    k->state.hidden = (uint8_t)!port_extras_race_on(RACE_SWITCH_MARKERS);
    k->detail_pending = -1;  /* 004c5360: nothing asked for */
    k->loaded = 1;
}

void drive_keys_frame(DriveKeys *k, DriveInput *in, RaceState *race, RaceViews *views)
{
    KeysCtx c = { k, in, race, views };
    const RaceKeysOps ops = { &c, follow, viewed_changed, follow_own, own_changed, reset, f1, screen, detail, sound_off,
                              sound_on };
    const int32_t timer = in->timer != NULL ? *in->timer : 0;
    const RaceKeysInputs inputs = { in->keys, in->codes[INPUT_KEY_HOLD], drive_joy_state()->present, in->pad_buttons,
                                    race->humans, race->phase, W513730_UNKNOWN, race->race_type, race->replay,
                                    race->playback, timer, race->player, 0, HIGH_RESOLUTION, 1, port_f3_ultra };
    k->state.viewed = race->viewed;
    k->state.view_changed = race->view_taken;
    const uint8_t hidden_before = k->state.hidden;
    race_keys(&ops, &k->state, &inputs);
    /* port: F6 flipped the car markers: the new value goes to the mode's file, so the next race starts with it */
    if (k->state.hidden != hidden_before) port_extras_set_race_switch(RACE_SWITCH_MARKERS, !k->state.hidden);
    race->chase.port_look_back = race_look_back_held(race->port_follow_camera, in->keys, (int32_t)sizeof in->keys, in->look_back_key);   /* port: the 360 camera's look back */
    race_look_back_view(race);
    race->viewed = k->state.viewed;
    race->view_taken = k->state.view_changed;
}
