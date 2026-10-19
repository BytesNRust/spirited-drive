/* A link game's race (link_race.h): the drive's link from the link game's
 * words, its sends, waits and slots through the race_net glue. */
#include "game/frontend/link_race.h"

#include "game/frontend/link_central.h"
#include "game/frontend/link_game.h"
#include "game/frontend/link_sync.h"
#include "game/frontend/menu_glue.h"
#include "game/frontend/tournament.h"
#include "game/viewer/track_viewer.h"

#include <string.h>

static LinkGame *const L = &link_game;

/* 00445e00: the words little-endian, as the original's memory holds them */
static void race_send(int32_t to, const int16_t *words, int32_t size)
{
    uint8_t data[RACE_NET_STATE];
    if (size < 0 || size > (int32_t)sizeof data) return;
    for (int32_t i = 0; i + 1 < size; i += 2)
        data[i] = (uint8_t)words[i / 2], data[i + 1] = (uint8_t)((uint16_t)words[i / 2] >> 8);
    if (size & 1) data[size - 1] = (uint8_t)words[size / 2];
    link_send((uint32_t)to, data, size);
}

static void (*chat_screen)(void *ctx);
static void chat(void) { chat_screen(NULL); }

/* The race's queues, records and 0041019c for the link's handlers while it runs */
static void racing(InputQueue *queues, PlayerRecord *records, void (*stand_in)(int16_t player))
{
    L->queues = queues, L->records = records, L->stand_in = stand_in;
}

/* 00410c18 on the link game's words */
static int32_t race_wait(int32_t stage) { return link_wait((int16_t)stage); }

enum { SETUP_NET_GAME = 0x18 };  /* 004fb7d4 in the setup 004fb7bc */

static char own_name[sizeof ((RaceEntry *)0)->name];
static uint8_t spare_names[GAME_STATE_SIZE];  /* 0052ad44 when the drive keeps none */
static uint8_t *names_at;

static int32_t setup_dword(int at)
{
    const uint8_t *s = L->setup + at;
    return (int32_t)((uint32_t)s[0] | (uint32_t)s[1] << 8 | (uint32_t)s[2] << 16 | (uint32_t)s[3] << 24);
}
static void put_setup_dword(int at, int32_t v)
{
    for (int k = 0; k < 4; k++) L->setup[at + k] = (uint8_t)((uint32_t)v >> 8 * k);
}

static void ex_service(void *ctx) { (void)ctx, link_service(); }
static void ex_send(void *ctx, uint32_t to, const uint8_t *data, int32_t size) { (void)ctx, link_send(to, data, size); }
static void ex_deadline(void *ctx, int32_t ticks) { (void)ctx, link_deadline(ticks); }
static void ex_drop(void *ctx, int16_t slot) { (void)ctx, link_drop(slot); }
static void ex_names_done(void *ctx, int32_t humans) { (void)ctx, tournament_default_names(names_at, humans); }
static int32_t ex_step(void *ctx) { (void)ctx; return link_wait_step(); }
static void ex_setup_both(void *ctx, uint8_t *setup, const uint8_t *record) { (void)ctx, race_net_setup_both(setup, record); }
static void ex_keys(void *ctx) { (void)ctx; }  /* 0046417c, the menus' music: the drive plays its own */

/* 00410bac, then 00439ea0's words read again */
static int32_t wait_step(void *ctx)
{
    RaceNetSetupWait *w = ctx;
    const int32_t answered = link_wait_step();
    w->play = menu_words.link_game, w->dropped = menu_words.link_lost, w->humans = setup_dword(RACE_NET_SETUP_HUMANS);
    return answered;
}

static void ex_host_setup(void *ctx, uint8_t *setup, const uint8_t *first, const uint8_t *second)
{
    (void)ctx;
    static const RaceNetSetupHostOps ops = { NULL, ex_service, ex_deadline, ex_step, ex_drop, ex_names_done };
    RaceNetSetupHost h = { setup, L->state, L->links, L->entries, L->flags_b, names_at };
    race_net_host_setup(&ops, &h, first, second);
}

static void ex_send_setup(void *ctx, uint8_t *setup)
{
    (void)ctx;
    static const RaceNetSetupSendOps ops = { NULL, ex_service, ex_send };
    race_net_send_setup(&ops, setup, L->state);
}

static void ex_send_info(void *ctx, uint8_t *record)
{
    (void)ctx;
    static const RaceNetInfoSendOps ops = { NULL, ex_send };
    race_net_send_info(&ops, menu_words.link_game, link_view_only(), record);
}

