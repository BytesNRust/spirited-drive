/* The UDP driver as a network service's driver (net_udp_service.h) */
#include "game/net/net_udp_service.h"

#include <stddef.h>

static const uint8_t UDP_GUID[16] = { 'T', 'N', 'F', 'S', ' ', 'U', 'D', 'P', ' ', 'L', 'A', 'N', '/', 'I', 'P', 0 };

const NetServiceDriver NET_UDP_DRIVER = { 1, UDP_GUID };

uint32_t net_udp_service_open(NetUdpService *s)
{
    for (uint32_t i = 0; i < NET_UDP_HANDLES; i++) {
        if (s->open[i] != NULL) continue;
        NetUdp *const u = net_udp_open(s->port);
        if (u == NULL) return 0;
        for (const char *const *h = s->hosts; h != NULL && *h != NULL; h++) net_udp_add_host(u, *h);
        s->open[i] = u;
        return i + 1;
    }
    return 0;
}

NetUdp *net_udp_service_get(const NetUdpService *s, uint32_t handle)
{
    return handle == 0 || handle > NET_UDP_HANDLES ? NULL : s->open[handle - 1];
}

void net_udp_service_close(NetUdpService *s, uint32_t handle)
{
    NetUdp *const u = net_udp_service_get(s, handle);
    if (u == NULL) return;
    net_udp_close(u);
    s->open[handle - 1] = NULL;
}

static NetUdp *udp_of(void *ctx, uint32_t handle) { return net_udp_service_get(ctx, handle); }

int32_t net_udp_service_send(void *ctx, uint32_t handle, uint8_t *address, int32_t message, uint8_t *data,
                             int32_t size)
{
    NetUdp *const u = udp_of(ctx, handle);
    return u == NULL ? 0 : net_udp_send(u, address, message, data, size);
}

void net_udp_service_send_word(void *ctx, uint32_t handle, uint8_t *address, int32_t message, int32_t ecx,
                               int32_t arg)
{
    (void)ecx, (void)arg;
    net_udp_service_send(ctx, handle, address, message, NULL, 0);
}

int32_t net_udp_service_receive(void *ctx, uint32_t handle, uint8_t *address, int32_t *message, uint8_t *data,
                                int32_t size)
{
    NetUdp *const u = udp_of(ctx, handle);
    if (u == NULL) return *message = 0, 0;
    return net_udp_receive(u, address, message, data, size);
}

int32_t net_udp_service_equal(void *ctx, uint32_t handle, const uint8_t *a, const uint8_t *b)
{
    (void)ctx, (void)handle;
    return net_udp_equal(a, b);
}

int32_t net_udp_service_query(void *ctx, uint32_t handle)
{
    NetUdp *const u = udp_of(ctx, handle);
    return u == NULL ? 0 : net_udp_query(u);
}

int32_t net_udp_service_ready(void *ctx, uint32_t handle) { return udp_of(ctx, handle) != NULL; }

void net_udp_service_link(void *ctx, uint32_t handle, uint8_t *address, int32_t message, int32_t closing)
{
    NetUdp *const u = udp_of(ctx, handle);
    if (u != NULL) net_udp_link(u, address, message, closing);
}

int32_t net_udp_service_find(void *ctx, uint32_t handle, const char *name)
{
    NetUdp *const u = udp_of(ctx, handle);
    return u == NULL ? 0 : net_udp_find(u, name);
}
