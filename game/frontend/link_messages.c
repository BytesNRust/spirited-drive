/* The link game's messages (link_sync.h): in through 00446058's handlers,
 * out through 00445e00, and the links that drop, go or join. */
#include "game/frontend/link_sync.h"

#include "game/frontend/menu_glue.h"

#include <stdio.h>

static LinkGame *const L = &link_game;

static InputQueue spare_queues[RACE_PLAYERS];
static PlayerRecord spare_records[RACE_PLAYERS];

static InputQueue *queues(void) { return L->queues != NULL ? L->queues : spare_queues; }
static PlayerRecord *records(void) { return L->records != NULL ? L->records : spare_records; }

int16_t link_word(const uint8_t *at) { return (int16_t)(at[0] | at[1] << 8); }

/* the locks (004f815c, 004f824c, ...): one thread, nothing to hold */
static void lock(void *ctx, uint32_t lock) { (void)ctx, (void)lock; }

int16_t link_slot_of(uint32_t link)
{
    RaceNetSlots s = { link_word(L->state + RACE_NET_AT_HUMANS), link_word(L->state + RACE_NET_AT_OTHERS), { 0 } };
    for (int i = 0; i < RACE_NET_LINKS; i++) s.links[i] = L->links[i];
    return (int16_t)race_net_slot_of(&s, link);
}

static int16_t slot_of(void *ctx, uint32_t link) { return (void)ctx, link_slot_of(link); }

static void stand_in(void *ctx, int16_t player)
{
    (void)ctx;
    if (L->stand_in != NULL) L->stand_in(player);
}

/* --- out: 00445e00 and 004463ec --- */

static int32_t write(void *ctx, uint32_t link, const uint8_t *data, int32_t size)
{
    (void)ctx;
    return link_write(link, data, size);
}

static void drop(void *ctx, int16_t slot) { (void)ctx, link_drop(slot); }

void link_send(uint32_t link, const uint8_t *data, int32_t size)
{
    link_entries_sync();
    RaceNetSend s = { menu_words.mode_menu, menu_words.link_game, menu_words.link_lost, menu_words.intro_seen,
                      L->state, L->links, L->entries };
    static const RaceNetSendOps ops = { NULL, write, drop };
    race_net_send(&ops, &s, link, data, size);
    if (s.dropped != menu_words.link_lost) menu_words.link_lost = s.dropped;
}

static void send(void *ctx, uint32_t link, const uint8_t *data, int32_t size)
{
    (void)ctx;
    link_send(link, data, size);
}

static void drop_out(const RaceNetDrop *d)
{
    L->unknown_4bc6ac = d->unknown_4bc6ac, menu_words.link_game = d->play, menu_words.link_lost = d->dropped;
    L->joined = d->joined, L->flags_a[8] = d->unknown_50a5bc, L->counts = d->counts;
}

static void drop_in(RaceNetDrop *d)
{
    *d = (RaceNetDrop){ menu_words.intro_seen, L->unknown_4bc6ac, menu_words.link_game, menu_words.link_lost, L->joined,
                        L->flags_a[8], L->counts, L->links, queues(), records() };
}

/* 004463ec's ops (ctx its words): the globals current while they run */
static void drop_close(void *ctx, uint32_t link)
{
    drop_out(ctx);
    link_close_entry(link);
    drop_in(ctx);
}

static void drop_forget(void *ctx, uint32_t session, uint32_t name)
{
    (void)session;
    const int i = link_index(name - RACE_NET_NAME_AT);
    drop_out(ctx);
    if (i >= 0) link_forget(L->entries[i].name);
    drop_in(ctx);
}

static void drop_stand_in(void *ctx, int16_t player)
{
    drop_out(ctx);
    stand_in(NULL, player);
    drop_in(ctx);
}

void link_drop(int16_t slot)
{
    link_entries_sync();
    RaceNetDrop d;
    drop_in(&d);
    const RaceNetDropOps ops = { &d, drop_close, drop_forget, drop_stand_in };
    race_net_drop(&ops, &d, slot);
    drop_out(&d);
}

/* --- in: 00446058 and its handlers (004bcb98) --- */

void link_chat_in(uint32_t link, const uint8_t *data)
{
    RaceNetChatIn c = { menu_words.mode_menu, 0, &L->chat[0][0], L->chat_line, L->chat_arrived };
    static const RaceNetChatOps ops = { NULL, send, lock, lock };
    race_net_on_chat(&ops, &c, link, data);
    L->chat_line = c.line, L->chat_arrived = c.unknown_4b9062;
}

static void on_settings(uint32_t link, const uint8_t *data)
{
    RaceNetSettingsIn s = { 0, menu_words.mode_menu, L->settings, L->unknown_4f8138, L->flags_a[7] };
    static const RaceNetSettingsInOps ops = { NULL, lock, lock, slot_of };
    race_net_on_settings(&ops, &s, link, data);
    L->flags_a[7] = s.unknown_50a5ba;
}

static void on_state(const uint8_t *data)
{
    RaceNetStateIn s = { 0, L->incoming, L->flags_a[8] };
    static const RaceNetLockOps ops = { NULL, lock, lock };
    race_net_on_state(&ops, &s, data);
    L->flags_a[8] = s.unknown_50a5bc;
}

