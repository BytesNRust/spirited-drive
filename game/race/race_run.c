/* The race (race_run.h), as 00455a5c runs it. */
#include "game/race/race_run.h"

#include "game/gfx/x86_int.h"

#include <stddef.h>

enum {
    SCREEN_W = 0x280, SCREEN_H = 0x1e0, SCREEN_BITS = 8,
    NET_EVERYONE = -1, NET_LOADED = 6, NET_STARTED = 7,
    TIME_TRIAL = 0, SHORT_START_TRACK = 3, SHORT_START_NODE = 5, SHORT_START_Z = 0x1e0000,
    STAGE_NODES_SHIFT = 16, STAGE_NODES_LESS = 0xb5,
    START_TICKS = 3,
    PHASE_RACING = 1, PHASE_HELD = 2, PHASE_OVER = 3,
    HOLD_RUN = 0, HOLD_STEP = 4,
    BEHIND = 0xe,
    PLAY_LIVE = 0, PLAY_REPLAY = 1, PLAY_WAIT = 2, PLAY_WAIT_STEP = 3, PLAY_STEP = 2,
    END_DROPPED = 2, DROP_AFTER = 4
};

/* 00455a62..00455ce7: the words started, the screen, the loading picture,
 * the sounds, the detail, the track's info, the cars, the network's first
 * wait (6), the computer cars, a time trial on track 3 starting at node 5,
 * the palettes, the dash, the music, and with the back buffer the screen
 * cleared. */
static void setup(const RaceRunOps *ops, RaceState *race, RaceRunWords *w)
{
    for (int i = 0; i < 6; i++) race->start_beeps[i] = 1;
    w->unknown_513e5c = race->tick = race->phase = w->done = w->due = 0;
    race->camera_hold = race->unknown_513e64 = race->unknown_513e60 = 0;
    w->running = 1;
    race->unknown_513e6c = 0;
    ops->service(ops->ctx);
    ops->screen_mode(ops->ctx, SCREEN_W, SCREEN_H, SCREEN_BITS);
    ops->load_pick(ops->ctx, 1);
    ops->service(ops->ctx);
    ops->setup_43cbe0(ops->ctx);
    ops->force_43cbf4(ops->ctx);
    ops->service(ops->ctx);
    ops->setup_443b64(ops->ctx);
    ops->setup_443924(ops->ctx);
    ops->start_voices(ops->ctx);
    ops->engines_start(ops->ctx);
    ops->service(ops->ctx);
    ops->detail_setup(ops->ctx, w->high);
    ops->track_info(ops->ctx);
    ops->service(ops->ctx);
    ops->setup_457594(ops->ctx);
    ops->service(ops->ctx);
    ops->setup_44edd8(ops->ctx);
    ops->cars_enter(ops->ctx);
    ops->trail_bank(ops->ctx);
    ops->manager_start(ops->ctx);
    if (race->unknown_4fb7d4 == 1 && w->net_failed == 0 && w->laps_low == 0) {
        ops->net_send(ops->ctx, NET_EVERYONE);
        if (ops->net_wait(ops->ctx, NET_LOADED) == 0) w->net_failed = 1;
    }
    ops->service(ops->ctx);
    ops->setup_44d2c4(ops->ctx);
    ops->service(ops->ctx);
    ops->ai_setup(ops->ctx);
    if (race->race_type == TIME_TRIAL && race->track == SHORT_START_TRACK && race->cars[0] != NULL) {  /* port guard */
        race->cars[0]->node = SHORT_START_NODE;
        race->cars[0]->progress = SHORT_START_NODE;
        race->cars[0]->z = SHORT_START_Z;
    }
    ops->service(ops->ctx);
    ops->palette_cars(ops->ctx);
    ops->setup_446a0c(ops->ctx);
    ops->pointer_load(ops->ctx, 1);
    ops->service(ops->ctx);
    ops->dash_colours(ops->ctx);
    ops->service(ops->ctx);
    ops->dash_load(ops->ctx);
    ops->setup_48f8e0(ops->ctx);
    ops->setup_473578(ops->ctx);
    ops->palette_writes(ops->ctx);
    ops->service(ops->ctx);
    ops->cockpit_colours(ops->ctx);
    ops->setup_4574ec(ops->ctx);
    ops->hud_colours(ops->ctx);
    ops->setup_446aec(ops->ctx);
    ops->palette_banks(ops->ctx);
    ops->service(ops->ctx);
    ops->setup_446918(ops->ctx);
    ops->setup_4470dc(ops->ctx);
    ops->setup_446af0(ops->ctx);
    if (w->music_on != 0) {
        w->music_started = 1;
        ops->music_setup(ops->ctx);
        ops->music(ops->ctx);
    }
    ops->service(ops->ctx);
    if (w->back_buffer == 0) return;
    ops->select_front(ops->ctx);
    ops->clip(ops->ctx, 0, w->surface_width, 0, w->surface_height);
    ops->setup_48f8e0(ops->ctx);
}

