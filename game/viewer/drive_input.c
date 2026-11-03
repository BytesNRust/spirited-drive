/* --drive's controls through the race's input path (drive_input.h). */
#include "viewer/drive_input.h"

#include "platform/platform.h"
#include "game/sys/key_latch.h"
#include "game/frontend/config.h"
#include "game/race/input_read.h"
#include "game/race/input_tick.h"
#include "game/race/race_keys.h"
#include "game/sys/port_controls.h"
#include "game/sys/resource.h"
#include "viewer/drive_autopilot.h"
#include "viewer/drive_joy.h"
#include "viewer/track_viewer.h"

#include <string.h>

/* config.dat's defaults for 0052ad26 (game/frontend/config_dat.c): keys for
 * all three devices; left, right, up, down; A, Z, space, C, H, M. */
static const int16_t DEFAULT_CONFIG[INPUT_CONFIG_WORDS] = { 0, 0, 0, 0x4b00, 0x4d00, 0x4800, 0x5000,
                                                            0x41, 0x5a, 0x20, 0x43, 0x48, 0x4d };

enum { WAIT_LIMIT = 4000, KEY_CODES = 0x5c };  /* set-1 codes up to F12, then the pad's View, Nitro and Look back (RACE_KEYS_PAD_*) */  /* a stand-in for 004397f4's idle count: the queue is filled before each take */

typedef struct {
    DriveInput *in;
    RaceState *race;
    InputWords words;
    int32_t waits;
    int16_t *dropped;
    InputTickWords *tick;
    uint32_t wait_ms;           /* when the wait for this player's word began */
    int32_t idle;               /* the idle count last written (004397f4 sets 0 for each player) */
} DriveCtx;

static void no_range(void *ctx, int32_t l, int32_t t, int32_t r, int32_t b) { (void)ctx, (void)l, (void)t, (void)r, (void)b; }
static int32_t no_buttons(void *ctx) { (void)ctx; return 0; }
static int32_t pointer_x(void *ctx) { (void)ctx; return 0; }
static void pad_read(void *ctx, int32_t player, int16_t stand_in) { (void)player, (void)stand_in; drive_joy_pad_read(ctx); }
static void no_send(void *ctx, int32_t to, const int16_t *words, int32_t size) { (void)ctx, (void)to, (void)words, (void)size; }
static void no_call(void *ctx) { (void)ctx; }
static void no_drop(void *ctx, int16_t player) { (void)ctx, (void)player; }

static uint32_t read_word(void *ctx)
{
    DriveCtx *c = ctx;
    const Car *own = c->race->cars[c->race->player];
    if (drive_autopilot_on() && own != NULL) return drive_autopilot_word(own, c->race, c->in->own_specs);  /* port: test harness only */
    InputPad pad = { 0, 0, 0 };
    const InputReadOps ops = { &pad, pad_read, { NULL, no_range, no_buttons, pointer_x } };
    MouseSteerWords mouse = { c->race->replay, c->race->viewed, 0, c->race->unknown_513e6c, c->race->mirrored, 0, 0x20 };
    const InputReadInputs inputs = { c->in->keys, c->in->codes, c->in->stand_in_block + 1, c->race->controls,
                                     c->race->entries, 0, c->race->mirrored, drive_joy_state()->present,
                                     c->race->port_pad_and_keys };
    const uint32_t word = input_read(&ops, &pad, &mouse, &inputs);
    c->in->pad_buttons = pad.buttons;
    return word;
}

static void put_word(void *ctx, int32_t player, int32_t word)
{
    DriveCtx *c = ctx;
    input_put(&c->in->queues[player], (int16_t)word, &c->words);
}

static int take_word(void *ctx, int32_t player, uint32_t *control, int32_t slot)
{
    DriveCtx *c = ctx;
    return input_take(&c->in->queues[player], control, slot, &c->words);
}

/* 00475c14's place in the wait: never reached while the queue keeps up;
 * gives the player up rather than spin. */
static void wait_service(void *ctx)
{
    DriveCtx *c = ctx;
    if (++c->waits > WAIT_LIMIT) *c->dropped = 1;
}

/* --- a link race --- */

enum { WAIT_MS = 1, TICK_MS = 10, NOTICE_TICKS = 120 };  /* a wait's sleep; 100 Hz ticks; the slow notice's time */

static void link_send(void *ctx, int32_t to, const int16_t *words, int32_t size)
{
    const DriveCtx *c = ctx;
    c->in->link->send(to, words, size);
}

/* 00439b24: the host's store, each human's word for the slot sent round */
static void link_store(void *ctx)
{
    DriveCtx *c = ctx;
    const InputSendOps send = { ctx, link_send };
    input_store(c->in->queues, c->in->store_clock, c->race->humans, c->in->stand_in_block + 1, &send);
}

/* 0041e324: the network's notices on the HUD */
static void notice_slow(void *ctx)
{
    const DriveCtx *c = ctx;
    c->in->hud->waiting = 1, c->in->hud->lights_until = c->race->tick + NOTICE_TICKS;
}

/* 0041e318 */
static void notice_lost(void *ctx) { ((const DriveCtx *)ctx)->in->hud->lost = 1; }

/* 0041e340 */
static void notice_off(void *ctx)
{
    const DriveCtx *c = ctx;
    c->in->hud->lost = 0, c->in->hud->lights_until = c->race->tick;
}

