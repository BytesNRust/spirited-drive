/* The link game over the network (link_game.h): the transport, the
 * entries' hooks, the messages in and out, and the network menus' calls. */
#include "game/frontend/link_game.h"

#include "game/frontend/frontend.h"
#include "game/frontend/link_sync.h"
#include "game/frontend/menu_glue.h"
#include "game/sys/input.h"
#include "game/sys/port_log.h"
#include "game/sys/resource.h"
#include "game/sys/timer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

LinkGame link_game;

enum { LINK_PORT = 0x3a98 };  /* the port both ends use unless told otherwise (15000) */

static LinkGame *const L = &link_game;

/* the GUID the game's sessions carry (004bc6b0): names the game ("EAGAME.<guid>") */
static const uint8_t GAME_GUID[NET_GUID] = { 'T', 'N', 'F', 'S', ' ', 'S', 'E', ' ', 'l', 'i', 'n', 'k', ' ', 'v', '1', 0 };

/* --- the texts 00467214 shows, by their exe address --- */

static const struct { uint32_t at; const char *text; } TEXTS[] = {
    { RACE_NET_CREATED, "Game Created" }, { RACE_NET_CREATED_G, "Spiel erstellt" },
    { RACE_NET_CREATE_FAILED, "Create Game Failed" }, { RACE_NET_CREATE_FAILED_G, "Spiel konnte nicht erstellt werden" },
    { RACE_NET_CONNECT_FAILED, "Failed To Connect" }, { RACE_NET_CONNECT_FAILED_G, "Verbinden fehlgeschlagen" },
    { RACE_NET_CONNECTING, "Connecting to server..." }, { RACE_NET_CONNECTING_G, "Verbinde an Server..." },
    { RACE_NET_ABORTED, "Connection Aborted" }, { RACE_NET_ABORTED_G, "Verbindung abgebrochen" },
    { RACE_NET_LINK_FAILED, "Connection Failed" }, { RACE_NET_LINK_FAILED_G, "Verbindung Keine" },
    { RACE_NET_CONNECTED, "Successfully connected" }, { RACE_NET_CONNECTED_G, "Verbindung hergestellt" },
    { RACE_NET_WAITING_EN, "Waiting for other players..." }, { RACE_NET_WAITING_DE, "Warte auf andere Spieler..." },
    { RACE_NET_START_ABORTED, "Connection Aborted" }, { RACE_NET_START_ABORTED_G, "Verbindung abgebrochen" },
};

const char *link_text(uint32_t at)
{
    for (size_t i = 0; i < sizeof TEXTS / sizeof TEXTS[0]; i++)
        if (TEXTS[i].at == at) return TEXTS[i].text;
    return "";
}

/* --- the clock, the service and the keys --- */

static uint32_t clock_ticks(void *ctx) { return (void)ctx, timer_ticks(); }

void link_service(void)
{
    input_pump();
    net_runtime_service(&L->rt);
}

static void runtime_yield(void *ctx)
{
    (void)ctx;
    link_service();
}

static void runtime_message(void *ctx, const char *text)
{
    (void)ctx;
    port_log("tnfs net: %s\n", text);
}

/* --- the entries and their links (an entry's exe address, 0050a5dc + 0x54 i) --- */

uint32_t link_of(int i) { return RACE_NET_PLAYERS_AT + (uint32_t)i * RACE_NET_ENTRY_SIZE; }

int link_index(uint32_t link)
{
    if (link < RACE_NET_PLAYERS_AT) return -1;
    const uint32_t d = link - RACE_NET_PLAYERS_AT;
    if (d % RACE_NET_ENTRY_SIZE != 0 || d / RACE_NET_ENTRY_SIZE >= RACE_NET_ENTRIES) return -1;
    return (int)(d / RACE_NET_ENTRY_SIZE);
}

static int handle_index(const NetPacketHandle *h) { return (int)(h - L->handles); }

