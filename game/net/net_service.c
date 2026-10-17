/* The network transport's services (net_service.h) */
#include "game/net/net_service.h"

#include "game/net/net_queue.h"
#include "game/net/net_request.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

const char NET_NEW_TWO[] = "newnetgame -- MAXPLAYERS CANNOT BE 2.";
const char NET_NEW_MOST[] = "newnetgame -- MAXPLAYERS CANNOT EXCEED %d.";

int32_t net_kill_service(const NetKillServiceOps *ops, const NetCheck *c, NetSession *s)
{
    net_check(ops->check, c, "killnetservice");
    if (net_queue_place(ops->end->mutex, ops->end->sessions, &s->node) == -1) return 0;
    int32_t r;
    if (s->tcp != 0) {
        r = ops->tcp_kill(ops->ctx, s);
    } else {
        if (s->server != NULL || s->word_9c != 0) net_end_game(ops->end, c, s);
        s->word_a0 = 0;
        while (s->word_a4 != 0) ops->wait->service(ops->wait->ctx);
        ops->driver_close(ops->ctx, s->handle);
        r = ops->driver_stop(ops->ctx);
    }
    net_queue_remove_locked(ops->end->mutex, ops->end->sessions, &s->node);
    if (net_queue_count(ops->end->sessions) == 0) ops->unservice(ops->ctx);
    return r;
}

void net_game_name(const uint8_t *guid, char *out)
{
    const uint32_t d = (uint32_t)guid[0] | (uint32_t)guid[1] << 8 | (uint32_t)guid[2] << 16 | (uint32_t)guid[3] << 24;
    snprintf(out, NET_GAME_NAME, "EAGAME.%08x.%04x%04x.%02x%02x%02x%02x.%02x%02x%02x%02x", d,
             (unsigned)(guid[4] | guid[5] << 8), (unsigned)(guid[6] | guid[7] << 8), guid[8], guid[9], guid[10],
             guid[11], guid[12], guid[13], guid[14], guid[15]);
}

static uint32_t net_most_players(const uint8_t *params)
{
    return (uint32_t)params[0x40] | (uint32_t)params[0x41] << 8 | (uint32_t)params[0x42] << 16 |
           (uint32_t)params[0x43] << 24;
}

int32_t net_new_game(const NetNewGameOps *ops, const NetCheck *c, NetSession *s, uint8_t *params,
                     const char *password, int32_t arg)
{
    net_check(ops->check, c, "newnetgame");
    if (s->tcp != 0) return ops->tcp_new(ops->ctx, s, params, password, arg);
    if (net_most_players(params) == 0) params[0x40] = NET_MOST_PLAYERS, params[0x41] = params[0x42] = params[0x43] = 0;
    if (s->joined != 0) {
        net_end_game(ops->end, c, s);
        s->joined = 0;
    }
    if (net_most_players(params) < 2) ops->message(ops->ctx, NET_NEW_TWO, 0);
    if (net_most_players(params) > NET_MOST_PLAYERS) ops->message(ops->ctx, NET_NEW_MOST, NET_MOST_PLAYERS);
    const uint32_t size = (net_most_players(params) - 2) * NET_PLAYER_SIZE + 0xf8;
    s->server = ops->alloc(ops->ctx, "netserv", size, *ops->tag);
    if (s->server == NULL) return 0;
    s->server->session = s;
    memcpy(s->server->params, params, NET_GAME_PARAMS);
    s->server->word_8 = 0;
    s->server->players = 0;
    s->joined = *ops->now;
    s->word_a8 = arg;
    s->word_9c = *ops->stamp;
    memcpy(s->address, s->self, NET_ADDRESS);
    strncpy(s->server->password, password, NET_PASSWORD);  /* 00482890: strncpy's padding too */
    net_game_name(s->guid, (char *)s->server->game);
    if (ops->driver_open(ops->ctx, s->handle, s->server->game) != 0) return 1;
    ops->release(ops->ctx, s->server);
    s->server = NULL;
    return 0;
}

int32_t net_shut_game(const NetShutGameOps *ops, const NetCheck *c, NetSession *s, int32_t release)
{
    net_check(ops->check, c, "shutnetgame");
    if (s->tcp != 0) return ops->tcp_shut(ops->ctx, s, release);
    if (s->server == NULL) return 0;
    if (s->server->word_8 == 0) {
        ops->driver_leave(ops->ctx, s->handle, s->server->game);
        s->server->game[0] = 0;
        s->server->word_8 = 1;
    }
    if (release != 0) {
        ops->release(ops->ctx, s->server);
        s->server = NULL;
    }
    return 1;
}

int32_t net_open_game(const NetOpenGameOps *ops, const NetCheck *c, NetSession *s)
{
    net_check(ops->check, c, "opennetgame");
    if (s->tcp != 0) return ops->tcp_open(ops->ctx, s);
    if (s->server == NULL || s->server->word_8 == 0) return 0;
    s->server->word_8 = 1;
    net_game_name(s->guid, (char *)s->server->game);
    if (ops->driver_open(ops->ctx, s->handle, s->server->game) == 0) {
        s->server->word_8 = 0;
        return 0;
    }
    const int32_t now = *ops->now;
    s->word_9c = 1;
    for (uint32_t i = 0; i < net_most_players(s->server->params); i++)
        net_put_little(s->server->entries + i * NET_PLAYER_SIZE + NET_PLAYER_SEEN, (uint32_t)now, 4);
    return 1;
}

int32_t net_get_services(const NetServicesOps *ops, const NetCheck *c, const NetServicesCallback *cb, int32_t arg2,
                         int32_t arg3, uint32_t most)
{
    net_check(ops->check, c, "getnetservices");
    if (most <= NET_SERVICES_MOST) {
        for (int32_t i = 0; i < *ops->count; i++) {
            NetServiceDriver *const d = ops->drivers[i];
            int32_t a, b;
            if (ops->driver_info(ops->ctx, d, &a, &b) == 0) continue;
            if (cb->fn(cb->ctx, ops->drivers[i]->name, ops->drivers[i]->id, a, b) == 0) return 0;
        }
    }
    if (ops->tcp_services == NULL) return 1;
    return ops->tcp_services(ops->ctx, cb, arg2, arg3, most);
}
