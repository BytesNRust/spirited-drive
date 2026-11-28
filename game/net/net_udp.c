/* The port's UDP network driver (net_udp.h): POSIX sockets, Winsock on Windows */
#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>   /* before anything that brings windows.h */
#include <ws2tcpip.h>
#include <iphlpapi.h>
#endif
#include "game/net/net_udp.h"

#include "game/net/net_queue.h"
#include "game/net/net_server.h"
#include "game/sys/port_log.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__EMSCRIPTEN__)

/* No UDP sockets in a browser */
int32_t net_udp_present(void) { return 0; }
NetUdp *net_udp_open(uint16_t port) { return (void)port, NULL; }
void net_udp_close(NetUdp *u) { (void)u; }
uint16_t net_udp_port(const NetUdp *u) { return (void)u, 0; }
int32_t net_udp_add_host(NetUdp *u, const char *host) { return (void)u, (void)host, 0; }
int32_t net_udp_find(NetUdp *u, const char *name) { return (void)u, (void)name, 0; }
int32_t net_udp_equal(const uint8_t *a, const uint8_t *b) { return memcmp(a, b, 6) == 0; }
int32_t net_udp_self(const NetUdp *u, uint8_t *out) { return (void)u, memset(out, 0, NET_UDP_ADDRESS), 0; }
int32_t net_udp_query(NetUdp *u) { return (void)u, 0; }
int32_t net_udp_send(NetUdp *u, const uint8_t *address, int32_t message, const uint8_t *data, int32_t size)
{
    return (void)u, (void)address, (void)message, (void)data, (void)size, 0;
}
int32_t net_udp_receive(NetUdp *u, uint8_t *address, int32_t *message, uint8_t *data, int32_t size)
{
    return (void)u, (void)address, (void)data, (void)size, *message = 0, 0;
}
int32_t net_udp_link(NetUdp *u, const uint8_t *address, int32_t message, int32_t closing)
{
    return (void)u, (void)address, (void)message, (void)closing, 1;
}

#else

#ifndef _WIN32
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

enum { DATAGRAM = 4 + NET_UDP_MOST, NAME = 0x20 };

typedef struct {
    uint8_t from[NET_UDP_ADDRESS];
    int32_t message;
    int32_t size;  /* the data's */
    uint8_t data[NET_UDP_MOST];
} Held;

#ifdef _WIN32

typedef SOCKET Socket;
#define NO_SOCKET INVALID_SOCKET
#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif

/* Winsock counts these: one sock_stop for each sock_start that succeeded */
static int sock_start(void)
{
    WSADATA data;
    return WSAStartup(MAKEWORD(2, 2), &data) == 0;
}

static void sock_stop(void) { WSACleanup(); }
static void sock_close(Socket s) { closesocket(s); }

static const char *sock_error(void)
{
    static char text[32];
    const int e = WSAGetLastError();
    if (e == WSAEADDRINUSE) return "Address already in use";   /* the words POSIX gives, which docs/online.md names */
    snprintf(text, sizeof text, "Winsock error %d", e);
    return text;
}

static int sock_nonblock(Socket s)
{
    u_long on = 1;
    return ioctlsocket(s, FIONBIO, &on) == 0;
}

/* Windows reports a datagram sent to a port nobody listens on as an error of the next receive (a player that left): off */
static void sock_ignore_unreachable(Socket s)
{
    BOOL off = FALSE;
    DWORD n = 0;
    WSAIoctl(s, SIO_UDP_CONNRESET, &off, sizeof off, NULL, 0, &n, NULL, NULL);
}

/* The next datagram's length (a longer one than the buffer cut to it, as POSIX does), or -1 with none */
static int sock_receive(Socket s, uint8_t *buf, int size, struct sockaddr_in *from)
{
    socklen_t n = sizeof *from;
    const int got = recvfrom(s, (char *)buf, size, 0, (struct sockaddr *)from, &n);
    return got < 0 && WSAGetLastError() == WSAEMSGSIZE ? size : got;
}

#else

typedef int Socket;
#define NO_SOCKET (-1)

