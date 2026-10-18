#define _POSIX_C_SOURCE 200809L
/* The transport as the game runs it (game/net/net_runtime.c), two of them
 * in one process over the loopback: the host makes a game (newnetgame), the
 * client finds it (getnetgames) and joins (setnetgame), the host's accept
 * hook sees the join, both open a packet connection (opennetconnection over
 * 004c62c8) and a packet goes each way (sendpacket, the deliver hook). */
#include "game/net/net_runtime.h"

#include "tests/test.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static NetRuntime host, client;
static NetSessionRecord hs, cs;
static NetPacketHandle hh, ch;
static uint8_t host_peer[NET_ADDRESS], client_peer[NET_ADDRESS];
static int joins, host_opening;
static const uint8_t GUID[NET_GUID] = "TNFS link test!";

static uint32_t ticks(void *ctx)
{
    (void)ctx;
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint32_t)(t.tv_sec * 100 + t.tv_nsec / 10000000);
}

/* What the race does from its service op: the host opens its end once a
 * player joined (00442f58). In one process it waits for the client's open
 * to be under way (its wait runs this), as two machines would overlap */
static void yield(void *ctx)
{
    (void)ctx;
    const struct timespec pause = { 0, 1000000 };
    nanosleep(&pause, NULL);
    net_runtime_service(&host);
    net_runtime_service(&client);
    if (joins > 0 && ch.conn != NULL && hh.conn == NULL && !host_opening) {
        host_opening = 1;
        CHECK_EQ_INT(net_runtime_open(&host, &hh, 0x20, 0, 0xfa, 6), 1);
    }
}

static int32_t accept_join(void *ctx, int32_t fn, NetSession *s, uint8_t *record)
{
    (void)ctx, (void)s;
    CHECK_EQ_INT(fn, 0x442ec0);
    memcpy(host_peer, record, NET_ADDRESS);
    joins++;
    return 1;
}

typedef struct {
    int found;
    char name[0x21];
    uint8_t address[NET_ADDRESS];
} Found;

static int32_t found_game(void *ctx, NetSession *s, const uint8_t *info, int32_t players, uint8_t *address)
{
    Found *const f = ctx;
    (void)s, (void)players;
    f->found++;
    memcpy(f->name, info, 0x20);
    memcpy(f->address, address, NET_ADDRESS);
    return 1;
}

static uint8_t got[2][32];
static uint32_t got_n[2];

static int32_t deliver(NetPacketHandle *h, const uint8_t *data, uint32_t n)
{
    const int side = h == &hh ? 0 : 1;
    if (n <= sizeof got[side]) memcpy(got[side], data, n), got_n[side] = n;
    return 1;
}

static int wait_for(const uint32_t *n)
{
    for (int i = 0; i < 2000 && *n == 0; i++) yield(NULL);
    return *n != 0;
}

static void test_link(void)
{
    net_runtime_init(&host, 0, NULL, NULL, ticks);
    host.yield = yield, host.accept = accept_join;
    CHECK_EQ_INT(net_runtime_init_service(&host, &hs, GUID, "host", "host"), 1);
    char at[32];
    snprintf(at, sizeof at, "127.0.0.1:%u", (unsigned)net_udp_port(net_udp_service_get(&host.udp, hs.s.handle)));
    const char *hosts[] = { at, NULL };
    net_runtime_init(&client, 0, hosts, NULL, ticks);
    client.yield = yield;
    CHECK_EQ_INT(net_runtime_init_service(&client, &cs, GUID, "client", "client"), 1);

    uint8_t params[NET_GAME_PARAMS] = "Shelby race";
    params[0x40] = 4;
    CHECK_EQ_INT(net_runtime_new_game(&host, &hs.s, params, "", 0x442ec0), 1);

    Found f = { 0 };
    const NetGamesCallback cb = { &f, found_game };
    CHECK(net_runtime_get_games(&client, &cs.s, &cb, 200) != 0);
    CHECK_EQ_INT(f.found >= 1, 1);
    CHECK(strcmp(f.name, "Shelby race") == 0);

    hh.session = &hs.s, hh.address = host_peer, hh.deliver = deliver;
    CHECK(net_runtime_set_game(&client, &cs.s, f.address, "") > 0);
    CHECK_EQ_INT(joins, 1);

    ch.session = &cs.s, ch.address = client_peer, ch.deliver = deliver;
    memcpy(client_peer, cs.s.address, NET_ADDRESS);
    CHECK_EQ_INT(net_runtime_open(&client, &ch, 0x20, 0, 0xfa, 6), 1);
    CHECK(hh.conn != NULL && hh.open != 0);

    CHECK_EQ_INT(net_runtime_send(&client, &ch, (const uint8_t *)"to host", 8), 1);
    CHECK_EQ_INT(wait_for(&got_n[0]), 1);
    CHECK(got_n[0] == 8 && strcmp((const char *)got[0], "to host") == 0);
    CHECK_EQ_INT(net_runtime_send(&host, &hh, (const uint8_t *)"to client", 10), 1);
    CHECK_EQ_INT(wait_for(&got_n[1]), 1);
    CHECK(got_n[1] == 10 && strcmp((const char *)got[1], "to client") == 0);

    net_runtime_shutdown(&client);
    net_runtime_shutdown(&host);
    CHECK(ch.conn == NULL && hh.conn == NULL);
    net_runtime_kill_service(&client, &cs.s);
    net_runtime_kill_service(&host, &hs.s);
}

int main(void)
{
    test_link();
    return TEST_REPORT();
}
