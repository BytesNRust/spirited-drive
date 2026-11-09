/* The race's side of the network game: the setup exchange and the start (race_net.h). */
#include "game/race/race_net.h"
#include "game/race/race_net_words.h"

#include "game/gfx/x86_int.h"

#include <string.h>

void race_net_send_info(const RaceNetInfoSendOps *ops, int16_t play, int16_t replay, uint8_t *record)
{
    if (play == 0 || replay != 0) return;
    put_state_word(record, 0, 0);
    ops->send(ops->ctx, 0, record, RACE_NET_INFO);
}

enum { SETUP_RACE_W4FB7D4 = 0x18 };

void race_net_send_setup(const RaceNetSetupSendOps *ops, uint8_t *setup, const uint8_t *state)
{
    race_net_send_setup_trailer(ops, setup, state, NULL, 0);
}

void race_net_send_setup_trailer(const RaceNetSetupSendOps *ops, uint8_t *setup, const uint8_t *state, const uint8_t *trailer, int32_t trailer_size)
{
    if ((uint32_t)(setup[SETUP_RACE_W4FB7D4] | setup[SETUP_RACE_W4FB7D4 + 1] << 8 | setup[SETUP_RACE_W4FB7D4 + 2] << 16 |
                   (uint32_t)setup[SETUP_RACE_W4FB7D4 + 3] << 24) != 1)
        return;
    const int32_t slot = state_word(state, RACE_NET_AT_TO);
    for (int k = 0; k < 4; k++) setup[RACE_NET_SETUP_4FBACC + k] = (uint8_t)((uint32_t)slot >> 8 * k);
    put_state_word(setup, 0, 1);
    uint8_t message[RACE_NET_SETUP_HEAD + RACE_NET_SETUP_CHUNK] = { 1, 0 };
    int32_t left = RACE_NET_SETUP;
    for (uint8_t chunk = 0; left > 0; chunk++) {
        const int32_t n = left > RACE_NET_SETUP_CHUNK ? RACE_NET_SETUP_CHUNK : left;
        message[2] = chunk;
        memcpy(message + RACE_NET_SETUP_HEAD, setup + RACE_NET_SETUP - left, (size_t)n);
        left -= n;
        ops->service(ops->ctx);
        ops->send(ops->ctx, 0, message, n + RACE_NET_SETUP_HEAD);
    }
    if (trailer != NULL && trailer_size > 0 && trailer_size <= RACE_NET_SETUP_TRAILER_MAX) {   /* port: after the setup's chunks, before the end */
        message[2] = RACE_NET_SETUP_TRAILER_CHUNK;
        memcpy(message + RACE_NET_SETUP_HEAD, trailer, (size_t)trailer_size);
        ops->service(ops->ctx);
        ops->send(ops->ctx, 0, message, trailer_size + RACE_NET_SETUP_HEAD);
    }
    ops->send(ops->ctx, 0, message, RACE_NET_SETUP_END);
}

/* Each of the humans' names (their record +2) to the menu's, 8 at most and each within its record (port guards) */
static void copy_names(uint8_t *names, const uint8_t *records, int32_t humans)
{
    for (int32_t i = 0; i < humans && i < RACE_NET_PLAYERS; i++) {  /* port guard */
        const uint8_t *from = records + i * RACE_NET_INFO + 2;
        uint8_t *to = names + i * RACE_NET_MENU_NAME;
        for (int k = 0; k < RACE_NET_INFO - 2; k++)  /* port guard */
            if ((to[k] = from[k]) == 0) break;
    }
}

void race_net_wait_setup(const RaceNetSetupWaitOps *ops, RaceNetSetupWait *w)
{
    if (w->play == 0) return;
    ops->deadline(ops->ctx, -1);
    while (w->flags_a[RACE_NET_SETUP_FLAG] == 0) {
        if (ops->step(ops->ctx) != 0) {
            w->dropped = 1;
            break;
        }
        if (w->play == 0) return;
    }
    w->flags_a[RACE_NET_SETUP_FLAG] = 0;
    copy_names(w->names, w->records, w->humans);
    ops->names_done(ops->ctx, w->humans);
}

