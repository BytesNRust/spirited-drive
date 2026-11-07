/* The link game's session (link_game.h): hosting, joining, giving up, and
 * the words they reset. */
#include "game/frontend/link_sync.h"

#include "game/frontend/frontend.h"
#include "game/frontend/menu_glue.h"
#include "game/platform/platform.h"
#include "game/sys/port_log.h"
#include "game/sys/resource.h"
#include "game/sys/timer.h"

#include <stdlib.h>
#include <string.h>

static LinkGame *const L = &link_game;

enum { SC_ESCAPE = 1 };

uint8_t link_esc(void) { return platform_key_down(SC_ESCAPE) ? 1 : 0; }

int32_t link_clock(void) { return (int32_t)timer_ticks(); }

void link_message(uint32_t text, int32_t ticks) { glue_banner_wait(NULL, link_text(text), ticks); }

static void message(void *ctx, uint32_t text, int32_t ticks) { (void)ctx, link_message(text, ticks); }

static void lock(void *ctx, uint32_t lock) { (void)ctx, (void)lock; }

/* --- 0044655c, 00442dec, 00410b7c, 00410bac --- */

static void transport_reset(void *ctx)
{
    (void)ctx;
    if (L->rt.packet_started == 0) net_packet_start(&L->rt.start);
}

void link_reset(void)
{
    RaceNetReset r = { L->flags_a, L->flags_b, L->unknown_4bcb94, L->unknown_4f8138, L->entries, L->tags };
    static const RaceNetResetOps ops = { NULL, transport_reset };
    race_net_reset(&ops, &r);
    L->unknown_4bcb94 = r.unknown_4bcb94;
    for (int i = 0; i < RACE_NET_TABLE_ENTRIES; i++)
        if (L->handles[i].conn == NULL) memset(&L->handles[i], 0, sizeof L->handles[i]);
}

enum { GAMES_LIST = 1, PLAYERS_LIST = 2 };  /* the two buffers' handles */

static void free_handle(void *ctx, uint32_t handle)
{
    (void)ctx;
    uint8_t **const list = handle == GAMES_LIST ? &L->games.games.list : &L->players.list;
    free(*list);
    *list = NULL;
}

void link_release(void)
{
    RaceNetBuffers b = { L->games.games.count, L->games.games.list != NULL ? GAMES_LIST : 0, L->players.count,
                         L->players.list != NULL ? PLAYERS_LIST : 0 };
    static const RaceNetReleaseOps ops = { NULL, free_handle };
    race_net_release(&ops, &b);
    L->games.games.count = b.unknown_4bc6c4, L->players.count = b.unknown_4bc6cc;
}

void link_deadline(int32_t ticks) { L->deadline = ticks == RACE_NET_NO_DEADLINE ? RACE_NET_NO_DEADLINE : link_clock() + ticks; }

static void keys(void *ctx) { (void)ctx, link_service(); }
static void service(void *ctx) { (void)ctx, link_service(); }

static void forget(void *ctx, uint32_t session, uint32_t name)
{
    (void)ctx, (void)session;
    const int i = link_index(name - RACE_NET_NAME_AT);
    if (i >= 0) link_forget(L->entries[i].name);
}

int32_t link_wait_step(void)
{
    static const RaceNetOps ops = { NULL, keys, service, forget };
    const RaceNetWords w = { menu_words.link_lost, L->deadline, link_clock() };
    return race_net_wait_step(&ops, &w);
}

/* --- 00442970: host --- */

static void host_reset(void *ctx) { (void)ctx, link_reset(); }

static int32_t create(void *ctx, uint32_t session, const uint8_t *name, const uint8_t *password, uint32_t handler)
{
    (void)ctx, (void)session;
    uint8_t params[NET_GAME_PARAMS] = { 0 };  /* 0050a320's name, then 0050a360's word (the most players) */
    memcpy(params, name, RACE_NET_SESSION_NAME);
    params[RACE_NET_SESSION_NAME] = (uint8_t)L->unknown_50a360;
    return net_runtime_new_game(&L->rt, &L->session.s, params, (const char *)password, (int32_t)handler);
}

static void shut_down(void *ctx) { (void)ctx, link_shut_down(); }
static void release(void *ctx) { (void)ctx, link_release(); }

int32_t link_host(const char *name)
{
    RaceNetHost h = { menu_words.mode_menu, menu_words.link_lost, menu_words.link_game, (int16_t)resource_german(),
                      L->unknown_4bc6ac, menu_words.intro_seen, L->joined, L->counts, L->queue.head, L->queue.tail,
                      L->queue.count, (const uint8_t *)frontend_player_name, L->state, L->session_name,
                      L->unknown_50a360 };
    static const RaceNetHostOps ops = { NULL, host_reset, create, message, shut_down, release };
    const int32_t r = race_net_host_start(&ops, &h, (const uint8_t *)name);
    menu_words.mode_menu = h.host, menu_words.link_lost = h.dropped, menu_words.link_game = h.play;
    L->unknown_4bc6ac = h.unknown_4bc6ac, menu_words.intro_seen = h.unknown_4fbb0c, L->joined = h.joined;
    L->counts = h.counts, L->queue.head = h.queue_head, L->queue.tail = h.unknown_50a36c;
    L->queue.count = h.queue_count, L->unknown_50a360 = h.unknown_50a360;
    if (r != 0) port_log("mark:game_hosted\n");   /* port: the test harness starts the guests once the game is up */
    return r;
}