static void ex_wait_setup(void *ctx)
{
    (void)ctx;
    RaceNetSetupWait w = { menu_words.link_game, menu_words.link_lost, L->flags_a, setup_dword(RACE_NET_SETUP_HUMANS),
                           L->setup + RACE_NET_SETUP_RECORD_A, names_at };
    const RaceNetSetupWaitOps ops = { &w, ex_deadline, wait_step, ex_names_done };
    race_net_wait_setup(&ops, &w);
    menu_words.link_lost = w.dropped;
}

/* 00410d80's words read again after each service and step */
static void ping_service(void *ctx)
{
    RaceNetPingAll *p = ctx;
    link_service();
    p->clock = link_clock();
}

static int32_t ping_step(void *ctx)
{
    RaceNetPingAll *p = ctx;
    L->deadline = p->deadline;
    const int32_t answered = link_wait_step();
    p->clock = link_clock(), p->deadline = L->deadline, p->dropped = menu_words.link_lost;
    return answered;
}

static int32_t ping_latency(void *ctx, int32_t ticks) { (void)ctx; return race_net_latency(ticks); }

/* 00410d80: the host times every machine's answer, a client answers; 004fbac8 (00411160 of the slowest) into
 * *wait_ticks when one answered */
static int32_t ping_all(int32_t *wait_ticks)
{
    RaceNetPingAll p = { menu_words.intro_seen, L->state, L->links, L->entries, L->pings, link_clock(), L->deadline,
                         L->flags_a, menu_words.link_lost, *wait_ticks };
    const RaceNetPingAllOps ops = { &p, ping_service, ex_send, ping_step, ex_drop, ping_latency };
    const int32_t slowest = race_net_ping_all(&ops, &p);
    L->deadline = p.deadline, menu_words.link_lost = p.dropped, *wait_ticks = p.unknown_4fbac8;
    return slowest;
}

/* 004275f8's link start: 00410b7c(-1) and 00410c18(2), then 00410d80 when all came (else 00513e58 1: the setup
 * as a solo race's), then 004273d0 with 0042702c's record (the kind word, then the entry with the player's name);
 * the host's setup says it is a link game's (004fb7d4 1) with the link's humans (004fb7e0, 004f81d2) as
 * race_from_menu sets them */
static const uint8_t *exchange(const RaceEntry *own, uint8_t *names, int32_t *wait_ticks)
{
    static const RaceNetExchangeOps ops = { NULL, ex_setup_both, ex_host_setup, ex_send_setup, ex_keys, ex_send_info, ex_wait_setup };
    link_deadline(RACE_NET_NO_DEADLINE);
    const int32_t once = link_wait(2) == 0;
    if (!once) ping_all(wait_ticks);  /* its answer (004fa0f0) is read by none of the race's ported code */
    uint8_t record[RACE_NET_INFO] = { 0 };
    RaceEntry entry = *own;
    memcpy(entry.name, own_name, sizeof entry.name);
    memcpy(record + 2, &entry, RACE_NET_INFO - 2);
    names_at = names != NULL ? names : spare_names;
    if (menu_words.intro_seen != 0) put_setup_dword(SETUP_NET_GAME, 1), put_setup_dword(RACE_NET_SETUP_HUMANS, link_humans());
    RaceNetExchange x = { once, menu_words.intro_seen, menu_words.link_game, L->setup };
    race_net_exchange_setup(&ops, &x, record, record);
    return L->setup + RACE_NET_SETUP_RECORD_A + 2;
}

void link_race_begin(const char *name, void (*chat_with)(void *ctx))
{
    chat_screen = chat_with;
    memset(own_name, 0, sizeof own_name);
    for (size_t i = 0; i + 1 < sizeof own_name && name[i] != 0; i++) own_name[i] = name[i];
    TrackViewerLink l = { link_humans(), menu_words.single, menu_words.intro_seen, exchange, race_send, link_service,
                          race_wait, link_slot_of, link_sync, link_host_ready, link_deadline, link_any_ready, chat,
                          &L->flags_a[RACE_NET_STAGE_FLAG], &menu_words.link_lost, L->stand_ins, link_drop, racing };
    if (l.humans > TRACK_VIEWER_RACERS) l.humans = TRACK_VIEWER_RACERS;
    track_viewer_drive_link(&l);
    L->race_message = track_viewer_drive_message;
}

void link_race_end(void) { L->race_message = NULL; }
