/* The network transport as the game runs it (net_runtime.h). */
#define _POSIX_C_SOURCE 200809L
#include "game/net/net_runtime.h"
#include "game/sys/port_log.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum { RUNTIME_CATCH_UP = 100 };  /* the most ticks one service call makes up (a second's) */

static NetRuntime *rt_of(void *ctx) { return ctx; }

static void refresh_clock(NetRuntime *rt) { rt->clock = (int32_t)rt->ticks(rt->ctx); }

/* --- the threads' stand-ins: one thread, so the mutexes do nothing --- */

static void *mutex_create(void *ctx) { return ctx; }
static void mutex_none(void *ctx, void *mutex) { (void)ctx, (void)mutex; }

static int32_t on_main_thread(void *ctx, uint32_t thread) { return (void)ctx, (void)thread, 1; }

static void say(NetRuntime *rt, const char *text)
{
    if (rt->message != NULL) rt->message(rt->ctx, text);
    else port_log("net: %s\n", text);
}

static void fatal_name(void *ctx, const char *format, const char *name)
{
    char text[160];
    snprintf(text, sizeof text, "%s (%s)", format, name);
    say(rt_of(ctx), text);
}

static void fatal_value(void *ctx, const char *format, int32_t value)
{
    char text[160];
    snprintf(text, sizeof text, "%s (%d)", format, (int)value);
    say(rt_of(ctx), text);
}

static void message_text(void *ctx, const char *text) { say(rt_of(ctx), text); }

/* --- the trace ring (004c62b8 / 004c62fc stay 0: never called) --- */

static void trace_check(void *ctx, const char *name, uint32_t data, uint32_t text, int32_t value, int32_t flagged,
                        int32_t kind)
{
    (void)ctx, (void)name, (void)data, (void)text, (void)value, (void)flagged, (void)kind;
}

static void trace_data(void *ctx, const char *name, const int32_t *message, uint8_t *out, int32_t size)
{
    (void)ctx, (void)name, (void)message, (void)out, (void)size;
}

static void trace_word(void *ctx, const char *name, const int32_t *message) { (void)ctx, (void)name, (void)message; }

static void trace_flag(void *ctx, const char *name, const int32_t *word, int32_t flagged)
{
    (void)ctx, (void)name, (void)word, (void)flagged;
}

static void trace_name(void *ctx, const char *name) { (void)ctx, (void)name; }

static void trace_packet(void *ctx, const char *name, const int32_t *data, const uint8_t *text, int32_t value,
                         int32_t flagged, int32_t kind)
{
    (void)ctx, (void)name, (void)data, (void)text, (void)value, (void)flagged, (void)kind;
}

/* --- the clock and the waits --- */

static int32_t clock_now(void *ctx)
{
    NetRuntime *const rt = rt_of(ctx);
    refresh_clock(rt);
    return rt->clock;
}

static void wait_service(void *ctx)
{
    NetRuntime *const rt = rt_of(ctx);
    if (rt->yield != NULL) {
        rt->yield(rt->ctx);
    } else {
        const struct timespec pause = { 0, 1000000 };
        nanosleep(&pause, NULL);
        net_runtime_service(rt);
    }
    refresh_clock(rt);
}

/* --- the service list (0049d028 / 0049d0a0) --- */

static void sessions_on(void *ctx)
{
    NetRuntime *const rt = rt_of(ctx);
    rt->service_started = 1;
    rt->serviced = rt->ticks(rt->ctx);
}

static void sessions_off(void *ctx) { rt_of(ctx)->service_started = 0; }

static void packets_on(void *ctx)
{
    NetRuntime *const rt = rt_of(ctx);
    rt->packets_on = 1;
    rt->serviced = rt->ticks(rt->ctx);
}

static void packets_off(void *ctx) { rt_of(ctx)->packets_on = 0; }

static void at_exit_none(void *ctx) { (void)ctx; }

/* --- the UDP driver's methods as the session's driver --- */

