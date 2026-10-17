/* The network transport's requests (net_request.h) */
#include "game/net/net_request.h"

#include "game/net/net_queue.h"

#include <stddef.h>

void net_request_send(const NetRequestOps *ops, NetSession *s, NetRequest *r, int32_t message, uint8_t *out,
                      int32_t size, int32_t timeout)
{
    r->session = s;
    const int32_t id = *ops->next_id;
    r->id = id;
    r->answered = 0;
    *ops->next_id = id + 1;
    net_put_big(out, (uint32_t)id, 4);
    if (*ops->trace_level >= 3) ops->trace(ops->ctx, "SendSrvReq", &message, out, size);
    if (ops->driver_prepare != NULL) ops->driver_prepare(ops->ctx, s->handle, s->address);
    s->request = r;
    *ops->deadline = (int32_t)((uint32_t)ops->clock->clock(ops->clock->ctx) + (uint32_t)timeout);
    while (r->answered == 0 && net_request_expired(ops->clock, ops->deadline) == 0) {
        const int32_t sent = ops->driver_send(ops->ctx, s->handle, s->address, message, out, size);
        net_wait_ticks(ops->wait, ops->wait_clock, sent != 0 ? *ops->resend : 1);
    }
    s->request = NULL;
}
