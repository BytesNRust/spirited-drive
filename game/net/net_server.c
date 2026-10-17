/* The network transport's game server side (net_server.h) */
#include "game/net/net_server.h"

#include "game/net/net_queue.h"

#include <string.h>

const char NET_ADD_NO_SLOT[] = "addnewplayer -- FAILED TO FIND EMPTY PLAYER SLOT, SHOULD HAVE NEVER GOT THIS FAR.";

int32_t net_bit_count(uint32_t bits)
{
    int32_t n = 0;
    for (; bits != 0; bits &= ~(0u - bits)) n++;
    return n;
}

void net_game_info(const NetGameInfoOps *ops, NetServer *v, uint8_t *address, uint32_t id)
{
    NetSession *const s = v->session;
    if (id == 0 && v->word_8 != 0) return;
    int32_t message = NET_GAME_INFO;
    uint8_t out[NET_GAME_INFO_SIZE];
    net_put_big(out, id, 4);
    for (int i = 0; i < 5; i++) {
        const uint8_t *w = v->params + 0x30 + 4 * i;
        net_put_big(out + 0x38 + 4 * i, (uint32_t)w[0] | (uint32_t)w[1] << 8 | (uint32_t)w[2] << 16 | (uint32_t)w[3] << 24, 4);
    }
    net_put_big(out + 4, v->players, 4);
    strncpy((char *)out + 8, (const char *)v->params, 0x20);
    strncpy((char *)out + 0x28, (const char *)v->params + 0x20, 0x10);
    if (*ops->trace_level >= 3) ops->trace(ops->ctx, "GameInfo->", &message, out, NET_GAME_INFO_SIZE);
    ops->driver_send(ops->ctx, s->handle, address, message, out, NET_GAME_INFO_SIZE);
}

