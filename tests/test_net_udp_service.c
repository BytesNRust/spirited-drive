#define _POSIX_C_SOURCE 200809L
/* The UDP driver under the ported protocol (game/net/net_udp_service.c):
 * over the loopback a client asks for a game by name (NET_SERVER_ASK), the
 * host's net_receive / net_serve answer with NET_GAME_INFO, and the
 * client's net_receive hands the game to its getnetgames callback. */
#include "game/net/net_server.h"
#include "game/net/net_udp_service.h"

#include "tests/test.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static int32_t trace_level = 0, now = 0, stamp = 0;

static void serve_trace(void *ctx, const char *name, const int32_t *message, uint8_t *out, int32_t size)
{
    (void)ctx, (void)name, (void)message, (void)out, (void)size;
}

static void add_message(void *ctx, const char *text) { (void)ctx, (void)text; }

static int32_t accept_all(void *ctx, int32_t fn, NetSession *s, uint8_t *record)
{
    return (void)ctx, (void)fn, (void)s, (void)record, 1;
}

typedef struct {
    NetEqualOps equal;
    NetGameInfoOps info;
    NetAddPlayerOps add;
    NetServeOps serve;
    NetReceiveOps receive;
} Ops;

static void ops_for(Ops *o, NetUdpService *svc)
{
    o->equal = (NetEqualOps){ .ctx = svc, .driver_equal = net_udp_service_equal };
    o->info = (NetGameInfoOps){ .ctx = svc, .trace_level = &trace_level, .trace = serve_trace,
                                .driver_send = net_udp_service_send };
    o->add = (NetAddPlayerOps){ .ctx = svc, .message = add_message, .now = &now };
    o->serve = (NetServeOps){ .ctx = svc, .equal = &o->equal, .info = &o->info, .add = &o->add,
                              .trace_level = &trace_level, .now = &now, .stamp = &stamp, .trace = serve_trace,
                              .driver_send = net_udp_service_send, .accept = accept_all };
    o->receive = (NetReceiveOps){ .ctx = svc, .serve = &o->serve, .driver_receive = net_udp_service_receive };
}

/* Until something waits on the handle (1 s at most) */
static int32_t wait_query(NetUdpService *svc, uint32_t handle)
{
    const struct timespec pause = { 0, 1000000 };
    for (int i = 0; i < 1000; i++) {
        if (net_udp_service_query(svc, handle) != 0) return 1;
        nanosleep(&pause, NULL);
    }
    return 0;
}

typedef struct {
    int calls;
    char name[0x21];
    int32_t players;
    uint8_t address[NET_UDP_ADDRESS];
} Found;

static int32_t found_game(void *ctx, NetSession *s, const uint8_t *info, int32_t players, uint8_t *address)
{
    Found *const f = ctx;
    (void)s;
    f->calls++;
    memcpy(f->name, info, 0x20);
    f->players = players;
    memcpy(f->address, address, NET_UDP_ADDRESS);
    return 1;
}

static void test_handles(void)
{
    NetUdpService svc = { 0 };
    CHECK(net_udp_service_get(&svc, 0) == NULL);
    const uint32_t a = net_udp_service_open(&svc), b = net_udp_service_open(&svc);
    CHECK_EQ_INT(a, 1);
    CHECK_EQ_INT(b, 2);
    CHECK(net_udp_service_get(&svc, a) != NULL && net_udp_service_get(&svc, NET_UDP_HANDLES + 1) == NULL);
    CHECK_EQ_INT(net_udp_service_ready(&svc, a), 1);
    net_udp_service_close(&svc, a);
    CHECK(net_udp_service_get(&svc, a) == NULL);
    CHECK_EQ_INT(net_udp_service_ready(&svc, a), 0);
    int32_t message = 7;
    uint8_t address[NET_UDP_ADDRESS], data[4];
    CHECK_EQ_INT(net_udp_service_receive(&svc, a, address, &message, data, 4), 0);
    CHECK_EQ_INT(message, 0);
    CHECK_EQ_INT(net_udp_service_send(&svc, a, address, NET_ACK, NULL, 0), 0);
    CHECK_EQ_INT(net_udp_service_open(&svc), 1);  /* the freed slot again */
    net_udp_service_close(&svc, 1), net_udp_service_close(&svc, b), net_udp_service_close(&svc, 0);
    CHECK_EQ_INT(memcmp(NET_UDP_DRIVER.guid, "TNFS UDP LAN/IP", 16), 0);
}

