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

int32_t race_net_queue_push(RaceNetQueue *q, const uint8_t *name, int16_t viewer, const uint8_t *link)
{
    if ((uint32_t)q->tail >= RACE_NET_ASKS) return 0;  /* port guard */
    uint8_t *ask = q->asks[q->tail], *end = (uint8_t *)q->asks + sizeof q->asks;
    ask[RACE_NET_ASK_VIEWER] = (uint8_t)viewer, ask[RACE_NET_ASK_VIEWER + 1] = (uint8_t)((uint16_t)viewer >> 8);
    for (uint8_t *at = ask; at < end; at++)  /* port guard: within the ten asks */
        if ((*at = *name++) == 0) break;
    memcpy(ask + RACE_NET_ASK_NAME, link, RACE_NET_NAME);
    const int32_t next = q->tail + 1;
    q->tail = next % RACE_NET_ASKS;
    q->count = (int32_t)((uint32_t)q->count + 1);
    return next / RACE_NET_ASKS;
}

int32_t race_net_join_request(RaceNetQueue *q, const uint8_t *req)
{
    const int viewer_room = q->counts.unknown_4bc6da < RACE_NET_MAX_VIEWERS, player_room = q->counts.unknown_4bc6d8 < RACE_NET_MAX_PLAYERS;
    const int viewer = req[RACE_NET_REQ_KIND] == 'V' ? viewer_room : !player_room;
    if (viewer ? !viewer_room : !player_room) return 0;
    if (viewer) q->counts.unknown_4bc6da = (int16_t)(q->counts.unknown_4bc6da + 1);
    else q->counts.unknown_4bc6d8 = (int16_t)(q->counts.unknown_4bc6d8 + 1);
    race_net_queue_push(q, req + RACE_NET_REQ_NAME, viewer, req);
    q->unknown_50a5bc = 1;
    return 1;
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

void race_net_drop(const RaceNetDropOps *ops, RaceNetDrop *d, int16_t slot)
{
    if (slot == 0 || d->unknown_4fbb0c == 0) return;
    if (slot < 0 || slot >= RACE_NET_LINKS) return;  /* port guard */
    int32_t kind = -1;
    for (int k = 0; k < RACE_NET_SEATS; k++)
        if (d->links[slot] == entry_at(k)) kind = 1;
    for (int k = 0; k < RACE_NET_VIEWERS; k++)
        if (d->links[slot] == entry_at(RACE_NET_KIND0_FIRST + k)) kind = 0;
    if (d->unknown_4bc6ac == 0 && kind != 0 && d->queues[slot].gave_up != 0) return;
    d->joined = (int32_t)((uint32_t)d->joined - 1);
    d->unknown_50a5bc = 1;
    if (d->unknown_4bc6ac != 0) {
        ops->close(ops->ctx, d->links[slot]);
        if (d->play == 1) ops->forget(ops->ctx, RACE_NET_SESSION, d->links[slot] + RACE_NET_NAME_AT);  /* 00442db8 */
    }
    race_net_count_down(&d->counts, kind);
    if (d->unknown_4bc6ac != 0) return;
    if (kind != 0) {
        d->queues[slot].gave_up = 1;
        d->records[slot].finish = RACE_NET_GONE_FINISH;
        ops->stand_in(ops->ctx, slot);
    }
    if (d->joined == 0) d->dropped = 1;
}

int32_t race_net_link_lost(const RaceNetLostOps *ops, RaceNetLost *l, int32_t kind, uint32_t link)
{
    if (l->host == 0) {
        l->dropped = 1;
        l->queues[0].gave_up = 1;
        l->records[0].finish = RACE_NET_GONE_FINISH;
        return 0;
    }
    ops->lock(ops->ctx, l->lock);
    l->joined = (int32_t)((uint32_t)l->joined - 1);
    l->unknown_50a5bc = 1;
    if (l->unknown_4bc6ac == 0) {
        RaceNetSlots slots = { state_word(l->state, RACE_NET_AT_HUMANS), state_word(l->state, RACE_NET_AT_OTHERS), { 0 } };
        memcpy(slots.links, l->links, sizeof slots.links);
        const int16_t slot = (int16_t)race_net_slot_of(&slots, link);
        if (slot < state_word(l->state, RACE_NET_AT_HUMANS) && slot >= 0) {  /* port guard: -1 */
            l->queues[slot].gave_up = 1;
            l->records[slot].finish = RACE_NET_GONE_FINISH;
            ops->stand_in(ops->ctx, slot);
        }
        if (l->joined == 0) l->dropped = 1;
    }
    for (int k = 0; k < RACE_NET_SEATS; k++)
        if (entry_at(k) == link) kind = 1;
    for (int k = 0; k < RACE_NET_VIEWERS; k++)
        if (entry_at(RACE_NET_KIND0_FIRST + k) == link) kind = 0;
    race_net_count_down(&l->counts, kind);
    ops->unlock(ops->ctx, l->lock);
    return 0;
}

int32_t race_net_host_start(const RaceNetHostOps *ops, RaceNetHost *h, const uint8_t *name)
{
    static const uint8_t no_password[1] = { 0 };  /* the byte at 00442510 */
    h->host = 1;
    h->dropped = 0;
    put_state_word(h->state, RACE_NET_AT_TO, 0);
    put_state_word(h->state, RACE_NET_AT_HUMANS, 1);
    h->counts.unknown_4bc6d8 = 1, h->counts.unknown_4bc6da = 0;
    h->unknown_4fbb0c = 1;
    h->joined = 0;
    put_state_word(h->state, RACE_NET_AT_OTHERS, 0);
    h->queue_head = 0, h->unknown_50a36c = 0, h->queue_count = 0;
    copy_name(h->state, RACE_NET_AT_NAMES, h->me, RACE_NET_STATE);
    h->unknown_50a360 = RACE_NET_SESSION_KIND;
    for (int i = 0; i < RACE_NET_SESSION_NAME; i++)  /* port guard: within 0050a320's 0x40 (the original runs on into 0050a360) */
        if ((h->session_name[i] = name[i]) == 0) break;
    ops->reset(ops->ctx);
    if (ops->create(ops->ctx, RACE_NET_SESSION, h->session_name, no_password, RACE_NET_JOIN_HANDLER) == 0) {
        ops->message(ops->ctx, h->german != 0 ? RACE_NET_CREATE_FAILED_G : RACE_NET_CREATE_FAILED, RACE_NET_FAILED_TICKS);
        ops->shut_down(ops->ctx);
        return 0;
    }
    h->unknown_4bc6ac = 1;
    h->play = 1;
    ops->release(ops->ctx);
    ops->message(ops->ctx, h->german != 0 ? RACE_NET_CREATED_G : RACE_NET_CREATED, RACE_NET_CREATED_TICKS);
    return 1;
}

static uint32_t net_text(const RaceNetConnect *c, uint32_t english, uint32_t german) { return c->german != 0 ? german : english; }

int32_t race_net_connect(const RaceNetConnectOps *ops, RaceNetConnect *c, const uint8_t *address)
{
    static const uint8_t no_password[1] = { 0 };  /* the byte at 00442511 */
    c->host = 0;
    c->unknown_4fbb0c = 0;
    c->dropped = 0;
    if (ops->connect(ops->ctx, RACE_NET_SESSION, address, no_password) <= 0) {
        ops->message(ops->ctx, net_text(c, RACE_NET_CONNECT_FAILED, RACE_NET_CONNECT_FAILED_G), RACE_NET_FAILED_TICKS);
        return 0;
    }
    ops->message(ops->ctx, net_text(c, RACE_NET_CONNECTING, RACE_NET_CONNECTING_G), RACE_NET_MESSAGE_STAYS);
    while (c->connected == 0) {
        if (c->esc != 0) {
            while (c->esc != 0) ops->service(ops->ctx);
            ops->message(ops->ctx, net_text(c, RACE_NET_ABORTED, RACE_NET_ABORTED_G), RACE_NET_FAILED_TICKS);
            return 0;
        }
        ops->service(ops->ctx);
    }
    ops->reset(ops->ctx);
    c->unknown_4bc6dc = 0;
    const RaceNetLink *e = race_net_link_fill(c->entries, RACE_NET_SESSION, c->link_name, 0, 1);
    if (ops->join(ops->ctx, e) == 0) {
        if (c->unknown_4bc6dc != 0) ops->message(ops->ctx, net_text(c, RACE_NET_ABORTED, RACE_NET_ABORTED_G), RACE_NET_ABORTED_TICKS);
        else ops->message(ops->ctx, net_text(c, RACE_NET_LINK_FAILED, RACE_NET_LINK_FAILED_G), RACE_NET_FAILED_TICKS);
        ops->shut_down(ops->ctx);
        return 0;
    }
    ops->deadline(ops->ctx, RACE_NET_NO_DEADLINE);
    while (c->unknown_50a5bc == 0)
        if (ops->wait(ops->ctx) != 0) {
            ops->give_up(ops->ctx);
            ops->message(ops->ctx, net_text(c, RACE_NET_ABORTED, RACE_NET_ABORTED_G), RACE_NET_FAILED_TICKS);
            return 0;
        }
    ops->lock(ops->ctx, c->lock);
    memcpy(c->state, c->incoming, RACE_NET_STATE);
    c->unknown_50a5bc = 0;
    c->followed = state_word(c->state, RACE_NET_AT_TO);
    ops->unlock(ops->ctx, c->lock);
    c->play = 1;
    c->unknown_52b316 = state_word(c->state, RACE_NET_AT_TO) == 0;
    ops->release(ops->ctx);
    ops->message(ops->ctx, net_text(c, RACE_NET_CONNECTED, RACE_NET_CONNECTED_G), RACE_NET_CREATED_TICKS);
    return 1;
}

void race_net_clear_flags(int16_t a[RACE_NET_FLAGS_A], int16_t b[RACE_NET_FLAGS_B])
{
    for (int16_t i = 0; i < RACE_NET_FLAGS_A; i++) a[i] = 0;
    for (int16_t i = 0; i < RACE_NET_FLAGS_B; i++) b[i] = 0;
}

void race_net_reset(const RaceNetResetOps *ops, RaceNetReset *r)
{
    race_net_clear_flags(r->flags_a, r->flags_b);
    ops->transport_reset(ops->ctx);
    r->unknown_4bcb94 = 1;
    memset(r->unknown_4f8138, 0, RACE_NET_WORDS_4F8138 * sizeof *r->unknown_4f8138);
    memset(r->entries, 0, RACE_NET_TABLE_ENTRIES * sizeof *r->entries);
    memset(r->tags, 0, RACE_NET_TABLE_ENTRIES * RACE_NET_TAG);
}

void race_net_release(const RaceNetReleaseOps *ops, RaceNetBuffers *b)
{
    b->unknown_4bc6c4 = 0;
    b->unknown_4bc6cc = 0;
    if (b->buffer_4bc6c6 != 0) {
        ops->free_handle(ops->ctx, b->buffer_4bc6c6);
        b->buffer_4bc6c6 = 0;
    }
    if (b->buffer_4bc6ce != 0) {
        ops->free_handle(ops->ctx, b->buffer_4bc6ce);
        b->buffer_4bc6ce = 0;
    }
}

void race_net_close_session(const RaceNetCloseOps *ops, RaceNetClose *c)
{
    if (c->unknown_4fbb0c == 0 || c->play != 1 || c->unknown_4bc6ac == 0) return;
    ops->close_session(ops->ctx, RACE_NET_SESSION, 0);
    c->unknown_4bc6ac = 0;
}

void race_net_leave(const RaceNetLeaveOps *ops, int16_t play)
{
    if (play == 1) ops->leave(ops->ctx, RACE_NET_SESSION);
}

static void give_up_close(void *ctx, uint32_t session, int32_t how)
{
    const RaceNetGiveUpOps *ops = ctx;
    ops->close_session(ops->ctx, session, how);
}

static void give_up_leave(void *ctx, uint32_t session)
{
    const RaceNetGiveUpOps *ops = ctx;
    ops->leave(ops->ctx, session);
}

void race_net_give_up(const RaceNetGiveUpOps *ops, RaceNetGiveUp *g)
{
    memset(g->stand_ins, 0, RACE_NET_PLAYERS * sizeof *g->stand_ins);
    if (g->unknown_4bcb94 == 0) return;
    const RaceNetCloseOps close = { (void *)ops, give_up_close };
    const RaceNetLeaveOps leave = { (void *)ops, give_up_leave };
    race_net_close_session(&close, &g->close);
    race_net_leave(&leave, g->close.play);
    ops->shut_down(ops->ctx);
    g->unknown_4bcb94 = 0;
    race_net_clear_flags(g->flags_a, g->flags_b);
    g->close.unknown_4bc6ac = 0;
    g->unknown_52b316 = 0;
    g->followed = 0;
    g->dropped = 1;
}