/* 00455cf8..00455e6d: the first start (00513e64 0): the timer and the
 * counts from 0, the humans' queues reset and on, the network's second wait
 * (7) behind the loading picture or the wait screen, the laps (a circuit's
 * segments x 4 and the laps chosen, a stage's nodes x 4 - 0xb5 and 1),
 * three ticks, and racing. */
static void start(const RaceRunOps *ops, RaceState *race, RaceRunWords *w)
{
    w->unknown_4fcdac = 1;
    w->timer = w->replay_due = race->tick = w->done = w->due = 0;
    w->unknown_4fcd90 = w->notice_on = 0;
    race->tick_flags = 0xff;
    w->tick_flags_upper = 0xffffff;
    for (int32_t i = 0; i < race->humans && i < RACE_PLAYERS; i++) {  /* port guard: eight queues */
        ops->queue_reset(ops->ctx, &w->queues[i]);
        ops->queue_clear(ops->ctx, &w->queues[i]);
    }
    for (int32_t i = 0; i < race->humans && i < RACE_PLAYERS; i++) w->queues[i].active = 1;
    if (race->unknown_4fb7d4 == 1 && w->net_failed == 0 && w->laps_low == 0) {
        if (w->back_buffer != 0) {
            ops->show_loading(ops->ctx);
            ops->video_flip(ops->ctx);
        } else {
            ops->wait_screen(ops->ctx);
        }
        ops->net_send(ops->ctx, NET_EVERYONE);
        if (ops->net_wait(ops->ctx, NET_STARTED) == 0) w->net_failed = 1;
    }
    if (race->circuit != 0)
        ops->laps_start(ops->ctx, x_shl(race->segments, 2), race->laps_setting);
    else
        ops->laps_start(ops->ctx, x_sub(x_shl(w->track_header_4 >> STAGE_NODES_SHIFT, 2), STAGE_NODES_LESS), 1);
    for (int i = 0; i < START_TICKS; i++) ops->tick(ops->ctx);
    race->unknown_513e64 = 1;
    race->unknown_513e60 = 0;
    race->phase = PHASE_RACING;
    w->unknown_4fcdac = w->waiting = w->unknown_4fcd90 = w->unknown_5120d4 = 0;
}

/* 00455e8c..00455f1b: the frames due: the timer live, less the wait
 * (playback 2), the timer less the wait but at least two past the frames
 * run (3), playback 1's own count (or the timer with 004fbb0c), the same
 * two at least; other playbacks keep the last. */
static void due(RaceState *race, RaceRunWords *w)
{
    switch (race->playback) {
    case PLAY_LIVE: w->due = w->timer; return;
    case PLAY_WAIT: w->due = x_sub(w->timer, w->wait_ticks); return;
    case PLAY_WAIT_STEP: w->due = x_sub(w->timer, w->wait_ticks); break;
    case PLAY_REPLAY:
        if (race->unknown_4fbb0c != 0) {
            w->due = w->timer;
            return;
        }
        w->due = w->replay_due;
        break;
    default: return;
    }
    if (w->due <= w->done) w->due = x_add(w->done, PLAY_STEP);
}

/* 00455f47..00455fba: held (phase 2) and running or stepping (004c5398 0
 * or 4): every frame due while the phase holds, then the draw. */
static void held_run(const RaceRunOps *ops, RaceState *race, RaceRunWords *w)
{
    if (w->done < w->due) {
        int drawn = 1;
        while (w->done < w->due) {
            ops->frame(ops->ctx, w->done);
            if (race->phase != PHASE_HELD) {
                drawn = 0;
                break;
            }
            w->done = x_add(w->done, 1);
        }
        if (drawn && race->phase != PHASE_OVER) ops->draw(ops->ctx);
    }
    w->unknown_513e84 = 0;
}

