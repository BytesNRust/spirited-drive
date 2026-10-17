/* The network menu's list of games (race_net_games.h) */
#include "game/race/race_net_games.h"

#include <stddef.h>
#include <string.h>

const char RACE_NET_GAME_LIST[] = "game";
const char RACE_NET_PLAY[] = "Play";
const char RACE_NET_VIEW_ONLY[] = "ViewOnly";
const char RACE_NET_INIT_FAILED[] = "BUG: init network service FAILED";

int32_t race_net_game_found(void *ctx, NetSession *s, const uint8_t *info, int32_t players, uint8_t *address)
{
    const RaceNetGamesFound *const f = ctx;
    RaceNetGames *const g = f->games;
    (void)players;
    if (g->count >= RACE_NET_GAMES_MOST) return 0;
    for (int32_t i = 0; i < g->count; i++)
        if (f->ops->same(f->ops->ctx, s, address, g->list + i * RACE_NET_GAME_SIZE + RACE_NET_GAME_ADDRESS) != 0)
            return 1;
    strcpy((char *)g->list + g->count * RACE_NET_GAME_SIZE, (const char *)info);
    strcpy((char *)g->list + g->count * RACE_NET_GAME_SIZE + RACE_NET_GAME_TEXT, (const char *)info + RACE_NET_GAME_TEXT);
    memcpy(g->list + g->count * RACE_NET_GAME_SIZE + RACE_NET_GAME_INFO, info + RACE_NET_GAME_INFO, 0x10);
    memcpy(g->list + g->count * RACE_NET_GAME_SIZE + RACE_NET_GAME_ADDRESS, address, RACE_NET_GAME_ADDRESS_BYTES);
    g->count = (int16_t)(g->count + 1);
    return 1;
}

/* the picked service's GUID; its block may be gone, so the address is only reckoned */
static const uint8_t *service_guid(const RaceNetServices *s)
{
    return (const uint8_t *)((uintptr_t)s->list + (uintptr_t)(intptr_t)(s->count * RACE_NET_SERVICE_SIZE + RACE_NET_SERVICE_GUID));
}

static void swap(uint8_t *a, uint8_t *b)
{
    uint8_t t[RACE_NET_GAME_SIZE];
    memcpy(t, a, sizeof t);
    memcpy(a, b, sizeof t);
    memcpy(b, t, sizeof t);
}

/* by name, the first of the smallest to the front each round */
static void sort_by_name(RaceNetGames *g)
{
    const int32_t n = g->count;
    for (int32_t i = 0; i < n; i++) {
        int32_t least = i;
        for (int32_t j = i; j < n; j++)
            if (strcmp((const char *)g->list + least * RACE_NET_GAME_SIZE, (const char *)g->list + j * RACE_NET_GAME_SIZE) > 0)
                least = j;
        if (least != i) swap(g->list + i * RACE_NET_GAME_SIZE, g->list + least * RACE_NET_GAME_SIZE);
    }
}

RaceNetGames *race_net_games_list(const RaceNetGamesOps *ops, RaceNetGamesState *st, int32_t timeout)
{
    RaceNetServices *const svc = st->services;
    if (svc->open != 0 && st->view != st->open_view) {
        svc->open = 0;
        ops->kill_service(ops->ctx);
    }
    if (svc->open == 0) {
        if (ops->init_service(ops->ctx, service_guid(svc), st->view != 0 ? RACE_NET_VIEW_ONLY : RACE_NET_PLAY) == 0) {
            ops->message(ops->ctx, RACE_NET_INIT_FAILED);
            return NULL;
        }
        svc->open = 1;
        st->open_view = st->view;
    }
    RaceNetGames *const g = &st->games;
    if (g->list == NULL) g->list = ops->alloc(ops->ctx, RACE_NET_GAME_LIST, RACE_NET_GAMES_BYTES, 0);
    g->count = 0;
    st->key = 0;
    ops->wait_keys_on(ops->ctx);
    const RaceNetGamesFound found = { ops, g };
    const NetGamesCallback cb = { (void *)&found, race_net_game_found };
    const int32_t got = ops->games(ops->ctx, &cb, timeout);
    ops->wait_keys_off(ops->ctx);
    if (got != 0 && g->count > 1) sort_by_name(g);
    return g;
}
