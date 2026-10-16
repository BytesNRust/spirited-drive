/* The race's side of the network game (race_net.h). */
#include "game/race/race_net.h"

#include <string.h>

typedef char race_net_link_size_check[sizeof(RaceNetLink) == 0x54 ? 1 : -1];

int32_t race_net_wait_step(const RaceNetOps *ops, const RaceNetWords *w)
{
    ops->keys(ops->ctx);
    if (w->dropped != 0) return 1;
    if (w->deadline != RACE_NET_NO_DEADLINE && w->deadline < w->clock) return 1;
    ops->service(ops->ctx);
    return 0;
}

void race_net_count_down(RaceNetCounts *c, int32_t kind)
{
    if (kind != 0) c->unknown_4bc6d8 = (int16_t)(c->unknown_4bc6d8 - 1);
    else c->unknown_4bc6da = (int16_t)(c->unknown_4bc6da - 1);
}

int32_t race_net_slot_of(const RaceNetSlots *s, uint32_t link)
{
    const int32_t n = (int32_t)s->unknown_4f81d2 + s->unknown_4f81d4;
    for (int32_t i = 1; i < n && i < RACE_NET_LINKS; i++)  /* port guard: the original reads on past 8 */
        if (s->links[i] == link) return i;
    return RACE_NET_NO_SLOT;
}

RaceNetLink *race_net_link_fill(RaceNetLink entries[RACE_NET_ENTRIES], uint32_t owner, const uint8_t *name,
                                int16_t slot, int16_t kind)
{
    const int32_t at = kind != 0 ? slot : RACE_NET_KIND0_FIRST + slot;
    if (at < 0 || at >= RACE_NET_ENTRIES) return NULL;  /* port guard */
    RaceNetLink *e = &entries[at];
    e->unknown_04 = 1, e->unknown_08 = 0x19, e->unknown_0c = 0xf, e->unknown_10 = 10;
    e->handler_14 = RACE_NET_HANDLER_14, e->handler_lost = RACE_NET_HANDLER_LOST;
    e->owner = owner;
    e->handler_1c = RACE_NET_HANDLER_1C;
    memcpy(e->name, name, RACE_NET_NAME);
    return e;
}

void race_net_forget(const RaceNetOps *ops, int16_t play, uint32_t entry)
{
    if (play == 1) ops->forget(ops->ctx, RACE_NET_SESSION, entry + RACE_NET_NAME_AT);
}

static int16_t state_word(const uint8_t *state, int at) { return (int16_t)(uint16_t)(state[at] | state[at + 1] << 8); }
static void put_state_word(uint8_t *state, int at, int16_t v) { state[at] = (uint8_t)v, state[at + 1] = (uint8_t)((uint16_t)v >> 8); }

static void send_state(const RaceNetPollOps *ops, const RaceNetPoll *p, int16_t slot)
{
    const uint32_t link = (uint32_t)slot < RACE_NET_LINKS ? p->links[slot] : 0;  /* port guard: the original reads on */
    ops->send(ops->ctx, link, p->state, RACE_NET_STATE);
    ops->service(ops->ctx);
}

void race_net_poll(const RaceNetPollOps *ops, const RaceNetPoll *p)
{
    if (p->host == 0) return;
    ops->update(ops->ctx);
    ops->lock(ops->ctx, p->lock);
    ops->handle(ops->ctx);
    ops->unlock(ops->ctx, p->lock);
    ops->service(ops->ctx);
    put_state_word(p->state, 0, RACE_NET_STATE_KIND);
    for (int16_t slot = 1; slot < state_word(p->state, RACE_NET_AT_HUMANS); slot++) {
        put_state_word(p->state, RACE_NET_AT_TO, slot);
        send_state(ops, p, slot);
    }
    put_state_word(p->state, RACE_NET_AT_TO, 0);
    for (int16_t slot = state_word(p->state, RACE_NET_AT_HUMANS);
         slot < (int32_t)state_word(p->state, RACE_NET_AT_HUMANS) + state_word(p->state, RACE_NET_AT_OTHERS); slot++)
        send_state(ops, p, slot);
}

