/* --drive's controls through the race's input path (drive_input.h). */
#include "viewer/drive_input.h"

#include "platform/platform.h"
#include "game/race/input_read.h"
#include "game/race/input_tick.h"

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
} DriveCtx;

static void no_range(void *ctx, int32_t l, int32_t t, int32_t r, int32_t b) { (void)ctx, (void)l, (void)t, (void)r, (void)b; }
static int32_t no_buttons(void *ctx) { (void)ctx; return 0; }
static int32_t pointer_x(void *ctx) { (void)ctx; return 0; }
static void no_pad(void *ctx, int32_t player, int16_t stand_in) { (void)ctx, (void)player, (void)stand_in; }
static void no_send(void *ctx, int32_t to, const int16_t *words, int32_t size) { (void)ctx, (void)to, (void)words, (void)size; }
static void no_call(void *ctx) { (void)ctx; }
static void no_drop(void *ctx, int16_t player) { (void)ctx, (void)player; }

static uint32_t read_word(void *ctx)
{
    DriveCtx *c = ctx;
    const InputReadOps ops = { NULL, no_pad, { NULL, no_range, no_buttons, pointer_x } };
    InputPad pad = { 0, 0, 0 };
    MouseSteerWords mouse = { c->race->replay, c->race->viewed, 0, c->race->unknown_513e6c, 0, 0, 0x20 };
    const InputReadInputs inputs = { c->in->keys, c->in->codes, c->in->stand_in_block + 1, c->race->controls,
                                     c->race->entries, 0, 0, 0 };
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

void drive_input_start(DriveInput *in, RaceState *race)
{
    uint8_t w4fb788;
    memset(in, 0, sizeof *in);
    input_setup(DEFAULT_CONFIG, 0, 0, race->controls, in->codes, &w4fb788);
    in->queues[0].active = 1;
}

void drive_input_read_keys(DriveInput *in)
{
    memset(in->keys, 0, sizeof in->keys);
    for (int code = 1; code < KEY_CODES; code++) in->keys[code] = platform_key_down(code);
}

uint32_t drive_input_tick(DriveInput *in, RaceState *race)
{
    DriveCtx c = { in, race, { race->playback, 0, 1, 0, 0, 0 }, 0, NULL };
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
