/* The network menu's New (race_net_host.h) */
#include "game/race/race_net_host.h"

#include <string.h>

const char RACE_NET_CREATED[] = "Game Created";
const char RACE_NET_CREATED_DE[] = "Spiel erstellt";
const char RACE_NET_CREATE_FAILED[] = "Create Game Failed";
const char RACE_NET_CREATE_FAILED_DE[] = "Spiel konnte nicht erstellt werden";

enum { FAILED_TIME = 300, CREATED_TIME = 200 };

int32_t race_net_host(const RaceNetHostOps *ops, RaceNetHostState *st, const char *name)
{
    const char password[1] = { 0 };  /* 00442510's byte */
    st->w524e7b = 1;
    st->link_lost = 0;
    st->w4f81d6 = 0;
    st->w4f81d2 = 1;
    st->w4bc6d8 = 1;
    st->w4bc6da = 0;
    st->w4fbb0c = 1;
    st->w50a5a8 = 0;
    st->w4f81d4 = 0;
    memset(st->params + RACE_NET_PARAMS_TAIL, 0, RACE_NET_PARAMS_BYTES - RACE_NET_PARAMS_TAIL);
    strcpy(st->names, name);
    const int32_t most = RACE_NET_PLAYERS_MOST;
    memcpy(st->params + RACE_NET_PARAMS_MOST, &most, 4);
    strcpy((char *)st->params, name);
    ops->prepare(ops->ctx);
    if (ops->new_game(ops->ctx, st->params, password) == 0) {
        ops->banner_wait(ops->ctx, st->german != 0 ? RACE_NET_CREATE_FAILED_DE : RACE_NET_CREATE_FAILED, FAILED_TIME);
        ops->close(ops->ctx);
        return 0;
    }
    st->hosting = 1;
    st->link_game = 1;
    ops->free_lists(ops->ctx);
    ops->banner_wait(ops->ctx, st->german != 0 ? RACE_NET_CREATED_DE : RACE_NET_CREATED, CREATED_TIME);
    return 1;
}