static int sock_start(void) { return 1; }
static void sock_stop(void) {}
static void sock_close(Socket s) { close(s); }
static const char *sock_error(void) { return strerror(errno); }
static int sock_nonblock(Socket s) { return fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK) == 0; }
static void sock_ignore_unreachable(Socket s) { (void)s; }

static int sock_receive(Socket s, uint8_t *buf, int size, struct sockaddr_in *from)
{
    socklen_t n = sizeof *from;
    return (int)recvfrom(s, buf, (size_t)size, 0, (struct sockaddr *)from, &n);
}

#endif

struct NetUdp {
    Socket fd;
    uint16_t port;                    /* host order */
    uint8_t self[NET_UDP_ADDRESS];
    uint8_t hosts[NET_UDP_HOSTS][NET_UDP_ADDRESS];
    int hosts_used;
    Held held[NET_UDP_QUEUE];         /* oldest first */
    int held_used;
};

static void address_of(const struct sockaddr_in *in, uint8_t *out)
{
    memset(out, 0, NET_UDP_ADDRESS);
    memcpy(out, &in->sin_addr.s_addr, 4);
    memcpy(out + 4, &in->sin_port, 2);
}

static struct sockaddr_in sockaddr_of(const uint8_t *address)
{
    struct sockaddr_in in;
    memset(&in, 0, sizeof in);
    in.sin_family = AF_INET;
    memcpy(&in.sin_addr.s_addr, address, 4);
    memcpy(&in.sin_port, address + 4, 2);
    return in;
}

/* The LAN address a datagram to the internet would leave from (connect on a
 * UDP socket sends nothing), else the loopback */
static uint32_t local_ip(void)
{
    uint32_t ip = htonl(INADDR_LOOPBACK);
    const Socket fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == NO_SOCKET) return ip;
    struct sockaddr_in to = { .sin_family = AF_INET, .sin_port = htons(9) };
    to.sin_addr.s_addr = htonl(0x08080808);
    struct sockaddr_in me;
    socklen_t n = sizeof me;
    if (connect(fd, (const struct sockaddr *)&to, sizeof to) == 0 &&
        getsockname(fd, (struct sockaddr *)&me, &n) == 0 && me.sin_addr.s_addr != 0)
        ip = me.sin_addr.s_addr;
    sock_close(fd);
    return ip;
}

int32_t net_udp_present(void) { return DATAGRAM; }

NetUdp *net_udp_open(uint16_t port)
{
    NetUdp *const u = calloc(1, sizeof *u);
    if (u == NULL) return NULL;
    if (!sock_start()) {
        port_log("net_udp_open: no sockets\n");
        free(u);
        return NULL;
    }
    const int on = 1;
    struct sockaddr_in in = { .sin_family = AF_INET, .sin_port = htons(port) };
    in.sin_addr.s_addr = htonl(INADDR_ANY);
    socklen_t n = sizeof in;
    u->fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (u->fd == NO_SOCKET || setsockopt(u->fd, SOL_SOCKET, SO_BROADCAST, (const void *)&on, sizeof on) != 0 ||
        !sock_nonblock(u->fd) ||
        bind(u->fd, (const struct sockaddr *)&in, sizeof in) != 0 ||
        getsockname(u->fd, (struct sockaddr *)&in, &n) != 0) {
        port_log("net_udp_open: port %u: %s\n", (unsigned)port, sock_error());
        if (u->fd != NO_SOCKET) sock_close(u->fd);
        sock_stop();
        free(u);
        return NULL;
    }
    sock_ignore_unreachable(u->fd);
    u->port = ntohs(in.sin_port);
    in.sin_addr.s_addr = local_ip();
    address_of(&in, u->self);
    return u;
}

void net_udp_close(NetUdp *u)
{
    if (u == NULL) return;
    sock_close(u->fd);
    sock_stop();
    free(u);
}

uint16_t net_udp_port(const NetUdp *u) { return u->port; }

