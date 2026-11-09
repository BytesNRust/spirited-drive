/* The race's side of the network game: the words received (race_net.h). */
#include "game/race/race_net.h"
#include "game/race/race_net_words.h"

#include "game/gfx/x86_int.h"

#include <string.h>

void race_net_on_stand_ins(const RaceNetStandInOps *ops, RaceNetStandIns *s, uint32_t link, const uint8_t *data)
{
    if (s->unknown_4fbb0c != 0) ops->stand_in(ops->ctx, ops->slot_of(ops->ctx, link));
    else memcpy(s->block, data, RACE_NET_STANDIN_BLOCK);
    for (int32_t i = 0; i < s->racers && i < RACE_NET_PLAYERS; i++)  /* port guard: eight */
        if (state_word(s->block, 2 + 2 * i) != 0) s->records[i].finish = RACE_NET_GONE_FINISH;
}

void race_net_on_state(const RaceNetLockOps *ops, RaceNetStateIn *s, const uint8_t *data)
{
    ops->lock(ops->ctx, s->lock);
    memcpy(s->in, data, RACE_NET_STATE);
    s->unknown_50a5bc = 1;
    ops->unlock(ops->ctx, s->lock);
}

void race_net_on_settings(const RaceNetSettingsInOps *ops, RaceNetSettingsIn *s, uint32_t link, const uint8_t *data)
{
    ops->lock(ops->ctx, s->lock);
    if (s->host != 0) {
        const int16_t slot = ops->slot_of(ops->ctx, link);
        if (slot >= 0 && slot < RACE_NET_SET_WORDS) {  /* port guard */
            put_state_word(s->settings, RACE_NET_SET_READY + 2 * slot, state_word(data, 4));
            put_state_word(s->settings, RACE_NET_SET_CARS + 2 * slot, state_word(data, 2));
            s->unknown_4f8138[slot] = 1;
        }
    } else {
        /* word by word in the exe (the five words, +0x44, the two dwords, the cars with 004f8284's eight, the readies): all of +2..+0x4e */
        memcpy(s->settings + RACE_NET_SET_TYPE, data + RACE_NET_SET_TYPE, RACE_NET_SETTINGS - RACE_NET_SET_TYPE);
    }
    s->unknown_50a5ba = 1;
    ops->unlock(ops->ctx, s->lock);
}

void race_net_on_stage(const RaceNetSlotOps *ops, RaceNetStageIn *s, uint32_t link, const uint8_t *data)
{
    if (s->host != 0) {
        const int16_t slot = ops->slot_of(ops->ctx, link);
        if (slot >= 0 && slot < RACE_NET_FLAGS_B) s->flags_b[slot] = state_word(data, 2);  /* port guard */
    } else {
        s->flags_a[RACE_NET_STAGE_FLAG] = state_word(data, 2);
    }
}

void race_net_on_hello(RaceNetHelloIn *h, const uint8_t *data)
{
    if (state_word(data, 2) == h->unknown_4fbb0c) {
        h->dropped = 1, h->unknown_513e44 = 1;
        return;
    }
    h->unknown_50a5be = 1;
    const int64_t at = RACE_NET_AT_NAMES + (int64_t)h->unknown_4fbb0c * RACE_NET_TAG;
    if (at >= 0 && at < RACE_NET_STATE) copy_name(h->state, (int)at, data + 4, RACE_NET_TAG);  /* port guard */
    if (h->unknown_4fbb0c == 0) return;
    for (int i = 0; i < RACE_NET_TAG; i++)  /* port guard: entry 0's tag */
        if ((h->tags[i] = data[4 + i]) == 0) return;
}

static void answer_ping(const RaceNetPingOps *ops, uint32_t link, int16_t answer)
{
    uint8_t message[RACE_NET_PING];
    put_state_word(message, 0, RACE_NET_PING_KIND), put_state_word(message, 2, answer);
    ops->send(ops->ctx, link, message, RACE_NET_PING);
}

void race_net_on_ping(const RaceNetPingOps *ops, RaceNetPingIn *p, uint32_t link, const uint8_t *data)
{
    switch ((uint16_t)state_word(data, 2)) {
    case 0: answer_ping(ops, link, 1); break;
    case 1: answer_ping(ops, link, 2); break;
    case 2:
        p->flags_a[RACE_NET_PINGED_FLAG] = 1;
        answer_ping(ops, link, 3);
        break;
    case 3: {
        const int16_t slot = ops->slot_of(ops->ctx, link);
        if (slot >= 0 && slot < RACE_NET_PINGS) p->unknown_4f8108[slot] = p->clock;  /* port guard */
        break;
    }
    default: break;
    }
}

