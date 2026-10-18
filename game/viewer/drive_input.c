/* --drive's controls through the race's input path (drive_input.h). */
#include "viewer/drive_input.h"

#include "platform/platform.h"
#include "game/frontend/config.h"
#include "game/race/input_read.h"
#include "game/race/input_tick.h"
#include "game/sys/resource.h"
#include "viewer/drive_joy.h"
#include "viewer/track_viewer.h"

#include <string.h>

/* config.dat's defaults for 0052ad26 (game/frontend/config_dat.c): keys for
 * all three devices; left, right, up, down; A, Z, space, C, H, M. */
static const int16_t DEFAULT_CONFIG[INPUT_CONFIG_WORDS] = { 0, 0, 0, 0x4b00, 0x4d00, 0x4800, 0x5000,
                                                            0x41, 0x5a, 0x20, 0x43, 0x48, 0x4d };

enum { WAIT_LIMIT = 4000, KEY_CODES = 0x59 };  /* set-1 codes up to F12 */  /* a stand-in for 004397f4's idle count: the queue is filled before each take */

typedef struct {
    DriveInput *in;
    RaceState *race;
    InputWords words;
    int32_t waits;
    int16_t *dropped;
    InputTickWords *tick;
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
    InputPad pad = { 0, 0, 0 };
    const InputReadOps ops = { &pad, pad_read, { NULL, no_range, no_buttons, pointer_x } };
    MouseSteerWords mouse = { c->race->replay, c->race->viewed, 0, c->race->unknown_513e6c, 0, 0, 0x20 };
    const InputReadInputs inputs = { c->in->keys, c->in->codes, c->in->stand_in_block + 1, c->race->controls,
                                     c->race->entries, 0, 0, drive_joy_state()->present };
    return input_read(&ops, &pad, &mouse, &inputs);
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

enum { LINK_TIMER = 500, WAIT_MS = 1, TICK_MS = 10 };  /* 004bb8d8 past its hold; a wait's sleep; 100 Hz ticks */

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

/* 00475c14 while a slot is not in: the transport serviced, the idle count (004bb8dc) counted in ticks */
static void link_wait(void *ctx)
{
    DriveCtx *c = ctx;
    c->in->link->service();
    platform_sleep_ms(WAIT_MS);
    c->tick->idle = ++c->waits * WAIT_MS / TICK_MS;
}

static uint32_t link_tick(DriveInput *in, RaceState *race)
{
    const struct TrackViewerLink *l = in->link;
    l->service();  /* the original's transport runs on its own thread (0049ceb8): here once a tick */
    DriveCtx c = { in, race, { 1, 0, l->host, l->slot, 0, 0 }, 0, NULL, NULL };
    const InputPollOps poll = { &c, read_word, link_send, put_word, link_store };
    InputPollWords pw = { race->replay, 1, l->slot, l->host, *in->late, 0 };
    input_poll(&poll, &pw);
    *in->late = pw.late;
    InputTickWords tw = { race->humans, 0, LINK_TIMER, in->ticks, 0, 0, 0, 1, 0, 1, l->host, l->slot, 0 };
    c.dropped = &tw.dropped, c.tick = &tw;
    const InputTickOps tick = { &c, take_word, no_call, link_wait, no_call, no_call, no_call, no_drop };
    uint32_t inputs[RACE_PLAYERS] = { 0 };
    input_tick(&tick, &tw, in->ticks, in->queues, inputs, in->stand_in_block + 1, race->players, race->launch_ready);
    in->ticks += 2;
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
    in->queues[0].active = 1;
}

void drive_input_read_keys(DriveInput *in)
{
    memset(in->keys, 0, sizeof in->keys);
    for (int code = 1; code < KEY_CODES; code++) in->keys[code] = platform_key_down(code);
}

uint32_t drive_input_tick(DriveInput *in, RaceState *race)
{
    if (in->link != NULL) return link_tick(in, race);
    DriveCtx c = { in, race, { race->playback, 0, 1, 0, 0, 0 }, 0, NULL, NULL };
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