static int32_t udp_info(void *ctx, NetServiceDriver *d, int32_t *a, int32_t *b)
{
    (void)ctx, (void)d;
    *a = 1, *b = 0;
    return 1;
}

static int32_t udp_present(void *ctx, const NetServiceDriver *d) { return (void)ctx, (void)d, net_udp_present(); }

static uint32_t udp_open(void *ctx, const NetServiceDriver *d)
{
    (void)d;
    return net_udp_service_open(&rt_of(ctx)->udp);
}

static void udp_stop(void *ctx, const NetServiceDriver *d) { (void)ctx, (void)d; }

static int32_t udp_stop_all(void *ctx) { return (void)ctx, 1; }

static void udp_self(void *ctx, const NetServiceDriver *d, uint32_t handle, uint8_t *self)
{
    (void)d;
    NetUdp *const u = net_udp_service_get(&rt_of(ctx)->udp, handle);
    if (u != NULL) net_udp_self(u, self);
}

static void udp_close(void *ctx, uint32_t handle) { net_udp_service_close(&rt_of(ctx)->udp, handle); }

static int32_t udp_advertise(void *ctx, uint32_t handle, uint8_t *game) { return (void)ctx, (void)handle, (void)game, 1; }

static void udp_leave(void *ctx, uint32_t handle, uint8_t *game) { (void)ctx, (void)handle, (void)game; }

static void udp_stop_find(void *ctx, uint32_t handle) { (void)ctx, (void)handle; }

/* --- the records the library allocates --- */

static void session_wire(NetSessionRecord *r)
{
    r->s.self = r->self, r->s.address = r->address, r->s.guid = r->guid;
    r->s.text_3c = r->text_3c, r->s.text_70 = r->text_70;
}

static void session_clear(void *ctx, NetSession *s)
{
    (void)ctx;
    NetSessionRecord *const r = (NetSessionRecord *)(void *)s;
    memset(r, 0, sizeof *r);
    session_wire(r);
}

static NetServer *server_alloc(void *ctx, const char *name, uint32_t size, uint32_t tag)
{
    (void)ctx, (void)name, (void)size, (void)tag;
    NetServerRecord *const r = calloc(1, sizeof *r);
    if (r == NULL) return NULL;
    r->v.game = r->game, r->v.password = r->password, r->v.params = r->params, r->v.entries = r->entries;
    return &r->v;
}

static void server_release(void *ctx, NetServer *v)
{
    (void)ctx;
    free(v);
}

static NetPacketConn *conn_alloc(void *ctx, const char *name, uint32_t size, uint32_t tag, size_t bytes)
{
    (void)ctx, (void)name, (void)size, (void)tag;
    return calloc(1, bytes);
}

static void conn_release(void *ctx, NetPacketConn *c)
{
    (void)ctx;
    free(c);
}

static int32_t accept_hook(void *ctx, int32_t fn, NetSession *s, uint8_t *record)
{
    NetRuntime *const rt = rt_of(ctx);
    return rt->accept == NULL ? 1 : rt->accept(rt->ctx, fn, s, record);
}

/* --- 004c62c8, the packet driver over a session --- */

static int32_t link_open(void *ctx, NetPacketHandle *h) { return net_link_open(&rt_of(ctx)->link, h); }
static void link_close(void *ctx, NetPacketHandle *h) { net_link_close(&rt_of(ctx)->link, h); }
static int32_t link_alive(void *ctx, NetPacketHandle *h) { return (void)ctx, net_link_alive(h); }
static uint32_t link_room(void *ctx, NetPacketHandle *h) { return net_link_room(&rt_of(ctx)->link, h); }
static int32_t link_ready(void *ctx, NetPacketHandle *h) { return net_link_query(&rt_of(ctx)->link, h); }

static uint32_t link_read(void *ctx, NetPacketHandle *h, uint8_t *to, uint32_t size)
{
    return (uint32_t)net_link_receive(&rt_of(ctx)->link, h, to, (int32_t)size);
}