int32_t net_udp_add_host(NetUdp *u, const char *host)
{
    char ip[64];
    unsigned port = 0;
    struct sockaddr_in in = { .sin_family = AF_INET };
    if (u->hosts_used == NET_UDP_HOSTS || sscanf(host, "%63[^:]:%u", ip, &port) != 2 || port == 0 || port > 0xffff ||
        inet_pton(AF_INET, ip, &in.sin_addr) != 1)
        return 0;
    in.sin_port = htons((uint16_t)port);
    address_of(&in, u->hosts[u->hosts_used++]);
    return 1;
}

static int32_t send_to(const NetUdp *u, const uint8_t *address, const uint8_t *datagram, size_t n)
{
    const struct sockaddr_in to = sockaddr_of(address);
    return sendto(u->fd, (const void *)datagram, (int)n, 0, (const struct sockaddr *)&to, sizeof to) == (int)n;
}

/* The datagram to each broadcasting interface's own broadcast address, on the socket's port: 1 when one took it */
#ifdef _WIN32
static int32_t send_to_each_interface(const NetUdp *u, const uint8_t *datagram, size_t n)
{
    const ULONG skip = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    ULONG size = 16 * 1024, got = ERROR_BUFFER_OVERFLOW;
    IP_ADAPTER_ADDRESSES *list = NULL;
    for (int again = 0; got == ERROR_BUFFER_OVERFLOW && again < 3; again++) {   /* `size` then says what it needs */
        free(list);
        list = malloc(size);
        if (list == NULL) return 0;
        got = GetAdaptersAddresses(AF_INET, skip, NULL, list, &size);
    }
    int32_t sent = 0;
    for (const IP_ADAPTER_ADDRESSES *a = got == NO_ERROR ? list : NULL; a != NULL; a = a->Next) {
        if (a->OperStatus != IfOperStatusUp || a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        for (const IP_ADAPTER_UNICAST_ADDRESS *i = a->FirstUnicastAddress; i != NULL; i = i->Next) {
            if (i->Address.lpSockaddr->sa_family != AF_INET || i->OnLinkPrefixLength < 1 || i->OnLinkPrefixLength > 30) continue;
            struct sockaddr_in to;
            memcpy(&to, i->Address.lpSockaddr, sizeof to);
            to.sin_addr.s_addr |= htonl(0xffffffffu >> i->OnLinkPrefixLength);
            to.sin_port = htons(u->port);
            uint8_t address[NET_UDP_ADDRESS];
            address_of(&to, address);
            if (send_to(u, address, datagram, n)) sent = 1;
        }
    }
    free(list);
    return sent;
}
#else
static int32_t send_to_each_interface(const NetUdp *u, const uint8_t *datagram, size_t n)
{
    enum { BROADCASTING = IFF_UP | IFF_BROADCAST };
    struct ifaddrs *list = NULL;
    int32_t sent = 0;
    if (getifaddrs(&list) == 0) {
        for (const struct ifaddrs *i = list; i != NULL; i = i->ifa_next) {
            if (i->ifa_addr == NULL || i->ifa_addr->sa_family != AF_INET || i->ifa_broadaddr == NULL ||
                (i->ifa_flags & BROADCASTING) != BROADCASTING)
                continue;
            struct sockaddr_in to;
            memcpy(&to, i->ifa_broadaddr, sizeof to);
            to.sin_port = htons(u->port);
            if (sendto(u->fd, datagram, n, 0, (const struct sockaddr *)&to, sizeof to) == (ssize_t)n) sent = 1;
        }
        freeifaddrs(list);
    }
    return sent;
}
#endif

/* The LAN's broadcast on its own port: to each broadcasting interface's own
 * address (macOS routes nothing to 255.255.255.255), else to 255.255.255.255 */
static int32_t send_broadcast(const NetUdp *u, const uint8_t *datagram, size_t n)
{
    if (send_to_each_interface(u, datagram, n)) return 1;
    uint8_t lan[NET_UDP_ADDRESS];
    struct sockaddr_in in = { .sin_family = AF_INET, .sin_port = htons(u->port) };
    in.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    address_of(&in, lan);
    return send_to(u, lan, datagram, n);
}

/* The LAN's broadcast, then each direct host */
static int32_t send_everywhere(const NetUdp *u, const uint8_t *datagram, size_t n)
{
    int32_t all = send_broadcast(u, datagram, n);
    for (int i = 0; i < u->hosts_used; i++) all &= send_to(u, u->hosts[i], datagram, n);
    return all;
}

int32_t net_udp_send(NetUdp *u, const uint8_t *address, int32_t message, const uint8_t *data, int32_t size)
{
    uint8_t datagram[DATAGRAM];
    const int32_t n = size < 0 ? 0 : size > NET_UDP_MOST ? NET_UDP_MOST : size;
    net_put_big(datagram, (uint32_t)message, 4);
    if (n > 0) memcpy(datagram + 4, data, (size_t)n);
    return address == NULL ? send_everywhere(u, datagram, (size_t)n + 4) : send_to(u, address, datagram, (size_t)n + 4);
}

int32_t net_udp_find(NetUdp *u, const char *name)
{
    uint8_t asked[NAME] = { 0 };
    strncpy((char *)asked, name, NAME);  /* 0x20 bytes, as net_serve compares them (no NUL past a long name) */
    net_udp_send(u, NULL, NET_SERVER_ASK, asked, NAME);
    return 1;
}

int32_t net_udp_equal(const uint8_t *a, const uint8_t *b) { return memcmp(a, b, 6) == 0; }

int32_t net_udp_self(const NetUdp *u, uint8_t *out)
{
    memcpy(out, u->self, NET_UDP_ADDRESS);
    return 1;
}

/* What waits on the socket into the held queue (the oldest dropped when full) */
static void drain(NetUdp *u)
{
    for (;;) {
        uint8_t datagram[DATAGRAM + 1];
        struct sockaddr_in from;
        const int got = sock_receive(u->fd, datagram, (int)sizeof datagram, &from);
        if (got < 0) return;
        if (got < 4) continue;
        if (u->held_used == NET_UDP_QUEUE) memmove(u->held, u->held + 1, sizeof(Held) * --u->held_used);
        Held *const h = &u->held[u->held_used++];
        address_of(&from, h->from);
        h->message = (int32_t)((uint32_t)datagram[0] << 24 | (uint32_t)datagram[1] << 16 | (uint32_t)datagram[2] << 8 |
                               datagram[3]);
        h->size = (int32_t)(got - 4 > NET_UDP_MOST ? NET_UDP_MOST : got - 4);
        memcpy(h->data, datagram + 4, (size_t)h->size);
    }
}

int32_t net_udp_query(NetUdp *u)
{
    drain(u);
    return u->held_used > 0;
}

/* The oldest held datagram, but for a link's receive (`address` its peer, not this socket's own) one socket serves
 * every link: another peer's data stays held for that peer's link (its other messages are served by whoever asks) */
int32_t net_udp_receive(NetUdp *u, uint8_t *address, int32_t *message, uint8_t *data, int32_t size)
{
    drain(u);
    const int any = net_udp_equal(address, u->self);
    int i = 0;
    while (i < u->held_used && !any && u->held[i].message == NET_DATA && !net_udp_equal(u->held[i].from, address)) i++;
    if (i == u->held_used) return *message = 0, 0;
    const Held *const h = &u->held[i];
    const int32_t n = size < 0 ? 0 : h->size < size ? h->size : size;
    memcpy(address, h->from, NET_UDP_ADDRESS);
    *message = h->message;
    if (n > 0) memcpy(data, h->data, (size_t)n);
    memmove(u->held + i, u->held + i + 1, sizeof(Held) * (size_t)(--u->held_used - i));
    return n;
}

int32_t net_udp_link(NetUdp *u, const uint8_t *address, int32_t message, int32_t closing)
{
    (void)closing;
    drain(u);
    int kept = 0;
    for (int i = 0; i < u->held_used; i++)
        if (!(net_udp_equal(u->held[i].from, address) && u->held[i].message == message)) u->held[kept++] = u->held[i];
    u->held_used = kept;
    return 1;
}

#endif
