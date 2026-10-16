/* The network transport's session (net_session.h) */
#include "game/net/net_session.h"

int32_t net_address_equal(const NetEqualOps *ops, NetSession *s, const uint8_t *a, const uint8_t *b)
{
    if (s->tcp != 0) return ops->tcp_equal(ops->ctx, s, a, b);
    return ops->driver_equal(ops->ctx, s->handle, a, b);
}
