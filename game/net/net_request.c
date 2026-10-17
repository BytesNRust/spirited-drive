/* The network transport's requests (net_request.h) */
#include "game/net/net_request.h"

#include "game/net/net_queue.h"
#include "game/net/net_service.h"

#include <stddef.h>
#include <string.h>

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

int32_t net_set_game(const NetSetOps *ops, const NetCheck *c, NetSession *s, const uint8_t *address,
                     const char *password)
{
    net_check(ops->check, c, "setnetgame");
    if (s->tcp != 0) return ops->tcp_set(ops->ctx, s, address, password);
    NetRequest r = { 0 };  /* 00477b30's zero bytes */
    if (s->joined != 0) {
        if (net_address_equal(ops->equal, s, s->address, address) != 0) return 1;
        net_end_game(ops->end, c, s);
        s->joined = 0;
    }
    uint8_t out[NET_SET_SIZE];
    memcpy(out + 0x14, s->self, NET_RECORD);
    memcpy(s->address, address, NET_ADDRESS);
    strncpy((char *)out + 4, password, NET_PASSWORD);  /* 00482890: strncpy's padding too */
    net_request_send(ops->request, s, &r, NET_SET_GAME, out, NET_SET_SIZE, (int32_t)((uint32_t)*ops->timeout * 2));
    return r.answered;
}

static int32_t net_server_players(const NetPlayerCallback *cb, NetSession *s)
{
    if (cb->fn(cb->ctx, s, s->self, 1) == 0) return 0;
    int32_t count = 1;
    for (int32_t i = 0; i < NET_PLAYERS; i++) {
        if ((s->server->players & 1u << i) == 0) continue;
        if (cb->fn(cb->ctx, s, s->server->entries + i * NET_PLAYER_SIZE, 0) == 0) return count;
        count++;
    }
    return count;
}

int32_t net_get_players(const NetPlayersOps *ops, const NetCheck *c, NetSession *s, const uint8_t *address,
                        const NetPlayerCallback *cb)
{
    net_check(ops->check, c, "getnetplayers");
    if (s->tcp != 0) return ops->tcp_players(ops->ctx, s, address, cb);
    if (address == NULL && s->joined != 0) {
        address = s->address;
    } else {
        if (address == NULL || s->joined != 0) return 0;
        memcpy(s->address, address, NET_ADDRESS);
    }
    if (s->server != NULL && net_address_equal(ops->equal, s, s->address, address) != 0) return net_server_players(cb, s);
    NetRequest r = { 0 };  /* 00477b48's zero bytes */
    uint8_t out[NET_SET_SIZE];
    net_request_send(ops->request, s, &r, NET_PLAYERS_ASK, out, NET_PLAYERS_SIZE,
                     (int32_t)((uint32_t)*ops->timeout * 2));
    if (r.answered == 0) return 0;
    const uint32_t players = r.players;
    r.callback = cb;
    r.answered = 1;
    int32_t count = 0;
    for (int32_t i = -1; r.answered != 0;) {
        if (i == -1 || (players & 1u << i) != 0) {
            net_put_big(out + 4, (uint32_t)i, 4);
            net_request_send(ops->request, s, &r, NET_PLAYER_GET, out, NET_PLAYER_SIZE_OUT,
                             (int32_t)((uint32_t)*ops->timeout * 2));
            if (r.answered == -3) r.answered = 0;
            else count++;
        }
        if (++i >= 32) return count;
    }
    return count;
}

int32_t net_get_games(const NetGamesOps *ops, const NetCheck *c, NetSession *s, const void *callback,
                      int32_t timeout)
{
    net_check(ops->check, c, "getnetgames");
    if (s->tcp != 0) return ops->tcp_games(ops->ctx, s, callback, timeout);
    char name[48];
    NetRequest r = { 0 };  /* 00477b18's zero bytes */
    net_game_name(s->guid, name);
    r.session = s;
    r.id = 0;
    r.games = callback;
    s->request = &r;
    *ops->deadline = (int32_t)((uint32_t)ops->clock->clock(ops->clock->ctx) + (uint32_t)timeout);
    int32_t found = 0;
    do {
        if (found == 0) found = ops->driver_find(ops->ctx, s->handle, name);
        net_wait_ticks(ops->wait, ops->wait_clock, 1);
    } while (net_request_expired(ops->clock, ops->deadline) == 0 && r.games != NULL);
    if (found != 0) ops->driver_stop_find(ops->ctx, s->handle);
    s->request = NULL;
    return found;
}
