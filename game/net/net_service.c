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

int32_t net_guid_equal(const uint8_t *a, const uint8_t *b)
{
    return memcmp(a, b, NET_GUID) == 0;
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
            if (cb->fn(cb->ctx, ops->drivers[i]->guid, ops->drivers[i]->name, a, b) == 0) return 0;
        }
    }
    if (ops->tcp_services == NULL) return 1;
    return ops->tcp_services(ops->ctx, cb, arg2, arg3, most);
}

static int32_t net_service_driver(const NetInitServiceOps *ops, NetSession *s, const uint8_t *service)
{
    int32_t i = 0;
    while (i < *ops->count && net_guid_equal(ops->drivers[i]->guid, service) == 0) i++;
    if (i >= *ops->count) return 0;
    s->tcp = 0;
    s->driver = ops->drivers[i];
    if (ops->driver_present(ops->ctx, s->driver) == 0) return 0;
    const uint32_t handle = ops->driver_open(ops->ctx, s->driver);
    s->handle = handle;
    if (handle == 0) {
        ops->driver_stop(ops->ctx, s->driver);
        return 0;
    }
    ops->driver_self(ops->ctx, s->driver, handle, s->self);
    s->word_a0 = 1;
    return 1;
}

int32_t net_init_service(const NetInitServiceOps *ops, const NetCheck *c, NetSession *s, const uint8_t *guid,
                         const uint8_t *service, const char *text_3c, const char *text_70)
{
    net_check(ops->check, c, "initnetservice");
    if (*ops->started == 0) {
        *ops->lock = ops->mutex->create(ops->mutex->ctx);
        net_queue_init(ops->mutex, ops->sessions, NULL, 0);
        ops->at_exit(ops->ctx);
        *ops->started = 1;
    }
    if (*ops->resend == 0) *ops->resend = (int32_t)((uint32_t)*ops->timeout + 9) / 10;
    ops->clear(ops->ctx, s);
    int32_t r;
    if (ops->tcp_init != NULL && ops->tcp_init(ops->ctx, s, guid, service, text_3c, text_70) != 0) r = 1;
    else r = net_service_driver(ops, s, service);
    if (r == 0) return 0;
    memmove(s->guid, guid, NET_GUID);  /* 0048ec60 */
    strncpy(s->text_3c, text_3c, NET_TEXT_3C);
    strncpy(s->text_70, text_70, NET_TEXT_70);
    if (net_queue_count(ops->sessions) == 0) ops->service_on(ops->ctx);
    net_queue_push_front(ops->mutex, ops->sessions, &s->node);
    return 1;
}

void net_service_exit(const NetKillServiceOps *ops, const NetCheck *c, void *const *lock)
{
    NetQueue *const q = ops->end->sessions;
    while (net_queue_count(q) != 0)
        net_kill_service(ops, c, (NetSession *)(void *)((char *)q->head - offsetof(NetSession, node)));
    net_queue_close(ops->end->mutex, q);
    ops->end->mutex->close(ops->end->mutex->ctx, *lock);
}