/* The ask's name string, up to its NUL (port guard: within the ask's 31 bytes; the original reads on) */
static void copy_tag(uint8_t *tag, const uint8_t *ask)
{
    for (int i = 0; i < RACE_NET_ASK; i++)
        if ((tag[i] = ask[i]) == 0) return;
}

void race_net_add_new_players(const RaceNetJoinOps *ops, RaceNetQueue *q)
{
    ops->service(ops->ctx);
    while (q->count != 0) {
        uint8_t ask[RACE_NET_ASK];
        memcpy(ask, q->asks[(uint32_t)q->head < RACE_NET_ASKS ? q->head : 0], RACE_NET_ASK);  /* port guard */
        ops->lock(ops->ctx, q->lock);
        q->head = (int32_t)((uint32_t)q->head + 1) % RACE_NET_ASKS;
        q->count = (int32_t)((uint32_t)q->count - 1);
        ops->unlock(ops->ctx, q->lock);
        const int16_t player = (ask[RACE_NET_ASK_VIEWER] | ask[RACE_NET_ASK_VIEWER + 1]) == 0;
        const int first = player ? 0 : RACE_NET_KIND0_FIRST, n = player ? RACE_NET_PLAYERS : RACE_NET_VIEWERS;
        int16_t slot = 0;
        while (slot < n && q->entries[first + slot].unknown_00 != 0) slot++;
        if (slot == n) {
            ops->abort(ops->ctx, player ? RACE_NET_PLAYER_OVERFLOW : RACE_NET_VIEWER_OVERFLOW);
            return;  /* the original has ended the process */
        }
        copy_tag(q->tags + (first + slot) * RACE_NET_TAG, ask);
        ops->service(ops->ctx);
        const RaceNetLink *e = race_net_link_fill(q->entries, RACE_NET_SESSION, ask + RACE_NET_ASK_NAME, slot, player);
        if (ops->join(ops->ctx, e) != 0) {
            q->joined = (int32_t)((uint32_t)q->joined + 1);
            continue;
        }
        if (q->play == 1) ops->forget(ops->ctx, RACE_NET_SESSION, e->name);
        race_net_count_down(&q->counts, player);
    }
    q->unknown_50a5bc = 0;
}

/* A string copied up to its NUL into the state block (port guard: within the block and `room` source bytes) */
static void copy_name(uint8_t *state, int at, const uint8_t *from, int room)
{
    for (int i = 0; i < room && at + i >= 0 && at + i < RACE_NET_STATE; i++)
        if ((state[at + i] = from[i]) == 0) return;
}

static uint32_t entry_at(int k) { return RACE_NET_PLAYERS_AT + (uint32_t)k * RACE_NET_ENTRY_SIZE; }

/* 0044659c on player entry k, the slots read from the state block */
static int16_t seat_of(const RaceNetSeats *s, int k)
{
    RaceNetSlots slots = { state_word(s->state, RACE_NET_AT_HUMANS), state_word(s->state, RACE_NET_AT_OTHERS), { 0 } };
    memcpy(slots.links, s->links, sizeof slots.links);
    return (int16_t)race_net_slot_of(&slots, entry_at(k));
}

/* An entry seated at links[at] with its tag in name slot `name`, the count at `count` + 1; or one gone, closed */
static void seat_entry(const RaceNetSeatOps *ops, RaceNetSeats *s, int k, int32_t at, int32_t name, int count)
{
    const RaceNetLink *e = &s->entries[k];
    if (e->unknown_00 == 0) {
        if (e->unknown_20 == 0) return;
        ops->close(ops->ctx, e);
        if (s->play == 1) ops->forget(ops->ctx, RACE_NET_SESSION, e->name);  /* 00442db8 */
        return;
    }
    if (at >= 0 && at < RACE_NET_LINK_ROOM) s->links[at] = entry_at(k);  /* port guard */
    const int from = k * RACE_NET_TAG;
    copy_name(s->state, RACE_NET_AT_NAMES + name * RACE_NET_TAG, s->tags + from, RACE_NET_TAGS - from);
    put_state_word(s->state, count, (int16_t)(state_word(s->state, count) + 1));
}

