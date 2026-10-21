#include "game/race/race_schedule.h"

#include "game/gfx/x86_int.h"

void race_schedule_run(const RaceScheduleOps *ops, RaceScheduled sched[RACE_SCHEDULE_SLOTS], int16_t frame)
{
    for (int i = 0; i < RACE_SCHEDULE_RUN; i++) {
        if (frame == 0 || sched[i].frame == 0 || frame < sched[i].frame) continue;
        ops->call(ops->ctx, i);
        sched[i].frame = 0;
    }
}

void race_schedule_clear(RaceScheduled sched[RACE_SCHEDULE_SLOTS])
{
    for (int i = 0; i < RACE_SCHEDULE_RUN; i++) sched[i] = (RaceScheduled){ 0, 0 };
}

enum { MESSAGE_KIND = 3, MESSAGE_BYTES = 8, SENT_DELAY = 8, PLAYBACK_DELAY = 600 };

void race_schedule_set(const InputSendOps *send, RaceScheduled sched[RACE_SCHEDULE_SLOTS], const RaceScheduleWords *w,
                       int16_t frame, int32_t which, int16_t arg)
{
    if (which < 0 || which >= RACE_SCHEDULE_SLOTS) return;
    sched[which].frame = frame;
    sched[which].arg = arg;
    if (w->w4fb7d4 != 1) return;
    const int16_t message[4] = { MESSAGE_KIND, (int16_t)which, arg, (int16_t)(frame + SENT_DELAY) };
    send->send(send->ctx, 0, message, MESSAGE_BYTES);
    if (w->w4fbb0c == 0 && w->playback == 1)
        sched[which].frame = (int16_t)(sched[which].frame + PLAYBACK_DELAY);
    else
        sched[which].frame = message[3];
}

enum {
    KEYS_RESET_BEFORE = 0x3c, KEYS_AFTER = 0x78, MENU_GAP = 0xb4, PAUSE_GAP = 0x78,
    PHASE_RACING = 1, DROPPED_MARK = 2
};

static const int32_t DETAIL_SEGMENTS[3] = { 0x1c, 0xe, 9 };

void race_menu_keys(const RaceMenuKeysOps *ops, const RaceMenuKeysInputs *in, RaceMenuKeysState *st, int16_t frame)
{
    if (in->tick < KEYS_RESET_BEFORE) st->pending = 0, st->last = 0;
    const int16_t next = (int16_t)(frame + 1);
    if (in->esc != 0 && in->phase == PHASE_RACING && in->tick > KEYS_AFTER && st->pending == 0) {
        if (in->playback != 0 && (in->lshift != 0 || in->rshift != 0)) {
            st->dropped = 1;
            st->w509d70 = DROPPED_MARK;
            return;
        }
        if (x_add(st->last, MENU_GAP) > in->tick) return;
        st->last = in->tick;
        if (in->replay != 0) return;
        ops->set(ops->ctx, next, RACE_SCHEDULE_MENU, 1);
        st->pending = 1;
        if (in->detail < 3) ops->detail(ops->ctx, DETAIL_SEGMENTS[in->detail]);
        return;
    }
    if (in->playback != 0 || in->pause == 0 || in->tick <= KEYS_AFTER || st->pending != 0) return;
    if (ops->notice_up(ops->ctx) != 0 || x_add(st->last, PAUSE_GAP) > in->tick) return;
    st->last = in->tick;
    ops->set(ops->ctx, next, RACE_SCHEDULE_PAUSE, 1);
    st->pending = 1;
}

enum { PAUSE_NET_TO = -1, PAUSE_NET_WAIT = 5 };

static void pause_serve(const RacePauseOps *ops, const RacePauseInputs *in)
{
    if (in->movie != 0)
        ops->movie(ops->ctx);
    else
        ops->service(ops->ctx);
}

static void pause_while_down(const RacePauseOps *ops, const RacePauseInputs *in)
{
    while (in->pause != 0) pause_serve(ops, in);
}

void race_pause(const RacePauseOps *ops, const RacePauseInputs *in, RaceMenuKeysState *st, int32_t *loading)
{
    *loading = 1;
    ops->sound_pause(ops->ctx);
    ops->voices_off(ops->ctx);
    ops->end_43cbcc(ops->ctx);
    pause_while_down(ops, in);
    while (in->pause == 0) pause_serve(ops, in);
    ops->hud_clear(ops->ctx);
    ops->take(ops->ctx, in->follow_kind);
    pause_while_down(ops, in);
    if (in->w4fb7d4 == 1 && in->w524e9f == 0) {
        ops->wait_screen(ops->ctx);
        ops->net_send(ops->ctx, PAUSE_NET_TO);
        ops->net_wait(ops->ctx, PAUSE_NET_WAIT);
    }
    ops->music_on(ops->ctx);
    st->pending = 0;
    st->last = in->port_resume_keys ? 0 : in->tick;
    ops->setup_43cbe0(ops->ctx);
    *loading = 0;
}

static int16_t message_word(const uint8_t *data, int at) { return (int16_t)(uint16_t)(data[at] | data[at + 1] << 8); }

void race_schedule_on_message(const InputSendOps *send, RaceScheduled sched[RACE_SCHEDULE_SLOTS],
                              const RaceScheduleInWords *w, uint8_t *data)
{
    const int relay = w->playback == 1 && w->w4fbb0c != 0;
    if (relay) data[6] = 1, data[7] = 0;
    const int16_t which = message_word(data, 2);
    if (which < 0 || which >= RACE_SCHEDULE_SLOTS) return;  /* port guard */
    sched[which].frame = message_word(data, 6);
    sched[which].arg = message_word(data, 4);
    if (!relay) return;
    const int16_t message[4] = { message_word(data, 0), which, message_word(data, 4), message_word(data, 6) };
    send->send(send->ctx, 0, message, MESSAGE_BYTES);
    sched[which].frame = (int16_t)(w->clock + SENT_DELAY);
}
