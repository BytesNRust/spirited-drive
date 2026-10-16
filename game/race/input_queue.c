/* Each human's queue of control words (input_queue.h). */
#include "game/race/input_queue.h"

#include "game/gfx/x86_int.h"

enum { STAND_IN_KIND = 10, STAND_IN_SIZE = 0x12, GAVE_UP_TIME = 999999, STAND_IN = 0x87a0, GAVE_UP = 0x803f, SEND_WORDS = 10, SEND_SIZE = 0x12, SEND_KIND = 2, BEHIND = 0x2a, POLL_SIZE = 4, LATE_MAX = 0xf };

int input_take(InputQueue *q, uint32_t *control, int slot, InputWords *w)
{
    if (q->active == 0) return 1;
    if (w->playback != 0 && w->dropped != 0) {
        *control = STAND_IN;
        q->ready[slot] = 0;
        q->filled[slot] = 1;
        return 1;
    }
    if (q->ready[slot] != 0) w->waiting = 0;
    else if (w->w4fbb0c == 0 || w->playback != 1) {
        if (w->playback != 1 && w->player != (int32_t)*control) w->waiting = 1;
        return 0;
    }
    *control = (uint32_t)(int32_t)q->input[slot];
    q->ready[slot] = 0;
    return 1;
}

void input_store(InputQueue *queues, int32_t *clock, int32_t humans, const int16_t *stand_in, const InputSendOps *ops)
{
    const int slot = (int)((uint32_t)(*clock / 2) & (INPUT_SLOTS - 1));
    int16_t words[SEND_WORDS] = { 0 };
    for (int i = 0; i < humans; i++) {
        InputQueue *q = &queues[i];
        if (q->gave_up != 0) {
            q->input[slot] = (int16_t)GAVE_UP;
            q->clock = x_add(q->clock, 2);
        } else if (q->filled[slot] == 0) {
            q->input[slot] = q->input[(slot - 1) & (INPUT_SLOTS - 1)];
            if (stand_in[i] != 0) q->input[slot] = (int16_t)STAND_IN;
            q->clock = x_add(q->clock, 2);
        }
        if (i + 1 < SEND_WORDS) words[i + 1] = q->input[slot];
        q->filled[slot] = 0;
        q->ready[slot] = 1;
    }
    words[0] = SEND_KIND;
    *clock = x_add(*clock, 2);
    ops->send(ops->ctx, 0, words, SEND_SIZE);
}

void input_put(InputQueue *q, int16_t word, InputWords *w)
{
    if (q->active == 0) return;
    const int slot = (int)((uint32_t)(q->clock / 2) & (INPUT_SLOTS - 1));
    if (w->playback == 1) {
        if ((q->ready[slot] != 0 || (w->w4fbb0c != 0 && q->filled[slot] != 0)) && w->dropped == 0) {
            w->dropped = 1;
            w->w4fcdac = 1;
            w->waiting = 1;
        }
    } else if (q->ready[(slot + BEHIND) & (INPUT_SLOTS - 1)] != 0) {
        w->waiting = 1;
    }
    q->ready[slot] = 1;
    q->filled[slot] = 1;
    q->input[slot] = word;
    q->clock = x_add(q->clock, 2);
}

void input_poll(const InputPollOps *ops, InputPollWords *w)
{
    if (w->replay != 0) return;
    const uint32_t word = ops->read(ops->ctx);
    const int32_t put = (int16_t)(uint16_t)word;
    if (w->w4fbb0c == 0 || w->playback == 2 || w->playback == 3) {
        const int16_t words[2] = { SEND_KIND, (int16_t)(uint16_t)word };
        ops->send(ops->ctx, 0, words, POLL_SIZE);
        if (w->playback == 1) {
            w->late = x_add(w->late, 1);
            if (w->late > LATE_MAX) w->waiting = 1;
        } else {
            ops->put(ops->ctx, w->player, put);
        }
        return;
    }
    ops->put(ops->ctx, w->player, put);
    if (w->playback == 1 && w->w4fbb0c != 0) ops->store(ops->ctx);
}

void input_stand_in(int32_t player, int16_t *block, PlayerRecord *records, const InputStandInWords *w,
                    const InputSendOps *ops)
{
    if (w->race_type == 0 || w->replay != 0) return;
    if (player == w->player && records[player].finish != 0) return;
    if (block[1 + player] != 0 && w->playback != 3 && w->playback != 2) return;
    block[0] = STAND_IN_KIND;
    block[1 + player] = 1;
    if (w->playback != 0) ops->send(ops->ctx, 0, block, STAND_IN_SIZE);
    if (w->w4fbb0c != 0) records[player].finish = GAVE_UP_TIME;
}

