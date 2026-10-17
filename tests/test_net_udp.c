#define _POSIX_C_SOURCE 200809L
/* The UDP driver (game/net/net_udp.c) over the loopback: two drivers exchange
 * datagrams, find a game by name, drop queued datagrams. */
#include "game/net/net_udp.h"
#include "game/net/net_server.h"

#include "tests/test.h"

#include <stdint.h>
#include <string.h>
#include <time.h>

/* The driver is non-blocking: ask until a datagram is there (1 s at most) */
static int32_t wait_receive(NetUdp *u, uint8_t *address, int32_t *message, uint8_t *data, int32_t size)
{
    const struct timespec pause = { 0, 1000000 };
    for (int i = 0; i < 1000; i++) {
        if (net_udp_query(u) != 0) return net_udp_receive(u, address, message, data, size);
        nanosleep(&pause, NULL);
    }
    return -1;
}

static void test_exchange(NetUdp *a, NetUdp *b)
{
    uint8_t at_a[NET_UDP_ADDRESS], at_b[NET_UDP_ADDRESS], from[NET_UDP_ADDRESS], data[0x200];
    CHECK_EQ_INT(net_udp_self(a, at_a), 1);
    CHECK_EQ_INT(net_udp_self(b, at_b), 1);
    CHECK_EQ_INT(net_udp_equal(at_a, at_a), 1);
    CHECK_EQ_INT(net_udp_equal(at_a, at_b), 0);
    CHECK_EQ_INT(net_udp_query(b), 0);
    int32_t message = 0;
    CHECK_EQ_INT(net_udp_receive(b, from, &message, data, sizeof data), 0);
    CHECK_EQ_INT(message, 0);

    CHECK_EQ_INT(net_udp_send(a, at_b, NET_DATA, (const uint8_t *)"hello", 5), 1);
    CHECK_EQ_INT(wait_receive(b, from, &message, data, sizeof data), 5);
    CHECK_EQ_INT(message, NET_DATA);
    CHECK_MEM(data, "hello", 5);
    CHECK_EQ_INT(net_udp_equal(from, at_a), 1);

    /* a message alone; data past 0x118 bytes cut; a short buffer takes what fits */
    CHECK_EQ_INT(net_udp_send(a, at_b, NET_ACK, NULL, 0), 1);
    CHECK_EQ_INT(wait_receive(b, from, &message, data, sizeof data), 0);
    CHECK_EQ_INT(message, NET_ACK);
    uint8_t big[0x150];
    for (size_t i = 0; i < sizeof big; i++) big[i] = (uint8_t)i;
    CHECK_EQ_INT(net_udp_send(a, at_b, NET_DATA, big, sizeof big), 1);
    CHECK_EQ_INT(wait_receive(b, from, &message, data, sizeof data), NET_UDP_MOST);
    CHECK_MEM(data, big, NET_UDP_MOST);
    CHECK_EQ_INT(net_udp_send(a, at_b, NET_DATA, big, 0x40), 1);
    CHECK_EQ_INT(wait_receive(b, from, &message, data, 0x10), 0x10);
    CHECK_MEM(data, big, 0x10);
}

/* link: what is queued from that address with that message goes */
static void test_link(NetUdp *a, NetUdp *b)
{
    uint8_t at_a[NET_UDP_ADDRESS], at_b[NET_UDP_ADDRESS], from[NET_UDP_ADDRESS], data[0x20];
    net_udp_self(a, at_a), net_udp_self(b, at_b);
    CHECK_EQ_INT(net_udp_send(a, at_b, NET_DATA, (const uint8_t *)"x", 1), 1);
    CHECK_EQ_INT(net_udp_send(a, at_b, NET_ACK, NULL, 0), 1);
    CHECK_EQ_INT(net_udp_send(a, at_b, NET_DATA, (const uint8_t *)"y", 1), 1);
    const struct timespec pause = { 0, 50000000 };
    nanosleep(&pause, NULL);
    CHECK_EQ_INT(net_udp_query(b), 1);
    CHECK_EQ_INT(net_udp_link(b, at_a, NET_DATA, 1), 1);
    int32_t message = 0;
    CHECK_EQ_INT(net_udp_receive(b, from, &message, data, sizeof data), 0);
    CHECK_EQ_INT(message, NET_ACK);
    CHECK_EQ_INT(net_udp_query(b), 0);
}

/* find: NET_SERVER_ASK with the name to each direct host */
static void test_find(NetUdp *a, NetUdp *b)
{
    uint8_t at_b[NET_UDP_ADDRESS], from[NET_UDP_ADDRESS], data[0x40];
    char host[32];
    net_udp_self(b, at_b);
    snprintf(host, sizeof host, "127.0.0.1:%u", (unsigned)net_udp_port(b));
    CHECK_EQ_INT(net_udp_add_host(a, host), 1);
    CHECK_EQ_INT(net_udp_add_host(a, "not an address"), 0);
    CHECK_EQ_INT(net_udp_find(a, "EAGAME.0123"), 1);
    int32_t message = 0;
    CHECK_EQ_INT(wait_receive(b, from, &message, data, sizeof data), 0x20);
    CHECK_EQ_INT(message, NET_SERVER_ASK);
    CHECK(strcmp((const char *)data, "EAGAME.0123") == 0);
}

int main(void)
{
    CHECK(net_udp_present() >= NET_UDP_MOST + 4);
    NetUdp *const a = net_udp_open(0), *const b = net_udp_open(0);
    CHECK(a != NULL && b != NULL);
    if (a != NULL && b != NULL) {
        CHECK(net_udp_port(a) != 0 && net_udp_port(a) != net_udp_port(b));
        test_exchange(a, b);
        test_link(a, b);
        test_find(a, b);
    }
    net_udp_close(a), net_udp_close(b);
    net_udp_close(NULL);
    return TEST_REPORT();
}