static void test_find_game(void)
{
    /* the host: a session with its server, a game named EAGAME.0123 for 4 */
    NetUdpService host = { 0 };
    uint8_t host_self[NET_UDP_ADDRESS], entries[NET_PLAYERS * NET_PLAYER_SIZE] = { 0 };
    uint8_t game[0x20] = "EAGAME.0123", params[0x44] = "Shelby race", password[16] = { 0 };
    memcpy(params + 0x20, "TR1", 4);
    params[0x40] = 4;
    NetServer server = { .players = 1, .entries = entries, .game = game, .password = (char *)password,
                         .params = params };
    NetSession hs = { .server = &server, .handle = net_udp_service_open(&host), .self = host_self,
                      .address = host_self };
    server.session = &hs;
    CHECK(hs.handle != 0);
    if (hs.handle == 0) return;
    net_udp_self(net_udp_service_get(&host, hs.handle), host_self);

    /* the client: the host as a direct host */
    char at[32];
    snprintf(at, sizeof at, "127.0.0.1:%u", (unsigned)net_udp_port(net_udp_service_get(&host, hs.handle)));
    const char *hosts[] = { at, NULL };
    NetUdpService client = { .hosts = hosts };
    uint8_t client_self[NET_UDP_ADDRESS], server_at[NET_UDP_ADDRESS] = { 0 };
    NetSession cs = { .handle = net_udp_service_open(&client), .self = client_self, .address = server_at };
    CHECK(cs.handle != 0);

    Ops ho, co;
    ops_for(&ho, &host), ops_for(&co, &client);
    CHECK_EQ_INT(net_udp_service_find(&client, cs.handle, "EAGAME.0123"), 1);
    CHECK_EQ_INT(wait_query(&host, hs.handle), 1);
    NetRequest hr = { .session = &hs };
    net_receive(&ho.receive, &hs, &hr);

    Found f = { 0 };
    const NetGamesCallback cb = { &f, found_game };
    NetRequest cr = { .session = &cs, .games = &cb };
    /* the client may hear its own LAN broadcast first: receive until the answer (1 s at most) */
    for (int i = 0; i < 1000 && f.calls == 0; i++)
        if (wait_query(&client, cs.handle) != 0) net_receive(&co.receive, &cs, &cr);
    CHECK_EQ_INT(f.calls, 1);
    CHECK(strcmp(f.name, "Shelby race") == 0);
    CHECK_EQ_INT(f.players, 1);
    CHECK_EQ_INT(cr.answered, 1);
    CHECK_EQ_INT(net_udp_port(net_udp_service_get(&host, hs.handle)),
                 (unsigned)(f.address[4] << 8 | f.address[5]));

    /* a name that is not the game's: no answer */
    CHECK_EQ_INT(net_udp_service_find(&client, cs.handle, "EAGAME.9999"), 1);
    CHECK_EQ_INT(wait_query(&host, hs.handle), 1);
    net_receive(&ho.receive, &hs, &hr);
    const struct timespec pause = { 0, 50000000 };
    nanosleep(&pause, NULL);
    net_receive(&co.receive, &cs, &cr);
    CHECK_EQ_INT(f.calls, 1);

    net_udp_service_close(&host, hs.handle), net_udp_service_close(&client, cs.handle);
}

/* the ask carries all 0x20 bytes net_serve compares: a game's name as
 * net_game_name makes it (43 characters) is found */
static void test_find_long_name(void)
{
    NetUdpService host = { 0 };
    uint8_t host_self[NET_UDP_ADDRESS], entries[NET_PLAYERS * NET_PLAYER_SIZE] = { 0 }, game[0x30] = { 0 };
    uint8_t params[0x44] = "Long name", password[16] = { 0 }, guid[16] = "0123456789abcde";
    params[0x40] = 4;
    net_game_name(guid, (char *)game);
    CHECK_EQ_INT((int)strlen((const char *)game), NET_GAME_NAME - 1);
    NetServer server = { .players = 1, .entries = entries, .game = game, .password = (char *)password,
                         .params = params };
    NetSession hs = { .server = &server, .handle = net_udp_service_open(&host), .self = host_self,
                      .address = host_self };
    server.session = &hs;
    if (hs.handle == 0) return;
    net_udp_self(net_udp_service_get(&host, hs.handle), host_self);
    char at[32];
    snprintf(at, sizeof at, "127.0.0.1:%u", (unsigned)net_udp_port(net_udp_service_get(&host, hs.handle)));
    const char *hosts[] = { at, NULL };
    NetUdpService client = { .hosts = hosts };
    uint8_t client_self[NET_UDP_ADDRESS], server_at[NET_UDP_ADDRESS] = { 0 };
    NetSession cs = { .handle = net_udp_service_open(&client), .self = client_self, .address = server_at };
    Ops ho, co;
    ops_for(&ho, &host), ops_for(&co, &client);
    CHECK_EQ_INT(net_udp_service_find(&client, cs.handle, (const char *)game), 1);
    CHECK_EQ_INT(wait_query(&host, hs.handle), 1);
    NetRequest hr = { .session = &hs };
    net_receive(&ho.receive, &hs, &hr);
    Found f = { 0 };
    const NetGamesCallback cb = { &f, found_game };
    NetRequest cr = { .session = &cs, .games = &cb };
    for (int i = 0; i < 5 && f.calls == 0; i++)
        if (wait_query(&client, cs.handle) != 0) net_receive(&co.receive, &cs, &cr);
    CHECK_EQ_INT(f.calls, 1);
    CHECK(strcmp(f.name, "Long name") == 0);
    net_udp_service_close(&host, hs.handle), net_udp_service_close(&client, cs.handle);
}

int main(void)
{
    test_handles();
    test_find_game();
    test_find_long_name();
    return TEST_REPORT();
}