static uint32_t link_send(void *ctx, NetPacketHandle *h, const uint8_t *data, uint32_t length)
{
    return (uint32_t)net_link_send(&rt_of(ctx)->link, h, (uint8_t *)data, (int32_t)length);
}

/* --- the ops --- */

static void fill_session_ops(NetRuntime *rt)
{
    rt->mutex = (NetMutexOps){ rt, mutex_create, mutex_none, mutex_none, mutex_none };
    rt->check_ops = (NetCheckOps){ rt, on_main_thread, fatal_name, trace_check };
    rt->equal = (NetEqualOps){ .ctx = rt, .driver_equal = net_udp_service_equal };
    rt->clock_ops = (NetClockOps){ rt, clock_now };
    rt->wait = (NetWaitOps){ rt, wait_service };
    rt->end = (NetEndOps){ .ctx = rt, .check = &rt->check_ops, .mutex = &rt->mutex, .sessions = &rt->sessions,
                           .driver_leave = udp_leave, .driver_send = net_udp_service_send_word,
                           .trace = trace_word, .release = server_release };
    rt->kill_service = (NetKillServiceOps){ .ctx = rt, .check = &rt->check_ops, .end = &rt->end, .wait = &rt->wait,
                                            .driver_close = udp_close, .driver_stop = udp_stop_all,
                                            .unservice = sessions_off };
    rt->init_service = (NetInitServiceOps){
        .ctx = rt, .check = &rt->check_ops, .mutex = &rt->mutex, .sessions = &rt->sessions, .drivers = rt->drivers,
        .count = &rt->driver_count, .started = &rt->init_started, .lock = &rt->lock, .resend = &rt->resend,
        .timeout = &rt->timeout, .at_exit = at_exit_none, .clear = session_clear, .driver_present = udp_present,
        .driver_open = udp_open, .driver_stop = udp_stop, .driver_self = udp_self, .service_on = sessions_on };
    rt->services = (NetServicesOps){ .ctx = rt, .check = &rt->check_ops, .drivers = rt->drivers,
                                     .count = &rt->driver_count, .driver_info = udp_info };
    rt->new_game = (NetNewGameOps){ .ctx = rt, .check = &rt->check_ops, .end = &rt->end, .message = fatal_value,
                                    .alloc = server_alloc, .release = server_release, .driver_open = udp_advertise,
                                    .now = &rt->now, .stamp = &rt->stamp, .tag = &rt->tag };
    rt->shut_game = (NetShutGameOps){ .ctx = rt, .check = &rt->check_ops, .driver_leave = udp_leave,
                                      .release = server_release };
    rt->kill = (NetKillOps){ .ctx = rt, .check = &rt->check_ops, .equal = &rt->equal,
                             .driver_send = net_udp_service_send_word, .message = message_text };
}

static void fill_request_ops(NetRuntime *rt)
{
    rt->request = (NetRequestOps){ .ctx = rt, .clock = &rt->clock_ops, .wait = &rt->wait, .wait_clock = &rt->clock,
                                   .next_id = &rt->next_request, .deadline = &rt->deadline,
                                   .trace_level = &rt->trace_level, .resend = &rt->resend, .trace = trace_data,
                                   .driver_send = net_udp_service_send };
    rt->set_game = (NetSetOps){ .ctx = rt, .check = &rt->check_ops, .equal = &rt->equal, .end = &rt->end,
                                .request = &rt->request, .timeout = &rt->timeout };
    rt->players = (NetPlayersOps){ .ctx = rt, .check = &rt->check_ops, .equal = &rt->equal, .request = &rt->request,
                                   .timeout = &rt->timeout };
    rt->games = (NetGamesOps){ .ctx = rt, .check = &rt->check_ops, .clock = &rt->clock_ops, .wait = &rt->wait,
                               .wait_clock = &rt->clock, .deadline = &rt->deadline,
                               .driver_find = net_udp_service_find, .driver_stop_find = udp_stop_find };
    rt->info = (NetGameInfoOps){ rt, &rt->trace_level, trace_data, net_udp_service_send };
    rt->add = (NetAddPlayerOps){ rt, message_text, &rt->now };
    rt->serve = (NetServeOps){ .ctx = rt, .equal = &rt->equal, .info = &rt->info, .add = &rt->add,
                               .trace_level = &rt->trace_level, .now = &rt->now, .stamp = &rt->stamp,
                               .trace = trace_data, .driver_send = net_udp_service_send, .accept = accept_hook };
    rt->receive = (NetReceiveOps){ rt, &rt->serve, net_udp_service_receive };
    rt->poll = (NetPollOps){ rt, &rt->receive, &rt->trace_level, &rt->stamp, trace_flag, net_udp_service_send };
    rt->sessions_ops = (NetSessionsOps){ &rt->mutex, &rt->sessions, &rt->poll };
}

