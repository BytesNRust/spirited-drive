/* The bytes the race's network code puts on the wire, pinned as the exe lays them out (00439c30 the player record,
 * 00439de0 the setup in chunks, 00410c18 the stage word, 00411050 the host's state block, handler 4's ping answer):
 * every message is captured through the send op and compared with literal bytes. */
#include "game/race/race_net.h"

#include <stdio.h>
#include <string.h>

enum { MAX_MESSAGES = 16, MAX_BYTES = 0x100 };

typedef struct {
    int count;
    uint32_t link[MAX_MESSAGES];
    int32_t size[MAX_MESSAGES];
    uint8_t data[MAX_MESSAGES][MAX_BYTES];
} Wire;

static int failures;

static void send(void *ctx, uint32_t link, const uint8_t *data, int32_t size)
{
    Wire *w = ctx;
    if (w->count >= MAX_MESSAGES || size > MAX_BYTES) return;
    w->link[w->count] = link, w->size[w->count] = size;
    memcpy(w->data[w->count++], data, (size_t)size);
}

static void nothing(void *ctx) { (void)ctx; }
static void lock_nothing(void *ctx, uint32_t lock) { (void)ctx, (void)lock; }
static int16_t slot_zero(void *ctx, uint32_t link) { (void)ctx, (void)link; return 0; }
static int32_t step_done(void *ctx) { (void)ctx; return 0; }
static void drop_none(void *ctx, int16_t slot) { (void)ctx, (void)slot; }

static void expect(const char *what, int ok)
{
    if (!ok) fprintf(stderr, "FAIL: %s\n", what), failures++;
}

static void expect_message(const char *what, const Wire *w, int k, uint32_t link, const uint8_t *bytes, int32_t size)
{
    char msg[96];
    snprintf(msg, sizeof msg, "%s: message %d", what, k);
    expect(msg, k < w->count && w->link[k] == link && w->size[k] == size && memcmp(w->data[k], bytes, (size_t)size) == 0);
}

static void info(void)
{
    Wire w = { 0 };
    uint8_t record[RACE_NET_INFO];
    for (int i = 0; i < RACE_NET_INFO; i++) record[i] = (uint8_t)(i + 0x41);
    const RaceNetInfoSendOps ops = { &w, send };
    race_net_send_info(&ops, 0, 0, record);
    expect("info: nothing sent when not playing", w.count == 0);
    race_net_send_info(&ops, 1, 1, record);
    expect("info: nothing sent in a replay", w.count == 0);
    race_net_send_info(&ops, 1, 0, record);
    uint8_t want[RACE_NET_INFO];
    for (int i = 0; i < RACE_NET_INFO; i++) want[i] = (uint8_t)(i + 0x41);
    want[0] = want[1] = 0;   /* the kind word 0 */
    expect("info: the record's size is 0x4b", RACE_NET_INFO == 0x4b);
    expect_message("info", &w, 0, 0, want, 0x4b);
    expect("info: one message", w.count == 1);
}

static void setup_chunks(void)
{
    Wire w = { 0 };
    uint8_t setup[RACE_NET_SETUP], state[RACE_NET_STATE] = { 0 };
    for (int i = 0; i < RACE_NET_SETUP; i++) setup[i] = (uint8_t)(i * 7 + 1);
    setup[0x18] = 1, setup[0x19] = setup[0x1a] = setup[0x1b] = 0;
    state[RACE_NET_AT_TO] = 3;
    const RaceNetSetupSendOps ops = { &w, nothing, send };
    race_net_send_setup(&ops, setup, state);
    /* 0x348 bytes: three chunks of 0xf7 and one of 0x5b, each {1, 0, chunk, bytes}, then {1, 0} */
    expect("setup: five messages", w.count == 5);
    expect("setup: chunk sizes", w.size[0] == 0xf7 + 3 && w.size[1] == 0xf7 + 3 && w.size[2] == 0xf7 + 3 &&
                                 w.size[3] == 0x348 - 3 * 0xf7 + 3 && w.size[4] == 2);
    uint8_t joined[RACE_NET_SETUP];
    int at = 0, heads = 1;
    for (int k = 0; k < 4 && k < w.count; k++) {
        heads &= w.data[k][0] == 1 && w.data[k][1] == 0 && w.data[k][2] == k;
        memcpy(joined + at, w.data[k] + 3, (size_t)(w.size[k] - 3));
        at += w.size[k] - 3;
    }
    expect("setup: heads {1, 0, chunk}", heads);
    expect("setup: 0x348 bytes in all", at == RACE_NET_SETUP);
    expect("setup: first word 1 and +0x310 the slot", joined[0] == 1 && joined[1] == 0 && joined[0x310] == 3 && joined[0x311] == 0 &&
                                                       memcmp(joined + 2, setup + 2, 0x30e) == 0 &&
                                                       memcmp(joined + 0x314, setup + 0x314, RACE_NET_SETUP - 0x314) == 0);
    expect("setup: the end message {1, 0}", w.size[4] == 2 && w.data[4][0] == 1 && w.data[4][1] == 0);

    Wire t = { 0 };
    const RaceNetSetupSendOps tops = { &t, nothing, send };
    const uint8_t trailer[5] = { 9, 8, 7, 6, 5 };
    race_net_send_setup_trailer(&tops, setup, state, trailer, 5);
    expect("setup+trailer: six messages", t.count == 6);
    const uint8_t want_trailer[8] = { 1, 0, 4, 9, 8, 7, 6, 5 };
    expect_message("setup+trailer", &t, 4, 0, want_trailer, 8);
    expect("setup+trailer: still ends {1, 0}", t.size[5] == 2);

    Wire n = { 0 };
    const RaceNetSetupSendOps nops = { &n, nothing, send };
    setup[0x18] = 0;
    race_net_send_setup(&nops, setup, state);
    expect("setup: nothing sent unless the setup's dword +0x18 is 1", n.count == 0);
}