static int32_t setup_dword(const uint8_t *setup, int at)
{
    return (int32_t)((uint32_t)setup[at] | (uint32_t)setup[at + 1] << 8 | (uint32_t)setup[at + 2] << 16 |
                     (uint32_t)setup[at + 3] << 24);
}

/* Each slot's wait for its info (0050a5c2[slot] 4) */
static void host_setup_wait(const RaceNetSetupHostOps *ops, RaceNetSetupHost *h)
{
    const RaceNetSend at = { .entries = h->entries };
    ops->deadline(ops->ctx, -1);
    for (int16_t slot = 1; slot < state_word(h->state, RACE_NET_AT_HUMANS) && slot < RACE_NET_LINKS; slot++) {  /* port guard */
        if (link_word(&at, h->links[slot]) != 0)
            while (h->flags_b[slot] != RACE_NET_SEATED)
                if (ops->step(ops->ctx) != 0) {
                    ops->drop(ops->ctx, slot);
                    break;
                }
        h->flags_b[slot] = 0;
    }
}

/* The setup's +0x2c = +0x28 + +0x20 + +0x30 (wrapping) */
static void setup_sum(uint8_t *setup)
{
    const int32_t sum = x_add(x_add(setup_dword(setup, RACE_NET_SETUP_SUM_A), setup_dword(setup, RACE_NET_SETUP_RACERS)),
                              setup_dword(setup, RACE_NET_SETUP_SUM_B));
    for (int k = 0; k < 4; k++) setup[RACE_NET_SETUP_SUM + k] = (uint8_t)((uint32_t)sum >> 8 * k);
}

void race_net_host_setup(const RaceNetSetupHostOps *ops, RaceNetSetupHost *h, const uint8_t *first, const uint8_t *second)
{
    ops->service(ops->ctx);
    setup_sum(h->setup);
    memcpy(h->setup + RACE_NET_SETUP_RECORD_A, first, RACE_NET_INFO);
    if (setup_dword(h->setup, SETUP_RACE_W4FB7D4) != 1) {
        memcpy(h->setup + RACE_NET_SETUP_RECORD_B, second, RACE_NET_INFO);
        return;
    }
    host_setup_wait(ops, h);
    const int32_t humans = setup_dword(h->setup, RACE_NET_SETUP_HUMANS);
    copy_names(h->names, h->setup + RACE_NET_SETUP_RECORD_A, humans);
    ops->names_done(ops->ctx, humans);
}

void race_net_setup_both(uint8_t *setup, const uint8_t *record)
{
    setup_sum(setup);
    memcpy(setup + RACE_NET_SETUP_RECORD_A, record, RACE_NET_INFO);
    memcpy(setup + RACE_NET_SETUP_RECORD_B, record, RACE_NET_INFO);
}

/* 0052b312 0 and 004fb7d4 1: 00513e58 = 1 (once); 1 then */
static int exchange_once(RaceNetExchange *x)
{
    if (x->play != 0 || setup_dword(x->setup, SETUP_RACE_W4FB7D4) != 1) return 0;
    x->once = 1;
    return 1;
}

void race_net_exchange_setup(const RaceNetExchangeOps *ops, RaceNetExchange *x, uint8_t *first, uint8_t *second)
{
    if (x->once != 0) {
        ops->setup_both(ops->ctx, x->setup, first);
        return;
    }
    if (x->unknown_4fbb0c != 0) {
        ops->host_setup(ops->ctx, x->setup, first, second);
        if (exchange_once(x)) return;
        ops->send_setup(ops->ctx, x->setup);
    } else {
        ops->keys(ops->ctx);
        ops->send_info(ops->ctx, first);
        if (exchange_once(x)) return;
        ops->wait_setup(ops->ctx);
    }
    exchange_once(x);
}

static int32_t ping_all_wait(const RaceNetPingAllOps *ops, RaceNetPingAll *p)
{
    while (p->flags_a[RACE_NET_PINGED_FLAG] == 0) {
        if (ops->step(ops->ctx) != 0) {
            p->dropped = 1;
            return -2;
        }
        ops->service(ops->ctx);
    }
    p->flags_a[RACE_NET_PINGED_FLAG] = 0;
    return 1;
}

static int32_t ping_all_count(const RaceNetPingAll *p)
{
    return state_word(p->state, RACE_NET_AT_HUMANS) + state_word(p->state, RACE_NET_AT_OTHERS);
}