static void fill_packet_ops(NetRuntime *rt)
{
    rt->link = (NetLinkOps){ .ctx = rt, .mutex = &rt->mutex, .lock = &rt->lock, .trace_level = &rt->trace_level,
                             .trace = trace_name, .driver_link = net_udp_service_link,
                             .driver_ready = net_udp_service_ready, .room = &rt->room,
                             .driver_query = net_udp_service_query, .serve = &rt->serve,
                             .driver_receive = net_udp_service_receive, .driver_send = net_udp_service_send };
    rt->link_driver = (NetDriver){ rt, link_open, link_close, link_alive, link_room, link_ready, link_read, link_send,
                                   0x102, 0x74, NET_DRIVER_SINGLE };
    rt->packet = (NetPacketOps){ rt, &rt->mutex, &rt->packet_trace, trace_packet };
    rt->queue_message = (NetQueueMessage){ rt, message_text };
    rt->close = (NetCloseOps){ rt, &rt->packet, &rt->wait, &rt->clock, &rt->open, &rt->closed, message_text,
                               conn_release };
    rt->recv = (NetPacketRecvOps){ &rt->close, &rt->queue_message, &rt->open_state };
    rt->shutdown = (NetShutdownOps){ rt, &rt->close, packets_off, &rt->packet_started };
    rt->start = (NetStartOps){ rt, &rt->mutex, &rt->open, &rt->closed, packets_on, at_exit_none,
                               &rt->packet_started };
    rt->open_ops = (NetOpenOps){ rt, &rt->close, on_main_thread, &rt->surface_locked, fatal_value, conn_alloc,
                                 &rt->next_conn, &rt->open_state };
    rt->connect = (NetConnectOps){ .ctx = rt, .check = &rt->check_ops, .open = &rt->open_ops,
                                   .driver = &rt->link_driver };
}

void net_runtime_init(NetRuntime *rt, uint16_t port, const char *const *hosts, void *ctx, uint32_t (*ticks)(void *ctx))
{
    memset(rt, 0, sizeof *rt);
    rt->udp.port = port, rt->udp.hosts = hosts;
    rt->drivers[0] = (NetServiceDriver *)&NET_UDP_DRIVER;
    rt->driver_count = 1;
    /* the exe's data: 004c62a4 450, 004c62a8 100, 004c62b0 32, 004c62bc 1,
     * 004c62e4 0x102, 004c62f8 1, 004c6300 400, 004c6170 100 */
    rt->now = 450, rt->stamp = 100, rt->tag = 32, rt->next_request = 1, rt->room = 0x102;
    rt->next_conn = 1, rt->open_state = 400, rt->timeout = 100;
    rt->ctx = ctx, rt->ticks = ticks;
    fill_session_ops(rt);
    fill_request_ops(rt);
    fill_packet_ops(rt);
    refresh_clock(rt);
    rt->serviced = (uint32_t)rt->clock;
}