/* each entry's +0 as the transport left its handle's +0 */
void link_entries_sync(void)
{
    for (int i = 0; i < RACE_NET_ENTRIES; i++) L->entries[i].unknown_00 = (uint32_t)L->handles[i].open;
}

/* 0046417c for 00443494: the keys read again (ctx the words it reads) */
static void escape_service(void *ctx)
{
    RaceNetSessionEscape *const e = ctx;
    link_service();
    e->esc = link_esc(), e->unknown_50a2fc = L->session.s.word_9c;
}

/* 00443494, the handle's +0x14 while it opens: 1 gives up */
static int32_t hook_escape(NetPacketHandle *h)
{
    (void)h;
    RaceNetSessionEscape e = { link_esc(), L->session.s.word_9c };
    const RaceNetSessionEscapeOps ops = { &e, escape_service };
    return race_net_session_escape(&ops, &e);
}

/* 004460cc, the handle's +0x18 when the link goes */
static int32_t hook_lost(NetPacketHandle *h)
{
    link_entries_sync();
    return link_lost_link(0, link_of(handle_index(h)));
}

/* 00446058, the handle's +0x1c: a message from that link */
static int32_t hook_deliver(NetPacketHandle *h, const uint8_t *data, uint32_t n)
{
    link_entries_sync();
    return link_receive(link_of(handle_index(h)), data, (int32_t)n);
}

/* 00479a6c(entry, 0x20, 0, 0xfa, 6) on an entry 00442e3c filled */
int32_t link_join(const RaceNetLink *entry)
{
    const int i = (int)(entry - L->entries);
    if (i < 0 || i >= RACE_NET_ENTRIES) return 0;
    NetPacketHandle *const h = &L->handles[i];
    memcpy(L->peers[i], entry->name, NET_ADDRESS);
    *h = (NetPacketHandle){ .word_4 = entry->unknown_04, .ack_time = entry->unknown_08, .resend_time = entry->unknown_0c,
                            .data_time = entry->unknown_10, .poll = hook_escape, .hook_18 = hook_lost,
                            .deliver = hook_deliver, .session = &L->session.s, .address = L->peers[i] };
    const int32_t r = net_runtime_open(&L->rt, h, 0x20, 0, 0xfa, 6);
    link_entries_sync();
    return r;
}

/* 0047b250 on an entry */
void link_close_entry(uint32_t link)
{
    const int i = link_index(link);
    if (i < 0) return;
    net_runtime_close(&L->rt, &L->handles[i]);
    link_entries_sync();
}

/* 004799a8(0050a260, name) */
void link_forget(const uint8_t *name)
{
    net_runtime_kill_player(&L->rt, &L->session.s, name);
}

/* 0047ab80 */
void link_shut_down(void)
{
    net_runtime_shutdown(&L->rt);
    link_entries_sync();
}

/* 0047b17c on an entry: 0 failed */
int32_t link_write(uint32_t link, const uint8_t *data, int32_t size)
{
    const int i = link_index(link);
    if (i < 0 || size < 0) return 0;
    return net_runtime_send(&L->rt, &L->handles[i], data, (uint32_t)size);
}

/* --- the join handler (00442ec0), the session's +0xa8 --- */

static int32_t runtime_accept(void *ctx, int32_t fn, NetSession *s, uint8_t *record)
{
    (void)ctx, (void)s;
    if (fn != RACE_NET_JOIN_HANDLER) return 1;
    return link_join_request(record);
}

/* 004100ec's ops: one thread, so any lock does; the TCP layer and the IPX
 * driver are not the port's (the UDP driver is the runtime's own) */
static uint32_t startup_mutex(void *ctx) { return (void)ctx, 1; }
static void startup_fatal(void *ctx, uint32_t text) { (void)ctx, (void)text; }
static void startup_nothing(void *ctx) { (void)ctx; }
static void startup_driver(void *ctx, uint32_t name) { (void)ctx, (void)name; }
static void startup_services(void *ctx) { (void)ctx, link_service_first(); }