/* 00456078..00456115: held and slowed (004c5398 below 4, not 0): a frame
 * only every 004c53a0 + 1 due, counted down in 00513e84; the draw when one
 * ran and the phase held. */
static void held_slow(const RaceRunOps *ops, RaceState *race, RaceRunWords *w)
{
    if (race->unknown_4c5398 >= HOLD_STEP || w->done >= w->due) return;
    int held = 1, ran = 0;
    while (w->done < w->due) {
        if (w->unknown_513e84 <= 0) {
            w->unknown_513e84 = w->unknown_4c53a0;
            ops->frame(ops->ctx, w->done);
            ran = 1;
            if (race->phase != PHASE_HELD) {
                held = 0;
                break;
            }
        }
        w->unknown_513e84 = x_sub(w->unknown_513e84, 1);
        w->done = x_add(w->done, 1);
    }
    if (ran && held && race->phase != PHASE_OVER) ops->draw(ops->ctx);
}

/* 00456123..004561e1: racing: each frame due (004fcdac set while 15 or
 * more are behind, live), until the phase, the end request or the dropped
 * game stops it; then the draw unless one did. */
static void racing(const RaceRunOps *ops, RaceState *race, RaceRunWords *w)
{
    if (w->done >= w->due) return;
    while (w->done < w->due) {
        if (race->playback == PLAY_LIVE) w->unknown_4fcdac = x_sub(w->due, w->done) > BEHIND;
        ops->service(ops->ctx);
        ops->frame(ops->ctx, w->done);
        if (race->phase == PHASE_OVER || race->phase == PHASE_HELD || race->end_request != 0 || race->dropped != 0) break;
        w->done = x_add(w->done, 1);
    }
    if (race->phase == PHASE_OVER || race->dropped != 0 || race->end_request != 0) return;
    ops->draw(ops->ctx);
}

/* 00455e8c..00456007: one pass of the loop. */
static void pass(const RaceRunOps *ops, RaceState *race, RaceRunWords *w)
{
    ops->service(ops->ctx);
    due(race, w);
    const int32_t behind = x_sub(w->timer, w->done);
    if (race->phase == PHASE_HELD) {
        if (behind <= BEHIND) w->unknown_4fcdac = 0;
        if (race->unknown_4c5398 == HOLD_STEP || race->unknown_4c5398 == HOLD_RUN)
            held_run(ops, race, w);
        else
            held_slow(ops, race, w);
    } else if (race->phase == PHASE_OVER) {
        if (behind <= BEHIND) w->unknown_4fcdac = 0;
        while (race->phase == PHASE_OVER) ops->frame(ops->ctx, 0);
    } else {
        racing(ops, race, w);
    }
    if (race->dropped != 0) race->end_state = END_DROPPED;
    if (race->end_state != 0) w->running = 0;
    if (w->net_failed != 0 && w->done > DROP_AFTER) {
        race->dropped = 1;
        w->running = 0;
    }
}

/* 0045600d..0045606c: everything taken down. */
static void teardown(const RaceRunOps *ops, RaceRunWords *w)
{
    w->unknown_4fcdac = w->waiting = 1;
    ops->force_43cbf4(ops->ctx);
    ops->end_43cbcc(ops->ctx);
    ops->end_hook(ops->ctx);
    ops->pointer_load(ops->ctx, 0);
    ops->end_446acc(ops->ctx);
    ops->end_44d2e4(ops->ctx);
    ops->end_450a68(ops->ctx);
    ops->cockpit_teardown(ops->ctx);
    ops->end_45106c(ops->ctx);
    ops->end_40d6f4(ops->ctx);
    ops->end_44eef8(ops->ctx);
    ops->end_440968(ops->ctx);
    ops->trail_free(ops->ctx);
    ops->results_45aa34(ops->ctx);
    ops->end_443be0(ops->ctx);
}

void race_run(const RaceRunOps *ops, RaceState *race, RaceRunWords *w)
{
    setup(ops, race, w);
    if (race->unknown_513e64 == 0) start(ops, race, w);
    ops->setup_461f54(ops->ctx);
    ops->setup_410250(ops->ctx);
    while (w->running != 0) pass(ops, race, w);
    teardown(ops, w);
}