static void swap_words(int16_t *w, int a, int b)
{
    const int16_t t = w[a];
    w[a] = w[b], w[b] = t;
}

void race_net_seat(const RaceNetSeatOps *ops, RaceNetSeats *s)
{
    if (s->host == 0 || s->play == 0) return;
    int16_t before[RACE_NET_SEATS] = { 0 }, after[RACE_NET_SEATS] = { 0 };
    for (int k = 0; k < RACE_NET_SEATS && s->unknown_52b310 != 0; k++) before[k] = seat_of(s, k);
    put_state_word(s->state, RACE_NET_AT_TO, 0);
    put_state_word(s->state, RACE_NET_AT_HUMANS, 1);
    copy_name(s->state, RACE_NET_AT_NAMES, s->me, RACE_NET_STATE);
    for (int k = 0; k < RACE_NET_SEATS; k++) {
        const int16_t h = state_word(s->state, RACE_NET_AT_HUMANS);
        seat_entry(ops, s, k, h, h, RACE_NET_AT_HUMANS);
    }
    for (int k = 0; k < RACE_NET_SEATS && s->unknown_52b310 != 0; k++) after[k] = seat_of(s, k);
    if (s->unknown_52b310 != 0)
        for (int k = 0; k < RACE_NET_SEATS; k++) {
            const int a = after[k], b = before[k];
            if (a == RACE_NET_NO_SLOT || a == b) continue;
            if (a < -1 || a >= RACE_NET_LINKS || b < -1 || b >= RACE_NET_LINKS) continue;  /* port guard */
            swap_words(s->unknown_52b2b4, a, b);
            swap_words(s->unknown_524e8d, a, b);
        }
    put_state_word(s->state, RACE_NET_AT_OTHERS, 0);
    for (int k = 0; k < RACE_NET_VIEWERS; k++) {
        const int16_t h = state_word(s->state, RACE_NET_AT_HUMANS), o = state_word(s->state, RACE_NET_AT_OTHERS);
        seat_entry(ops, s, RACE_NET_KIND0_FIRST + k, (int32_t)h + o, RACE_NET_VIEWER_NAMES + o, RACE_NET_AT_OTHERS);
    }
}

/* 0044659c's search on the counts and links as they are now */
static int16_t slot_now(const RaceNetSend *s, uint32_t link)
{
    RaceNetSlots slots = { state_word(s->state, RACE_NET_AT_HUMANS), state_word(s->state, RACE_NET_AT_OTHERS), { 0 } };
    memcpy(slots.links, s->links, sizeof slots.links);
    return (int16_t)race_net_slot_of(&slots, link);
}

/* The word at an entry's exe address (port guard: 0 outside the eleven entries) */
static uint32_t link_word(const RaceNetSend *s, uint32_t link)
{
    const uint32_t at = link - RACE_NET_PLAYERS_AT, room = (RACE_NET_KIND0_FIRST + RACE_NET_VIEWERS) * RACE_NET_ENTRY_SIZE;
    if (link < RACE_NET_PLAYERS_AT || at > room - 4) return 0;
    const uint8_t *b = (const uint8_t *)s->entries + at;
    return (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24;
}

void race_net_send(const RaceNetSendOps *ops, RaceNetSend *s, uint32_t link, const uint8_t *data, int32_t size)
{
    if (s->dropped != 0) return;
    if (link != 0) {
        if (link_word(s, link) != 0 && ops->send(ops->ctx, link, data, size) != 0) return;
        if (s->unknown_4fbb0c == 0 || s->play != 1) s->dropped = 1;
        else ops->drop(ops->ctx, slot_now(s, link));
        return;
    }
    if (s->host == 0) {
        if (s->entries[0].unknown_00 != 0 && ops->send(ops->ctx, entry_at(0), data, size) == 0) s->dropped = 1;
        return;
    }
    for (int k = 0; k < RACE_NET_KIND0_FIRST + RACE_NET_VIEWERS; k++)
        if (s->entries[k].unknown_00 != 0 && ops->send(ops->ctx, entry_at(k), data, size) == 0)
            ops->drop(ops->ctx, slot_now(s, entry_at(k)));
}