void link_game_init(uint16_t port, const char *const *hosts)
{
    memset(L, 0, sizeof *L);
    net_runtime_init(&L->rt, port != 0 ? port : LINK_PORT, hosts, NULL, clock_ticks);
    L->rt.yield = runtime_yield, L->rt.accept = runtime_accept, L->rt.message = runtime_message;
    memcpy(L->game_guid, GAME_GUID, NET_GUID);
    L->games.services = &L->services;
    L->ready = 1;
    RaceNetStartup st = { 0 };
    st.german = (int16_t)resource_german();
    static const RaceNetStartupOps ops = { NULL, startup_mutex, startup_fatal, startup_nothing, startup_driver,
                                           startup_services };
    race_net_startup(&ops, &st);
    L->rt.now = st.unknown_4c62a4, L->rt.stamp = st.unknown_4c62a8, L->rt.open_state = st.unknown_4c6300;
}

/* --- the menus' lists --- */

static void block_free(void *ctx, uint8_t *p) { (void)ctx, free(p); }

static uint8_t *block_alloc(void *ctx, const char *name, uint32_t size, uint32_t flags)
{
    (void)ctx, (void)name, (void)flags;
    return calloc(1, size);
}

static int32_t services(void *ctx, const NetServicesCallback *cb, int32_t a, int32_t b, uint32_t most)
{
    (void)ctx, (void)a, (void)b, (void)most;
    return net_runtime_get_services(&L->rt, cb);
}

static void kill_service(void *ctx)
{
    (void)ctx;
    net_runtime_kill_service(&L->rt, &L->session.s);
}

static void banner_wait(void *ctx, const char *text, int32_t time) { glue_banner_wait(ctx, text, time); }

static const RaceNetServicesOps SERVICES = { NULL, block_free, block_alloc, services, kill_service, banner_wait };

int32_t link_services_found(void) { return L->ready && link_service_first() != NULL; }

const char *link_service_first(void)
{
    return (const char *)race_net_services_first(&SERVICES, &L->services, (int16_t)resource_german());
}

const char *link_service_next(void)
{
    return (const char *)race_net_services_next(&SERVICES, &L->services, (int16_t)resource_german());
}

static int32_t init_service(void *ctx, const uint8_t *service, const char *text)
{
    (void)ctx, (void)service;  /* one driver: UDP */
    return net_runtime_init_service(&L->rt, &L->session, L->game_guid, text, frontend_player_name);
}

static void fatal(void *ctx, const char *text) { runtime_message(ctx, text); }
static void keys_on(void *ctx) { (void)ctx; }
static void keys_off(void *ctx) { (void)ctx; }

static int32_t games(void *ctx, const NetGamesCallback *cb, int32_t timeout)
{
    (void)ctx;
    return net_runtime_get_games(&L->rt, &L->session.s, cb, timeout);
}

static int32_t same(void *ctx, NetSession *s, const uint8_t *a, const uint8_t *b)
{
    (void)ctx;
    return net_address_equal(&L->rt.equal, s, a, b);
}

RaceNetGames *link_games(int32_t timeout)
{
    static const RaceNetGamesOps ops = { NULL, kill_service, init_service, fatal, block_alloc, keys_on, keys_off,
                                         games, same };
    L->games.view = L->view;
    RaceNetGames *const g = race_net_games_list(&ops, &L->games, timeout);
    L->view = L->games.view;
    return g;
}

static int32_t players(void *ctx, const uint8_t *address, const NetPlayerCallback *cb)
{
    (void)ctx;
    return net_runtime_get_players(&L->rt, &L->session.s, address, cb);
}

RaceNetPlayers *link_players(const uint8_t *address)
{
    static const RaceNetPlayersOps ops = { NULL, block_alloc, keys_on, keys_off, players };
    int16_t key = 0;
    return race_net_players_get(&ops, &L->players, &key, address);
}