/* --- 00442ad0: connect --- */

static void connect_in(RaceNetConnect *c)
{
    *c = (RaceNetConnect){ menu_words.mode_menu, menu_words.link_lost, menu_words.link_game, (int16_t)resource_german(),
                           L->view, L->unknown_4bc6dc, L->flags_a[8], link_esc(), menu_words.intro_seen,
                           menu_words.single, L->session.s.joined, 0, L->state, L->incoming, L->session.address,
                           L->entries };
}

static void connect_out(const RaceNetConnect *c)
{
    menu_words.mode_menu = c->host, menu_words.link_lost = c->dropped, menu_words.link_game = c->play;
    L->view = c->unknown_52b316, L->unknown_4bc6dc = c->unknown_4bc6dc, L->flags_a[8] = c->unknown_50a5bc;
    menu_words.intro_seen = c->unknown_4fbb0c, menu_words.single = c->followed;
}

/* 00442ad0's ops (ctx its words): the globals current while they run */
static int32_t c_connect(void *ctx, uint32_t session, const uint8_t *address, const uint8_t *password)
{
    (void)session;
    connect_out(ctx);
    const int32_t r = net_runtime_set_game(&L->rt, &L->session.s, address, (const char *)password);
    connect_in(ctx);
    return r;
}

static void c_message(void *ctx, uint32_t text, int32_t ticks)
{
    connect_out(ctx);
    link_message(text, ticks);
    connect_in(ctx);
}

static void c_service(void *ctx)
{
    connect_out(ctx);
    link_service();
    connect_in(ctx);
}

static void c_reset(void *ctx)
{
    connect_out(ctx);
    link_reset();
    connect_in(ctx);
}

static int32_t c_join(void *ctx, const RaceNetLink *entry)
{
    connect_out(ctx);
    const int32_t r = link_join(entry);
    connect_in(ctx);
    return r;
}

static void c_shut_down(void *ctx)
{
    connect_out(ctx);
    link_shut_down();
    connect_in(ctx);
}

static void c_deadline(void *ctx, int32_t ticks) { (void)ctx, link_deadline(ticks); }

static int32_t c_wait(void *ctx)
{
    connect_out(ctx);
    const int32_t r = link_wait_step();
    connect_in(ctx);
    return r;
}

static void c_give_up(void *ctx)
{
    connect_out(ctx);
    link_give_up();
    connect_in(ctx);
}

static void c_release(void *ctx)
{
    connect_out(ctx);
    link_release();
    connect_in(ctx);
}

int32_t link_connect(const uint8_t *address)
{
    RaceNetConnect c;
    connect_in(&c);
    const RaceNetConnectOps ops = { &c, c_connect, c_message, c_service, c_reset, c_join, c_shut_down, c_deadline,
                                    c_wait, c_give_up, lock, lock, c_release };
    const int32_t r = race_net_connect(&ops, &c, address);
    connect_out(&c);
    return r;
}

/* --- 00446220: give up --- */

static void close_session(void *ctx, uint32_t session, int32_t how)
{
    (void)ctx, (void)session;
    net_runtime_shut_game(&L->rt, &L->session.s, how);
}

static void leave(void *ctx, uint32_t session)
{
    (void)ctx, (void)session;
    net_runtime_end_game(&L->rt, &L->session.s);
}

/* 00442d60 */
void link_close_session(void)
{
    RaceNetClose c = { menu_words.intro_seen, menu_words.link_game, L->unknown_4bc6ac };
    static const RaceNetCloseOps ops = { NULL, close_session };
    race_net_close_session(&ops, &c);
    menu_words.intro_seen = c.unknown_4fbb0c, menu_words.link_game = c.play, L->unknown_4bc6ac = c.unknown_4bc6ac;
}

void link_give_up(void)
{
    RaceNetGiveUp g = { &L->stand_ins[1], L->unknown_4bcb94,
                        { menu_words.intro_seen, menu_words.link_game, L->unknown_4bc6ac }, L->flags_a, L->flags_b,
                        L->view, menu_words.single, menu_words.link_lost };
    static const RaceNetGiveUpOps ops = { NULL, close_session, leave, shut_down };
    race_net_give_up(&ops, &g);
    L->unknown_4bcb94 = g.unknown_4bcb94, menu_words.intro_seen = g.close.unknown_4fbb0c;
    menu_words.link_game = g.close.play, L->unknown_4bc6ac = g.close.unknown_4bc6ac;
    L->view = g.unknown_52b316, menu_words.single = g.followed, menu_words.link_lost = g.dropped;
    link_entries_sync();
}

void link_close(void) { link_give_up(); }