void input_stand_in_start(int16_t *block, PlayerRecord *records, const InputStandInStartWords *w,
                          const InputStandInStartOps *ops)
{
    if (w->w4fbb0c == 0) return;
    records[0].finish = 0;
    block[1] = 0;
    if (w->playback == 0) return;
    for (int32_t i = 1; i < w->racers && i < RACE_PLAYERS; i++) {
        const int out = i < w->humans && w->playback == 1 && ops->linked(ops->ctx, i) == 0;
        block[1 + i] = (int16_t)out;
        records[i].finish = out ? GAVE_UP_TIME : 0;
    }
    block[0] = STAND_IN_KIND;
    ops->send(ops->ctx, 0, block, STAND_IN_SIZE);
}

void input_queue_reset(InputQueue *q, int32_t *late, int32_t *unknown_4fb78c)
{
    q->active = 0;
    q->clock = 1;
    q->gave_up = 0;
    for (int i = 0; i < INPUT_SLOTS; i++) q->ready[i] = q->filled[i] = 0;
    *late = 0;
    *unknown_4fb78c = 0;
}

void input_queues_reset(InputQueue *queues, int32_t humans, InputResetWords *w)
{
    w->camera_keys = w->w4fb7ac = w->w4fb7b0 = w->w4bafe4 = 0;
    for (int32_t i = 0; i < humans && i < INPUT_QUEUES; i++)  /* port guard */
        input_queue_reset(&queues[i], &w->late, &w->w4fb78c);
}

enum { PRIME_WORD = 0x7a0, PRIME_LEAST = 2 };

void input_queue_prime(InputQueue *q, int32_t net_start, int32_t wait_ticks, int32_t *clock, int32_t *replay_due)
{
    if (net_start != 1) {
        q->clock = 0;
        return;
    }
    int32_t n = wait_ticks >> 1;
    if (n < PRIME_LEAST) n = PRIME_LEAST;
    for (int32_t i = 0; i < n && i < INPUT_SLOTS; i++) {  /* port guard: the original writes on past the queue */
        q->input[i] = PRIME_WORD;
        q->ready[i] = 1;
    }
    n = x_add(n, n);
    *clock = n;
    q->clock = n;
    *replay_due = x_sub(n, 2);
}

static int16_t message_word(const uint8_t *data, int at) { return (int16_t)(uint16_t)(data[at] | data[at + 1] << 8); }

static void on_host_message(const InputMessageOps *ops, InputQueue queues[RACE_PLAYERS], InputMessageWords *m,
                            uint32_t link, const uint8_t *data)
{
    const int16_t slot = ops->slot_of(ops->ctx, link);
    if (slot < 0 || slot >= RACE_PLAYERS) return;  /* port guard */
    InputQueue *q = &queues[slot];
    if (x_add(queues[0].clock, 2) < q->clock)
        q->input[(x_add(q->clock, -2) / 2) & (INPUT_SLOTS - 1)] = message_word(data, 2);
    else
        input_put(q, message_word(data, 2), &m->w);
}

void input_on_message(const InputMessageOps *ops, InputQueue queues[RACE_PLAYERS], InputMessageWords *m, uint32_t link,
                      const uint8_t *data)
{
    ops->lock(ops->ctx, m->sem);
    if (m->w.playback != 1) {
        if (m->w.w4fbb0c >= 0 && m->w.w4fbb0c < RACE_PLAYERS) m->w4c5598 = queues[m->w.w4fbb0c].clock;  /* port guard */
        input_put(&queues[m->w.player == 0 ? 1 : 0], message_word(data, 2), &m->w);
    } else if (m->w.w4fbb0c != 0) {
        on_host_message(ops, queues, m, link, data);
    } else {
        m->late = 0;
        if (m->w4fb78c == 0) m->w.waiting = 0;
        m->w4c5598 = queues[0].clock;
        for (int16_t i = 0; i < m->humans && i < RACE_PLAYERS; i++)  /* port guard */
            input_put(&queues[i], message_word(data, 2 + 2 * i), &m->w);
    }
    ops->unlock(ops->ctx, m->sem);
}
