#include "game/frontend/net_host.h"

#include <stddef.h>

enum { NAME_X = 0x3a, TITLE_Y = 0xf8, FIRST_Y = 0x113, LINE = 0x12 };

/* 00453e40 */
void net_players_draw(int16_t german, const uint8_t *address, const NetPlayersDrawOps *ops)
{
    const RaceNetPlayers *const p = ops->players(ops->ctx, address);
    ops->text_select(ops->ctx, 2);
    ops->text_boxed(ops->ctx, german != 0 ? "Rennfahrer" : "Racers:", NAME_X, TITLE_Y);  /* 004b0e68 / 004b0e74 */
    ops->text_select(ops->ctx, 0);
    if (p == NULL) return;
    int32_t y = FIRST_Y;
    for (int32_t i = 0; i < p->count; i++, y += LINE)
        ops->text_boxed(ops->ctx, (const char *)p->list + i * RACE_NET_PLAYER_SIZE, NAME_X, y);
}