static void on_ping(uint32_t link, const uint8_t *data)
{
    RaceNetPingIn p = { L->flags_a, link_clock(), L->pings };
    static const RaceNetPingOps ops = { NULL, send, slot_of };
    race_net_on_ping(&ops, &p, link, data);
}

static void on_stand_ins(uint32_t link, const uint8_t *data)
{
    RaceNetStandIns s = { menu_words.intro_seen, (uint8_t *)L->stand_ins, L->racers, records() };
    static const RaceNetStandInOps ops = { NULL, slot_of, stand_in };
    race_net_on_stand_ins(&ops, &s, link, data);
}

static void handle(void *ctx, int16_t kind, uint32_t link, const uint8_t *data, int32_t size)
{
    (void)ctx;
    static const RaceNetSlotOps slots = { NULL, slot_of };
    switch (kind) {
    case 0: {
        RaceNetInfoIn in = { L->flags_b, L->infos };
        race_net_on_info(&slots, &in, link, data);
        break;
    }
    case 1: {
        RaceNetSetupIn in = { L->flags_a, L->state, L->setup };
        race_net_on_setup(&in, data, size);
        break;
    }
    case 2:
    case 3:
        if (L->race_message != NULL) L->race_message(kind, link, data, size);
        break;
    case 4: on_ping(link, data); break;
    case 5: {
        RaceNetStageIn in = { menu_words.mode_menu, L->flags_a, L->flags_b };
        race_net_on_stage(&slots, &in, link, data);
        break;
    }
    case 6: link_chat_in(link, data); break;
    case 7: on_settings(link, data); break;
    case 8: on_state(data); break;
    case 9: break;  /* 0045156c: the serial / modem link's hello, not sent over the network */
    case 10: on_stand_ins(link, data); break;
    }
}

static void bug(void *ctx, uint32_t text, int32_t kind, uint32_t link)
{
    (void)ctx, (void)text;
    fprintf(stderr, "tnfs net: BUG: Unknow Packet Index Received <%d> pc<%x>\n", (int)kind, (unsigned)link);
}

int32_t link_receive(uint32_t link, const uint8_t *data, int32_t size)
{
    static const RaceNetReceiveOps ops = { NULL, handle, bug };
    return race_net_receive(&ops, menu_words.link_lost, link, data, size);
}

/* --- links gone and joining --- */

static void lost_in(RaceNetLost *l)
{
    *l = (RaceNetLost){ menu_words.mode_menu, menu_words.link_lost, 0, L->joined, L->flags_a[8], L->unknown_4bc6ac,
                        L->counts, L->state, L->links, queues(), records() };
}

static void lost_out(const RaceNetLost *l)
{
    menu_words.link_lost = l->dropped, L->joined = l->joined, L->flags_a[8] = l->unknown_50a5bc, L->counts = l->counts;
}

static void lost_stand_in(void *ctx, int16_t player)
{
    lost_out(ctx);
    stand_in(NULL, player);
    lost_in(ctx);
}

int32_t link_lost_link(int32_t kind, uint32_t link)
{
    RaceNetLost l;
    lost_in(&l);
    const RaceNetLostOps ops = { &l, lock, lock, lost_stand_in };
    const int32_t r = race_net_link_lost(&ops, &l, kind, link);
    lost_out(&l);
    return r;
}

/* the queue's words past its asks, from the globals and back */
static void queue_in(void)
{
    RaceNetQueue *const q = &L->queue;
    q->lock = 0, q->play = menu_words.link_game, q->joined = L->joined, q->unknown_50a5bc = L->flags_a[8];
    q->counts = L->counts, q->entries = L->entries, q->tags = L->tags;
}

static void queue_out(void)
{
    const RaceNetQueue *const q = &L->queue;
    L->joined = q->joined, L->flags_a[8] = q->unknown_50a5bc, L->counts = q->counts;
}

int32_t link_join_request(const uint8_t *record)
{
    queue_in();
    const int32_t r = race_net_join_request(&L->queue, record);
    queue_out();
    return r;
}

/* 00442f58's ops: the globals current while they run (a message may come in) */
static void add_service(void *ctx)
{
    (void)ctx;
    queue_out();
    link_service();
    queue_in();
}

static void abort_message(void *ctx, uint32_t message)
{
    (void)ctx;
    fprintf(stderr, "tnfs net: %s\n", message == RACE_NET_PLAYER_OVERFLOW ? "Player Overflow" : "Viewer Overflow");
}

static int32_t add_join(void *ctx, const RaceNetLink *entry)
{
    (void)ctx;
    queue_out();
    const int32_t r = link_join(entry);
    queue_in();
    return r;
}

static void add_forget(void *ctx, uint32_t session, const uint8_t *name)
{
    (void)ctx, (void)session;
    queue_out();
    link_forget(name);
    queue_in();
}

/* 00442f58 */
void link_add_new_players(void)
{
    static const RaceNetJoinOps ops = { NULL, add_service, lock, lock, abort_message, add_join, add_forget };
    queue_in();
    race_net_add_new_players(&ops, &L->queue);
    queue_out();
}