static void ping_answers(void)
{
    Wire w = { 0 };
    int16_t flags[8] = { 0 };
    int32_t pings[RACE_NET_PINGS] = { 0 };
    const RaceNetPingOps ops = { &w, send, slot_zero };
    RaceNetPingIn in = { flags, 100, pings };
    const uint8_t ask0[4] = { 4, 0, 0, 0 }, ask1[4] = { 4, 0, 1, 0 };
    race_net_on_ping(&ops, &in, 0x77, ask0);
    race_net_on_ping(&ops, &in, 0x78, ask1);
    const uint8_t want0[4] = { 4, 0, 1, 0 }, want1[4] = { 4, 0, 2, 0 };
    expect_message("ping: 0 is answered 1", &w, 0, 0x77, want0, 4);
    expect_message("ping: 1 is answered 2", &w, 1, 0x78, want1, 4);
    expect("ping: two messages", w.count == 2);
}

static void stage_word(void)
{
    Wire w = { 0 };
    uint8_t state[RACE_NET_STATE] = { 0 };
    int16_t a[8] = { 0 }, b[8] = { 0 };
    a[RACE_NET_STAGE_FLAG] = 3;   /* the host's stage 3 is in */
    RaceNetWait wait = { 1, 0, 0, 0, state, NULL, NULL, a, b };   /* 004fbb0c 0: a client */
    const RaceNetWaitOps ops = { &w, send, nothing, step_done, drop_none, nothing };
    expect("stage: a client answers the host", race_net_wait(&ops, &wait, 3) == 1);
    const uint8_t want[4] = { 5, 0, 3, 0 };
    expect_message("stage", &w, 0, 0, want, 4);
    expect("stage: the flag is taken", a[RACE_NET_STAGE_FLAG] == 0);

    Wire h = { 0 };
    RaceNetWait host = { 1, 0, 0, 1, state, NULL, NULL, a, b };   /* 004fbb0c 1: the host sends its stage, nobody to wait for */
    const RaceNetWaitOps hops = { &h, send, nothing, step_done, drop_none, nothing };
    expect("stage: the host's stage is sent", race_net_wait(&hops, &host, 2) == 1);
    const uint8_t want_host[4] = { 5, 0, 2, 0 };
    expect_message("stage (host)", &h, 0, 0, want_host, 4);
}

static void state_block(void)
{
    Wire w = { 0 };
    uint8_t state[RACE_NET_STATE] = { 0 };
    state[RACE_NET_AT_HUMANS] = 3, state[RACE_NET_AT_OTHERS] = 1;
    state[20] = 0x5a;
    const uint32_t links[RACE_NET_LINKS] = { 10, 11, 12, 13, 14, 15, 16, 17 };
    const RaceNetPollOps ops = { &w, nothing, lock_nothing, nothing, lock_nothing, nothing, send };
    const RaceNetPoll poll = { 1, 5, state, links };
    race_net_poll(&ops, &poll);
    /* the host's block (kind 8) to slots 1 and 2 with its "to" word set, then to the others' (slot 3) with 0 */
    expect("state: three messages of 0x74 bytes", w.count == 3 && w.size[0] == RACE_NET_STATE && w.size[1] == RACE_NET_STATE &&
                                                   RACE_NET_STATE == 0x74);
    expect("state: to slots 1, 2 and 3", w.link[0] == 11 && w.link[1] == 12 && w.link[2] == 13);
    expect("state: kind 8", w.data[0][0] == 8 && w.data[0][1] == 0 && w.data[2][0] == 8);
    expect("state: the to word is the slot, 0 for the others",
           w.data[0][RACE_NET_AT_TO] == 1 && w.data[1][RACE_NET_AT_TO] == 2 && w.data[2][RACE_NET_AT_TO] == 0);
    expect("state: the rest is the block", w.data[0][20] == 0x5a && w.data[0][RACE_NET_AT_HUMANS] == 3 && w.data[0][RACE_NET_AT_OTHERS] == 1);
}

int main(void)
{
    info();
    setup_chunks();
    ping_answers();
    stage_word();
    state_block();
    if (failures == 0) printf("race_net_wire: ok\n");
    return failures == 0 ? 0 : 1;
}