/* One round over the slots; 0 once the last one is reached (all linked ones answered), else until */
static int32_t ping_all_round(const RaceNetPingAllOps *ops, const RaceNetPingAll *p, int32_t until)
{
    const RaceNetSend at = { .entries = p->entries };
    for (int16_t slot = 1; slot < ping_all_count(p) && slot < RACE_NET_LINKS; slot++) {  /* port guard */
        ops->service(ops->ctx);
        if (link_word(&at, p->links[slot]) != 0 && p->pings[slot] == 0) break;
        if (slot == ping_all_count(p) - 1) until = 0;
    }
    return until;
}

enum { LATENCY_STEPS = 51 };

int32_t race_net_latency(int32_t ticks)
{
    static const int8_t waits[LATENCY_STEPS] = {  /* 00410020 */
        2, 2, 2, 2, 2, 2, 2, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 6, 6, 6, 6,
        6, 6, 6, 6, 6, 6, 6, 6, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8
    };
    return waits[ticks < 0 ? 0 : ticks >= LATENCY_STEPS ? LATENCY_STEPS - 1 : ticks];
}

int32_t race_net_ping_all(const RaceNetPingAllOps *ops, RaceNetPingAll *p)
{
    if (p->unknown_4fbb0c == 0) {
        ops->service(ops->ctx);
        p->deadline = RACE_NET_NO_DEADLINE;
        return ping_all_wait(ops, p);
    }
    for (int16_t i = 0; i < 2 * state_word(p->state, RACE_NET_AT_HUMANS) && i < RACE_NET_PINGS_CLEARED; i++)  /* port guard */
        p->pings[i] = 0;
    const int32_t start = p->clock;
    uint8_t ping[RACE_NET_PING];
    put_state_word(ping, 0, RACE_NET_PING_KIND), put_state_word(ping, 2, 0);
    ops->send(ops->ctx, 0, ping, RACE_NET_PING);
    int32_t until = x_add(p->clock, RACE_NET_PING_WAIT);
    if (until > p->clock)
        do {
            until = ping_all_round(ops, p, until);
            ops->service(ops->ctx);
        } while (until > p->clock);
    int16_t answered = 0;
    int32_t largest = 0;
    for (int16_t slot = 1; slot < ping_all_count(p) && slot < RACE_NET_LINKS; slot++) {  /* port guard */
        ops->service(ops->ctx);
        if (p->pings[slot] == 0) {
            ops->drop(ops->ctx, slot);
            continue;
        }
        const int32_t half = x_add(p->pings[slot], -start) / 2;
        answered = (int16_t)(answered + 1);
        if (largest <= half) largest = half;
    }
    if (answered == 0) return -1;
    p->unknown_4fbac8 = ops->latency(ops->ctx, largest);
    return largest;
}

void race_net_startup(const RaceNetStartupOps *ops, RaceNetStartup *s)
{
    s->lock = ops->mutex(ops->ctx);
    s->chat_lock = ops->mutex(ops->ctx);
    s->input_lock = ops->mutex(ops->ctx);
    if (s->lock == 0 || s->chat_lock == 0 || s->input_lock == 0) ops->fatal(ops->ctx, RACE_NET_MUTEX_FAILED);
    s->unknown_4c62a4 = RACE_NET_PLAYER_TIME, s->unknown_4c62a8 = RACE_NET_PULSE_TIME;
    s->unknown_4c6300 = RACE_NET_OPEN_TIME, s->unknown_4c6304 = RACE_NET_POOL_TAG;
    s->unknown_4c636c = 0, s->unknown_4c637c = 0, s->unknown_4c6384 = 0;
    ops->tcp(ops->ctx);
    ops->driver(ops->ctx, s->german != 0 ? RACE_NET_DRIVER_NAME_G : RACE_NET_DRIVER_NAME);
    ops->services(ops->ctx);
}

int16_t race_net_any_ready(int16_t play, const uint8_t *settings)
{
    if (play == 0) return 0;
    for (int i = 0; i < RACE_NET_SET_READIES; i++)
        if (state_word(settings, RACE_NET_SET_READY + 2 * i) != 0) return 1;
    return 0;
}