/* 004463ec: the player given up */
static void link_drop(void *ctx, int16_t player) { ((const DriveCtx *)ctx)->in->link->drop(player); }

/* 00475c14 while a slot is not in: the transport serviced, the idle count (004bb8dc) the time waited in ticks */
static void link_wait(void *ctx)
{
    DriveCtx *c = ctx;
    if (c->waits++ == 0 || c->tick->idle != c->idle) c->wait_ms = platform_ticks_ms();
    c->in->link->service();
    platform_sleep_ms(WAIT_MS);
    c->tick->idle = c->idle = (int32_t)((platform_ticks_ms() - c->wait_ms) / TICK_MS);
}

static uint32_t link_tick(DriveInput *in, RaceState *race)
{
    const struct TrackViewerLink *l = in->link;
    l->service();  /* the original's transport runs on its own thread (0049ceb8): here once a tick */
    DriveCtx c = { in, race, { 1, 0, l->host, l->slot, 0, 0 }, 0, NULL, NULL, 0, 0 };
    const InputPollOps poll = { &c, read_word, link_send, put_word, link_store };
    InputPollWords pw = { race->replay, 1, l->slot, l->host, *in->late, 0 };
    input_poll(&poll, &pw);
    *in->late = pw.late;
    if (*l->lost != 0) race->dropped = *l->lost;  /* 0052b314 is the link's too */
    InputTickWords tw = { race->humans, 0, *in->timer, in->ticks, in->notice_on, in->notice_until, 0, 1,
                          (int16_t)race->dropped, 1, l->host, l->slot, 0 };
    c.dropped = &tw.dropped, c.tick = &tw;
    const InputTickOps tick = { &c, take_word, no_call, link_wait, notice_slow, notice_lost, notice_off, link_drop };
    uint32_t inputs[RACE_PLAYERS] = { 0 };
    input_tick(&tick, &tw, in->ticks, in->queues, inputs, in->stand_in_block + 1, race->players, race->launch_ready);
    in->ticks += 2;
    in->notice_on = tw.notice_on, in->notice_until = tw.notice_until;
    race->dropped = tw.dropped;
    if (race->dropped != 0) *l->lost = (int16_t)race->dropped;
    for (int32_t i = 1; i < race->humans && i < RACE_PLAYERS; i++) race->inputs[i] = inputs[i];
    return inputs[0];
}

void drive_input_start(DriveInput *in, RaceState *race)
{
    uint8_t w4fb788;
    memset(in, 0, sizeof *in);
    /* 0052ad26 as the controls screen (00413cb0) left it; config.dat's defaults when none was loaded */
    int16_t config[INPUT_CONFIG_WORDS];
    const ConfigDat *loaded = config_loaded();
    for (int i = 0; i < INPUT_CONFIG_WORDS; i++) {
        const uint8_t *w = loaded != NULL ? loaded->image + CONFIG_DAT_CONTROLS + 2 * i : NULL;
        config[i] = w != NULL ? (int16_t)(uint16_t)(w[0] | w[1] << 8) : DEFAULT_CONFIG[i];
    }
    input_setup(config, (int16_t)resource_german(), 0, race->controls, in->codes, &w4fb788);
    const PortControls port = port_controls_load();
    in->look_back_key = port_controls_look_back_key(&port);   /* port: the 360 camera's look back (race_follow.h) */
    port_controls_apply_keys(&port, in->codes);   /* port: the player's keyboard keys (controls.cfg) over config.dat's */
    platform_virtual_key(ACT_HORN, in->codes[INPUT_KEY_HORN]);   /* port: a pad's horn button presses the horn's key */
    platform_virtual_key(ACT_VIEW, RACE_KEYS_PAD_VIEW);          /* and its View button not F1, the mirror's now */
    platform_virtual_key(ACT_NITRO, RACE_KEYS_PAD_NITRO);        /* port: and its Nitro button, the NITRO start's key */
    platform_virtual_key(ACT_LOOK_BACK, RACE_KEYS_PAD_LOOK_BACK);   /* port: and its Look back button */
    in->stand_in_block = in->own_stand_ins;
    in->queues[0].active = 1;
}

void drive_input_read_keys(DriveInput *in)
{
    memset(in->keys, 0, sizeof in->keys);
    for (int code = 1; code < KEY_CODES; code++) in->keys[code] = platform_key_down(code);
    key_latch_step();   /* port: a tapped key has been read by one more step */
}

uint32_t drive_input_tick(DriveInput *in, RaceState *race)
{
    if (in->link != NULL) return link_tick(in, race);
    DriveCtx c = { in, race, { race->playback, 0, 1, 0, 0, 0 }, 0, NULL, NULL, 0, 0 };
    const InputPollOps poll = { &c, read_word, no_send, put_word, no_call };
    InputPollWords pw = { race->replay, race->playback, 0, 1, 0, 0 };
    input_poll(&poll, &pw);
    InputTickWords tw = { 1, 0, 0, in->ticks, 0, 0, 0, race->playback, 0, 0, 1, 0, 0 };
    c.dropped = &tw.dropped;
    const InputTickOps tick = { &c, take_word, no_call, wait_service, no_call, no_call, no_call, no_drop };
    uint32_t inputs[RACE_PLAYERS] = { 0 };
    input_tick(&tick, &tw, in->ticks, in->queues, inputs, in->stand_in_block + 1, race->players, race->launch_ready);
    in->ticks += 2;
    return inputs[0];
}
