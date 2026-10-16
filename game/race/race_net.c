/* The race's side of the network game (race_net.h). */
#include "game/race/race_net.h"

#include <string.h>

typedef char race_net_link_size_check[sizeof(RaceNetLink) == 0x54 ? 1 : -1];

int32_t race_net_wait_step(const RaceNetOps *ops, const RaceNetWords *w)
{
    ops->keys(ops->ctx);
    if (w->dropped != 0) return 1;
    if (w->deadline != RACE_NET_NO_DEADLINE && w->deadline < w->clock) return 1;
    ops->service(ops->ctx);
    return 0;
}

void race_net_count_down(RaceNetCounts *c, int32_t kind)
{
    if (kind != 0) c->unknown_4bc6d8 = (int16_t)(c->unknown_4bc6d8 - 1);
    else c->unknown_4bc6da = (int16_t)(c->unknown_4bc6da - 1);
}

int32_t race_net_slot_of(const RaceNetSlots *s, uint32_t link)
{
    const int32_t n = (int32_t)s->unknown_4f81d2 + s->unknown_4f81d4;
    for (int32_t i = 1; i < n && i < RACE_NET_LINKS; i++)  /* port guard: the original reads on past 8 */
        if (s->links[i] == link) return i;
    return RACE_NET_NO_SLOT;
}

RaceNetLink *race_net_link_fill(RaceNetLink entries[RACE_NET_ENTRIES], uint32_t owner, const uint8_t *name,
                                int16_t slot, int16_t kind)
{
    const int32_t at = kind != 0 ? slot : RACE_NET_KIND0_FIRST + slot;
    if (at < 0 || at >= RACE_NET_ENTRIES) return NULL;  /* port guard */
    RaceNetLink *e = &entries[at];
    e->unknown_04 = 1, e->unknown_08 = 0x19, e->unknown_0c = 0xf, e->unknown_10 = 10;
    e->handler_14 = RACE_NET_HANDLER_14, e->handler_lost = RACE_NET_HANDLER_LOST;
    e->owner = owner;
    e->handler_1c = RACE_NET_HANDLER_1C;
    memcpy(e->name, name, RACE_NET_NAME);
    return e;
}

void race_net_forget(const RaceNetOps *ops, int16_t play, uint32_t entry)
{
    if (play == 1) ops->forget(ops->ctx, RACE_NET_SESSION, entry + RACE_NET_NAME_AT);
}

static int16_t state_word(const uint8_t *state, int at) { return (int16_t)(uint16_t)(state[at] | state[at + 1] << 8); }
static void put_state_word(uint8_t *state, int at, int16_t v) { state[at] = (uint8_t)v, state[at + 1] = (uint8_t)((uint16_t)v >> 8); }

static void send_state(const RaceNetPollOps *ops, const RaceNetPoll *p, int16_t slot)
{
    const uint32_t link = (uint32_t)slot < RACE_NET_LINKS ? p->links[slot] : 0;  /* port guard: the original reads on */
    ops->send(ops->ctx, link, p->state, RACE_NET_STATE);
    ops->service(ops->ctx);
}

void race_net_poll(const RaceNetPollOps *ops, const RaceNetPoll *p)
{
    if (p->host == 0) return;
    ops->update(ops->ctx);
    ops->lock(ops->ctx, p->lock);
    ops->handle(ops->ctx);
    ops->unlock(ops->ctx, p->lock);
    ops->service(ops->ctx);
    put_state_word(p->state, 0, RACE_NET_STATE_KIND);
    for (int16_t slot = 1; slot < state_word(p->state, RACE_NET_AT_HUMANS); slot++) {
        put_state_word(p->state, RACE_NET_AT_TO, slot);
        send_state(ops, p, slot);
    }
    put_state_word(p->state, RACE_NET_AT_TO, 0);
    for (int16_t slot = state_word(p->state, RACE_NET_AT_HUMANS);
         slot < (int32_t)state_word(p->state, RACE_NET_AT_HUMANS) + state_word(p->state, RACE_NET_AT_OTHERS); slot++)
        send_state(ops, p, slot);
}