void net_runtime_service(NetRuntime *rt)
{
    if (rt->serving) return;
    rt->serving = 1;
    refresh_clock(rt);
    uint32_t n = (uint32_t)rt->clock - rt->serviced;
    if (n > RUNTIME_CATCH_UP) n = RUNTIME_CATCH_UP;
    rt->serviced = (uint32_t)rt->clock;
    for (uint32_t i = 0; i < n; i++) {
        if (rt->service_started) net_sessions_service(&rt->sessions_ops);
        if (rt->packets_on) net_packet_tick(&rt->recv, &rt->open);
    }
    rt->serving = 0;
}

int32_t net_runtime_init_service(NetRuntime *rt, NetSessionRecord *r, const uint8_t *guid, const char *text_3c,
                                 const char *text_70)
{
    refresh_clock(rt);
    session_wire(r);
    const int32_t ok = net_init_service(&rt->init_service, &rt->check, &r->s, guid, NET_UDP_DRIVER.guid, text_3c,
                                        text_70);
    if (ok != 0 && rt->packet_started == 0) net_packet_start(&rt->start);
    return ok;
}

int32_t net_runtime_get_services(NetRuntime *rt, const NetServicesCallback *cb)
{
    return net_get_services(&rt->services, &rt->check, cb, 0, 0, NET_SERVICES_MOST);
}

int32_t net_runtime_kill_service(NetRuntime *rt, NetSession *s)
{
    refresh_clock(rt);
    return net_kill_service(&rt->kill_service, &rt->check, s);
}

int32_t net_runtime_new_game(NetRuntime *rt, NetSession *s, uint8_t *params, const char *password, int32_t arg)
{
    refresh_clock(rt);
    return net_new_game(&rt->new_game, &rt->check, s, params, password, arg);
}

int32_t net_runtime_end_game(NetRuntime *rt, NetSession *s)
{
    refresh_clock(rt);
    return net_end_game(&rt->end, &rt->check, s);
}

int32_t net_runtime_shut_game(NetRuntime *rt, NetSession *s, int32_t release)
{
    return net_shut_game(&rt->shut_game, &rt->check, s, release);
}

int32_t net_runtime_get_games(NetRuntime *rt, NetSession *s, const NetGamesCallback *cb, int32_t timeout)
{
    refresh_clock(rt);
    return net_get_games(&rt->games, &rt->check, s, cb, timeout);
}

int32_t net_runtime_set_game(NetRuntime *rt, NetSession *s, const uint8_t *address, const char *password)
{
    refresh_clock(rt);
    return net_set_game(&rt->set_game, &rt->check, s, address, password);
}

int32_t net_runtime_get_players(NetRuntime *rt, NetSession *s, const uint8_t *address, const NetPlayerCallback *cb)
{
    refresh_clock(rt);
    return net_get_players(&rt->players, &rt->check, s, address, cb);
}

int32_t net_runtime_kill_player(NetRuntime *rt, NetSession *s, const uint8_t *address)
{
    return net_kill_player(&rt->kill, &rt->check, s, address, &rt->now);
}

int32_t net_runtime_open(NetRuntime *rt, NetPacketHandle *h, uint32_t tag, int32_t crc32, uint32_t size, uint32_t bits)
{
    refresh_clock(rt);
    return net_open_net_connection(&rt->connect, &rt->check, h, tag, crc32, size, bits);
}

int32_t net_runtime_close(NetRuntime *rt, NetPacketHandle *h)
{
    refresh_clock(rt);
    return net_close_packet_connection(&rt->close, h);
}

int32_t net_runtime_send(NetRuntime *rt, NetPacketHandle *h, const uint8_t *data, uint32_t n)
{
    return net_packet_write(&rt->packet, &rt->queue_message, h, data, n);
}

void net_runtime_shutdown(NetRuntime *rt)
{
    refresh_clock(rt);
    net_packet_shutdown(&rt->shutdown);
}
