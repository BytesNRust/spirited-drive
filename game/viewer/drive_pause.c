/* --drive's deferred calls (drive_pause.h). */
#include "viewer/drive_pause.h"

#include "game/platform/platform.h"
#include "game/race/car_held.h"
#include "game/race/race_chase.h"
#include "game/race/race_detail.h"

#include <string.h>

enum {
    KEY_ESC = 1, KEY_PAUSE = 0x19, KEY_LSHIFT = 0x2a, KEY_RSHIFT = 0x36,
    DETAIL_UNKNOWN = 0,     /* 004fb785: not ported; the menu's draw distance request is all it picks */
    PAUSE_IDLE_MS = 1
};

typedef struct {
    DrivePause *p;
    const DriveInput *in;
    RaceState *race;
    DriveHud *hud;
    RacePauseInputs pause;
    int32_t detail_pending;     /* 004c5360: nothing takes it (drive_keys.h) */
} PauseCtx;

static void none(void *ctx) { (void)ctx; }
static void none_i(void *ctx, int32_t a) { (void)ctx, (void)a; }
static int32_t none_wait(void *ctx, int32_t kind) { (void)ctx, (void)kind; return 1; }

/* 00475c14(0) while paused: the window's events and the P key; a closed
 * window flips the key so every wait ends. */
static void serve(void *ctx)
{
    PauseCtx *c = ctx;
    if (!platform_poll_events()) c->p->quit = 1;
    if (c->p->quit) {
        c->pause.pause = c->pause.pause == 0;
        return;
    }
    c->pause.pause = platform_key_down(KEY_PAUSE);
    platform_sleep_ms(PAUSE_IDLE_MS);
}

static void hud_clear(void *ctx) { drive_hud_clear(((PauseCtx *)ctx)->hud); }

static void take(void *ctx, int32_t kind)
{
    PauseCtx *c = ctx;
    race_chase_take(c->race, &c->race->chase, kind);
}

/* 00455600. The sound calls (0041eba4, 004437c8, 004437f0) wait for the
 * audio (option B); 0043cbcc / 0043cbe0 act only with the CD audio
 * (0052b320); 00524e63 is 0 here, so 00464240 is not reached; 004fb7d4 is 0
 * (no network). */
static void pause(PauseCtx *c)
{
    c->pause = (RacePauseInputs){ c->in->keys[KEY_PAUSE], 0, c->race->chase.kind, 0, 0, c->race->tick };
    const RacePauseOps ops = { c, none, none, none, serve, serve, hud_clear, take, none, none_i, none_wait, none, none };
    const uint32_t start = platform_ticks_ms();
    race_pause(&ops, &c->pause, &c->p->keys, &c->hud->police.loading);
    c->p->paused_ms += platform_ticks_ms() - start;
}

/* 004b90dc: the in-race menu (00447564) is not ported yet. */
static void call(void *ctx, int which)
{
    if (which == RACE_SCHEDULE_PAUSE) pause(ctx);
}

static int32_t notice_up(void *ctx) { return race_stop_notice_up(((PauseCtx *)ctx)->race); }

static void no_send(void *ctx, int32_t to, const int16_t *words, int32_t size)
{
    (void)ctx, (void)to, (void)words, (void)size;
}

static void set(void *ctx, int16_t frame, int32_t which, int16_t arg)
{
    PauseCtx *c = ctx;
    const InputSendOps send = { NULL, no_send };
    const RaceScheduleWords w = { 0, 1, c->race->playback };  /* 004fbb0c taken as 1 in a local game, as drive_input does */
    race_schedule_set(&send, c->p->sched, &w, frame, which, arg);
}

static void detail(void *ctx, int32_t segments) { race_detail_request(segments, &((PauseCtx *)ctx)->detail_pending); }

void drive_pause_start(DrivePause *p)
{
    memset(p, 0, sizeof *p);
}

void drive_pause_frame(DrivePause *p, const DriveInput *in, RaceState *race, DriveHud *hud, int16_t frame)
{
    PauseCtx c = { p, in, race, hud, { 0 }, -1 };
    const RaceScheduleOps sched = { &c, call };
    race_schedule_run(&sched, p->sched, frame);
    const RaceMenuKeysOps keys = { &c, notice_up, set, detail };
    const RaceMenuKeysInputs inputs = { race->tick, race->phase, in->keys[KEY_ESC], in->keys[KEY_PAUSE],
                                        in->keys[KEY_LSHIFT], in->keys[KEY_RSHIFT], race->playback, race->replay,
                                        DETAIL_UNKNOWN };
    p->keys.dropped = race->dropped, p->keys.w509d70 = race->end_state;
    race_menu_keys(&keys, &inputs, &p->keys, frame);
    race->dropped = p->keys.dropped, race->end_state = p->keys.w509d70;
}