static uint32_t net_word(const uint8_t *b) { return (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24; }

uint32_t net_add_player(const NetAddPlayerOps *ops, NetServer *v, const uint8_t *record)
{
    for (uint32_t i = 0; i < net_word(v->params + 0x40) - 1; i++) {
        const uint32_t bit = 1u << (i & 31);
        if ((v->players & bit) != 0) continue;
        v->players |= bit;
        uint8_t *const e = v->entries + i * NET_PLAYER_SIZE;
        memcpy(e, record, NET_PLAYER_SEEN);
        net_put_little(e + NET_PLAYER_SEEN, (uint32_t)*ops->now, 4);
        net_put_little(e + NET_PLAYER_SEEN + 4, i, 4);
        return i;
    }
    ops->message(ops->ctx, NET_ADD_NO_SLOT);
    return 0;
}

static uint32_t net_big(const uint8_t *b) { return (uint32_t)b[0] << 24 | (uint32_t)b[1] << 16 | (uint32_t)b[2] << 8 | b[3]; }

static void serve_trace(const NetServeOps *ops, int32_t level, const char *name, const int32_t *message, uint8_t *out,
                        int32_t size)
{
    if (*ops->trace_level >= level) ops->trace(ops->ctx, name, message, out, size);
}

static int32_t serve_from_server(const NetServeOps *ops, NetSession *s, const uint8_t *address)
{
    return net_address_equal(ops->equal, s, address, s->address);
}

static void serve_game(const NetServeOps *ops, NetRequest *r, NetSession *s, uint8_t *address, const int32_t *message,
                       uint8_t *data, int32_t size)
{
    const uint32_t id = net_big(data);
    int32_t ours = id == 0 && r->id == 0 && r->games != NULL;
    if (ours == 0)
        ours = r->games == NULL && r->id != 0 && id == (uint32_t)r->id && serve_from_server(ops, s, address) != 0;
    if (ours == 0) return;
    if (s->word_9c != 0 && r->games == NULL) s->word_9c = *ops->stamp;
    uint8_t info[0x44];
    for (int i = 0; i < 5; i++) net_put_little(info + 0x30 + 4 * i, net_big(data + 0x38 + 4 * i), 4);
    strncpy((char *)info, (const char *)data + 8, 0x20);
    strncpy((char *)info + 0x20, (const char *)data + 0x28, 0x10);
    r->players = net_big(data + 4);
    r->answered = 1;
    serve_trace(ops, 3, "->GameInfo", message, data, size);
    if (r->games == NULL) return;
    if (r->games->fn(r->games->ctx, s, info, net_bit_count(net_word(data + 4)), address) == 0) r->games = NULL;
}

static void serve_player(const NetServeOps *ops, NetSession *s, uint8_t *address, const uint8_t *data)
{
    if (s->server == NULL) return;
    int32_t message = NET_PLAYER_INFO;
    int32_t size = 8 + NET_PLAYER_SEEN;
    uint8_t out[8 + NET_PLAYER_SEEN];
    memcpy(out, data, 8);
    const uint32_t id = net_big(data + 4);
    if (id == 0xffffffffu) {
        memcpy(out + 8, s->self, NET_PLAYER_SEEN);
    } else if ((s->server->players >> (id & 31) & 1) != 0) {
        memcpy(out + 8, s->server->entries + id * NET_PLAYER_SIZE, NET_PLAYER_SEEN);
    } else {
        message = NET_BAD_PLAYER;
        size = 8;
    }
    serve_trace(ops, 3, "PlyrInfo->", &message, out, size);
    ops->driver_send(ops->ctx, s->handle, address, message, out, size);
}

static void serve_player_answer(const NetServeOps *ops, NetRequest *r, NetSession *s, const uint8_t *address,
                                const int32_t *message, uint8_t *data, int32_t size)
{
    if (serve_from_server(ops, s, address) == 0) {
        serve_trace(ops, 3, "BadRspAddr", message, data, size);
        return;
    }
    if (s->word_9c != 0) s->word_9c = *ops->stamp;
    if (r->callback == NULL || net_big(data) != (uint32_t)r->id) {
        serve_trace(ops, 3, "BadPlyrReq", message, data, size);
        return;
    }
    serve_trace(ops, 3, "->PlyrInfo", message, data, size);
    if (*message == NET_PLAYER_INFO &&
        r->callback->fn(r->callback->ctx, r->session, data + 8, net_big(data + 4) == 0xffffffffu) == 0)
        r->callback = NULL;
    r->id = 0;
    r->answered = 1;
}

static void serve_response(const NetServeOps *ops, NetRequest *r, NetSession *s, const uint8_t *address,
                           const int32_t *message, uint8_t *data, int32_t size)
{
    if (serve_from_server(ops, s, address) == 0) {
        serve_trace(ops, 3, "BadSrvAddr", message, data, size);
        return;
    }
    if (net_big(data) != (uint32_t)r->id) {
        serve_trace(ops, 3, "BadSrvResp", message, data, size);
        return;
    }
    serve_trace(ops, 3, "->Response", message, data, size);
    r->answered = (int32_t)net_big(data + 4);
    r->id = 0;
    if (r->answered != 1) return;
    s->joined = *ops->now;
    s->word_9c = *ops->stamp;
}

/* The add's answer when the address is not a player yet: its name in the trace */
static const char *serve_join(const NetServeOps *ops, NetSession *s, const uint8_t *address, uint8_t *data, uint8_t *out)
{
    if ((uint32_t)net_bit_count(s->server->players) >= net_word(s->server->params + 0x40)) {
        net_put_big(out + 4, (uint32_t)-2, 4);
        return "GameFull->";
    }
    if (s->server->password[0] != 0 && strncmp(s->server->password, (const char *)data + 4, NET_PASSWORD) != 0) {
        net_put_big(out + 4, (uint32_t)-1, 4);
        return "BadPsswd->";
    }
    memcpy(data + 0x14, address, NET_ADDRESS);
    if (s->word_a8 != 0 && ops->accept(ops->ctx, s->word_a8, s, data + 0x14) == 0) {
        net_put_big(out + 4, (uint32_t)-3, 4);
        return "DenyJoin->";
    }
    net_put_big(out + 4, 1, 4);
    net_put_big(out + 8, net_add_player(ops->add, s->server, data + 0x14), 4);
    return "AddPlayr->";
}

static void serve_add(const NetServeOps *ops, NetSession *s, uint8_t *address, uint8_t *data)
{
    if (s->server == NULL || s->server->word_8 != 0) return;
    const uint8_t *const e = net_find_player(ops->equal, s->server, address, ops->now);
    int32_t message = NET_RESPONSE;
    uint8_t out[NET_ADD_ANSWER] = { 0 };
    memcpy(out, data, 4);
    if (e != NULL) {
        net_put_big(out + 4, 1, 4);
        net_put_big(out + 8, (uint32_t)((e - s->server->entries) / NET_PLAYER_SIZE), 4);
        serve_trace(ops, 4, "OldPlayr->", &message, out, NET_ADD_ANSWER);
    } else {
        const char *const name = serve_join(ops, s, address, data, out);
        serve_trace(ops, 3, name, &message, out, NET_ADD_ANSWER);
    }
    if (ops->driver_send(ops->ctx, s->handle, address, message, out, NET_ADD_ANSWER) == 0)
        serve_trace(ops, 3, "SrvFailRsp", &message, out, NET_ADD_ANSWER);
}

static void serve_ack(const NetServeOps *ops, NetSession *s, uint8_t *address, const int32_t *message)
{
    if (s->server != NULL) {
        const uint8_t *const e = net_find_player(ops->equal, s->server, address, ops->now);
        if (e != NULL && *ops->trace_level >= 4) {
            int32_t slot;
            memcpy(&slot, e + NET_PLAYER_SEEN + 4, 4);
            ops->trace(ops->ctx, "ClientAlive!", &slot, NULL, 0);
        }
        return;
    }
    if (s->joined == 0 || s->word_9c == 0 || serve_from_server(ops, s, address) == 0) return;
    s->joined = *ops->now;
    ops->driver_send(ops->ctx, s->handle, s->address, NET_ACK, NULL, 0);
    serve_trace(ops, 4, "ClientPulse!", message, NULL, 0);
}

static void serve_leave(const NetServeOps *ops, NetSession *s, uint8_t *address, const int32_t *message, uint8_t *data,
                        int32_t size)
{
    serve_trace(ops, 2, "->Leave", message, data, size);
    if (s->server == NULL) {
        s->joined = 0;
        s->word_9c = 0;
        return;
    }
    uint8_t *const e = net_find_player(ops->equal, s->server, address, ops->now);
    if (e == NULL) return;
    const int32_t slot = (int32_t)((e - s->server->entries) / NET_PLAYER_SIZE);
    s->server->players &= ~(1u << (slot & 31));
    net_put_little(e + NET_PLAYER_SEEN, 0, 4);
}

void net_serve(const NetServeOps *ops, NetRequest *r, uint8_t *address, int32_t message, uint8_t *data, int32_t size)
{
    NetSession *const s = r->session;
    switch (message) {
    case NET_GAME_INFO: serve_game(ops, r, s, address, &message, data, size); return;
    case NET_PLAYER_GET: serve_player(ops, s, address, data); return;
    case NET_PLAYER_INFO:
    case NET_BAD_PLAYER: serve_player_answer(ops, r, s, address, &message, data, size); return;
    case NET_RESPONSE: serve_response(ops, r, s, address, &message, data, size); return;
    case NET_PLAYERS_ASK:
        if (s != NULL && s->server != NULL) net_game_info(ops->info, s->server, address, net_big(data));
        return;
    case NET_SERVER_ASK:
        if (s != NULL && s->server != NULL && strncmp((const char *)s->server->game, (const char *)data, 0x20) == 0)
            net_game_info(ops->info, s->server, address, 0);
        return;
    case NET_SET_GAME: serve_add(ops, s, address, data); return;
    case NET_ACK: serve_ack(ops, s, address, &message); return;
    case NET_DATA:
        if (s->server != NULL) net_find_player(ops->equal, s->server, address, ops->now);
        else if (serve_from_server(ops, s, address) != 0 && s->word_9c != 0) s->word_9c = *ops->stamp;
        return;
    case NET_BYE:
    case NET_KILL: serve_leave(ops, s, address, &message, data, size); return;
    default: serve_trace(ops, 3, "BadMessage", &message, data, size); return;
    }
}

void net_receive(const NetReceiveOps *ops, NetSession *s, NetRequest *r)
{
    uint8_t address[NET_ADDRESS];
    uint8_t data[NET_RECEIVE_SIZE];
    for (;;) {
        int32_t message = NET_DATA;
        memcpy(address, s->self, NET_ADDRESS);
        const int32_t size = ops->driver_receive(ops->ctx, s->handle, address, &message, data, NET_RECEIVE_SIZE);
        if (message == 0) return;
        serve_trace(ops->serve, 5, "->NtwkRecv", &message, data, size);
        net_serve(ops->serve, r, address, message, data, size);
    }
}

static void poll_players(const NetPollOps *ops, NetSession *s, NetServer *v, int32_t pulse)
{
    for (uint32_t i = 0; i < net_word(v->params + 0x40); i++) {
        if ((v->players >> (i & 31) & 1) == 0) continue;
        uint8_t *const e = v->entries + i * NET_PLAYER_SIZE;
        const uint32_t seen = net_word(e + NET_PLAYER_SEEN);
        net_put_little(e + NET_PLAYER_SEEN, seen - 1, 4);
        if (seen == 0) {
            const int32_t slot = (int32_t)i;
            if (*ops->trace_level >= 2) ops->trace(ops->ctx, "PlayerTimeout", &slot, 1);
            v->players &= ~(1u << (i & 31));
        } else if (pulse != 0) {
            ops->driver_send(ops->ctx, s->handle, e, NET_ACK, NULL, 0);
        }
    }
}

int32_t net_poll_session(const NetPollOps *ops, NetSession *s)
{
    if (s->tcp != 0) return 1;
    if (s->word_a4 == 0) {
        NetRequest local = { 0 };  /* 00477b00's zero bytes */
        local.session = s;
        net_receive(ops->receive, s, s->request != NULL ? s->request : &local);
        if (s->joined != 0 && s->server == NULL) s->joined--;
    }
    NetServer *const v = s->server;
    if (v == NULL || v->word_8 != 0 || v->players == 0) return 1;
    const int32_t message = NET_ACK;
    int32_t pulse = 0;
    if (s->word_9c < 2) {
        s->word_9c = *ops->stamp;
        if (*ops->trace_level >= 3) ops->trace(ops->ctx, "ServerPulse!", &message, 0);
        pulse = 1;
    } else {
        s->word_9c--;
    }
    poll_players(ops, s, v, pulse);
    return 1;
}