void race_net_on_info(const RaceNetSlotOps *ops, RaceNetInfoIn *in, uint32_t link, const uint8_t *data)
{
    const int16_t slot = ops->slot_of(ops->ctx, link);
    if (slot < 0) return;  /* port guard */
    if (slot < RACE_NET_PLAYERS) memcpy(in->infos + slot * RACE_NET_INFO, data, RACE_NET_INFO);  /* port guard */
    if (slot < RACE_NET_FLAGS_B) in->flags_b[slot] = RACE_NET_INFO_STAGE;  /* port guard */
}

void race_net_on_setup(RaceNetSetupIn *in, const uint8_t *data, int32_t size)
{
    if (size == RACE_NET_SETUP_END) {
        in->flags_a[RACE_NET_SETUP_FLAG] = 1;
        const int32_t slot = state_word(in->state, RACE_NET_AT_TO);
        for (int k = 0; k < 4; k++) in->setup[RACE_NET_SETUP_4FBACC + k] = (uint8_t)((uint32_t)slot >> 8 * k);
        return;
    }
    if (data[2] == 0 && in->trailer_size != NULL) *in->trailer_size = 0;   /* port: a new setup begins; the last race's trailer is not this one's */
    if (data[2] == RACE_NET_SETUP_TRAILER_CHUNK && size > RACE_NET_SETUP_HEAD && in->trailer != NULL && in->trailer_size != NULL) {
        const int32_t n = size - RACE_NET_SETUP_HEAD;
        memcpy(in->trailer, data + RACE_NET_SETUP_HEAD, (size_t)(n > RACE_NET_SETUP_TRAILER_MAX ? RACE_NET_SETUP_TRAILER_MAX : n));
        *in->trailer_size = n > RACE_NET_SETUP_TRAILER_MAX ? RACE_NET_SETUP_TRAILER_MAX : n;
        return;
    }
    const int32_t at = data[2] * RACE_NET_SETUP_CHUNK;
    for (int32_t k = 0; k < size - RACE_NET_SETUP_HEAD && at + k < RACE_NET_SETUP; k++)  /* port guard */
        in->setup[at + k] = data[RACE_NET_SETUP_HEAD + k];
}

static const uint8_t CHAT_INDENT[] = "   ";  /* 0040a3c0 */

/* strcpy (at 0) or strcat (at the line's NUL) into line c->line; port guard: within the line, a line in 0..8 */
static void chat_put(RaceNetChatIn *c, const uint8_t *from, int append)
{
    if (c->line < 0 || c->line >= RACE_NET_CHAT_LINES) return;
    uint8_t *line = c->lines + c->line * RACE_NET_CHAT_LINE;
    int at = 0;
    if (append)
        while (at < RACE_NET_CHAT_LINE && line[at] != 0) at++;
    for (; at < RACE_NET_CHAT_LINE; at++, from++)
        if ((line[at] = *from) == 0) return;
}

static void chat_next(RaceNetChatIn *c)
{
    c->line = (int16_t)(c->line + 1);
    if (c->line == RACE_NET_CHAT_LINES) c->line = 0;
}

void race_net_on_chat(const RaceNetChatOps *ops, RaceNetChatIn *c, uint32_t link, const uint8_t *data)
{
    uint8_t copy[RACE_NET_CHAT];
    memcpy(copy, data, sizeof copy);
    if (c->host != 0 && link != 0) ops->send(ops->ctx, 0, copy, RACE_NET_CHAT);
    ops->lock(ops->ctx, c->lock);
    chat_put(c, data + RACE_NET_CHAT_NAME, 0);
    chat_next(c);
    chat_put(c, CHAT_INDENT, 0), chat_put(c, data + RACE_NET_CHAT_TEXT, 1);
    chat_next(c);
    if (data[RACE_NET_CHAT_MORE] != 0) {
        chat_put(c, CHAT_INDENT, 0), chat_put(c, data + RACE_NET_CHAT_MORE, 1);
        chat_next(c);
    }
    c->unknown_4b9062 = 1;
    ops->unlock(ops->ctx, c->lock);
}

int32_t race_net_receive(const RaceNetReceiveOps *ops, int16_t dropped, uint32_t link, const uint8_t *data, int32_t size)
{
    if (dropped != 0) return 1;
    const int16_t kind = state_word(data, 0);
    if (kind >= RACE_NET_KINDS) ops->bug(ops->ctx, RACE_NET_BUG_TEXT, kind, link);
    else if (kind >= 0) ops->handle(ops->ctx, kind, link, data, size);  /* port guard */
    return 1;
}

int32_t race_net_link_escape(const RaceNetEscapeOps *ops, uint8_t esc, int32_t *unknown_513e48)
{
    if (esc == 0) return 0;
    *unknown_513e48 = 1;
    ops->wait_release(ops->ctx);
    return 1;
}

int32_t race_net_session_escape(const RaceNetSessionEscapeOps *ops, const RaceNetSessionEscape *e)
{
    ops->service(ops->ctx);
    if (e->esc == 0) return e->unknown_50a2fc == 0;
    ops->service(ops->ctx);
    while (e->esc != 0) ops->service(ops->ctx);
    return 1;
}
