/* The online session's shared rules (session_rules.h): the record, the trailer it rides in after the setup's chunks, and what a receiver without the trailer does with it. */
#include "game/race/race_net.h"
#include "game/race/session_rules.h"
#include "tests/test.h"

#include <string.h>

static SessionRules sample(void)
{
    SessionRules r;
    memset(&r, 0, sizeof r);
    r.mode = SESSION_RESTOMOD, r.difficulty = 2, r.weather = 2, r.reverse = 1, r.lunar_springs = 1, r.lunar_gravity = 1, r.rally_gravel = 1, r.cheats = 5, r.rain_grip = 1, r.liveries = 1, r.time_of_day = 2, r.psx_time = 1;
    r.snow_weather = 1;
    for (int i = 0; i < SESSION_SLOTS; i++) r.paint[i] = (uint8_t)(i + 1);
    return r;
}

static void test_round_trip(void)
{
    const SessionRules r = sample();
    uint8_t buf[SESSION_RULES_SIZE];
    CHECK_EQ_INT(session_rules_put(&r, buf), SESSION_RULES_SIZE);
    CHECK_MEM(buf, "TNFSSYN1", 8);
    SessionRules got;
    CHECK_EQ_INT(session_rules_get(buf, sizeof buf, &got), 1);
    CHECK_MEM(&got, &r, sizeof r);
}

static void test_refuses_what_is_not_the_record(void)
{
    uint8_t buf[SESSION_RULES_SIZE];
    const SessionRules r = sample();
    session_rules_put(&r, buf);
    SessionRules got;
    memset(&got, 0x55, sizeof got);
    CHECK_EQ_INT(session_rules_get(buf, SESSION_RULES_SIZE - 1, &got), 0);   /* short */
    CHECK_EQ_INT(session_rules_get(NULL, 0, &got), 0);
    buf[0] = 'X';
    CHECK_EQ_INT(session_rules_get(buf, sizeof buf, &got), 0);               /* not the magic */
    session_rules_put(&r, buf);
    buf[SESSION_MODE_AT] = 3;                                                  /* a mode that does not exist */
    CHECK_EQ_INT(session_rules_get(buf, sizeof buf, &got), 0);
    session_rules_put(&r, buf);
    buf[SESSION_WEATHER_AT] = 9;
    CHECK_EQ_INT(session_rules_get(buf, sizeof buf, &got), 0);
    session_rules_put(&r, buf);
    buf[SESSION_DIFFICULTY_AT] = 3;
    CHECK_EQ_INT(session_rules_get(buf, sizeof buf, &got), 0);
    session_rules_put(&r, buf);
    buf[19] = 3;                                                               /* a time of day that does not exist */
    CHECK_EQ_INT(session_rules_get(buf, sizeof buf, &got), 0);
    CHECK_EQ_INT(got.mode, 0x55);                                              /* a refused record leaves the output alone */
}

/* port (snow_weather): the host's snow rides at the record's end (version 2); a version 1 record, an older host's, has none and means no snow */
static void test_snow_weather_rides_at_the_end(void)
{
    uint8_t buf[SESSION_RULES_SIZE];
    SessionRules r = sample(), got;
    session_rules_put(&r, buf);
    CHECK_EQ_INT(buf[SESSION_VERSION_AT], 2);
    CHECK_EQ_INT(buf[SESSION_SNOW_AT], 1);
    CHECK_EQ_INT(session_rules_get(buf, sizeof buf, &got), 1);
    CHECK_EQ_INT(got.snow_weather, 1);
    r.snow_weather = 0;
    session_rules_put(&r, buf);
    CHECK_EQ_INT(session_rules_get(buf, sizeof buf, &got), 1);
    CHECK_EQ_INT(got.snow_weather, 0);
    buf[SESSION_SNOW_AT] = 2;                                          /* not a switch */
    CHECK_EQ_INT(session_rules_get(buf, sizeof buf, &got), 0);
    r = sample();
    session_rules_put(&r, buf);
    buf[SESSION_VERSION_AT] = 1;                                       /* an older host's record: 29 bytes, no snow, however the last byte reads */
    got.snow_weather = 7;
    CHECK_EQ_INT(session_rules_get(buf, SESSION_RULES_SIZE_V1, &got), 1);
    CHECK_EQ_INT(got.snow_weather, 0);
    buf[SESSION_VERSION_AT] = 2;
    CHECK_EQ_INT(session_rules_get(buf, SESSION_RULES_SIZE_V1, &got), 0);   /* a version 2 record cut short */
}

static void test_a_later_version_still_reads(void)
{
    uint8_t buf[SESSION_RULES_SIZE + 6];
    const SessionRules r = sample();
    memset(buf, 0xee, sizeof buf);
    session_rules_put(&r, buf);
    buf[SESSION_VERSION_AT] = SESSION_RULES_VERSION + 1;                       /* more fields after the known ones */
    SessionRules got;
    CHECK_EQ_INT(session_rules_get(buf, sizeof buf, &got), 1);
    CHECK_MEM(&got, &r, sizeof r);
}

/* a link between a host and a guest: the messages the host sends, as the guest's handler takes them */
typedef struct {
    uint8_t message[16][3 + RACE_NET_SETUP_CHUNK];
    int size[16];
    int count;
} Wire;

static void wire_service(void *ctx) { (void)ctx; }
static void wire_send(void *ctx, uint32_t link, const uint8_t *data, int32_t size)
{
    Wire *w = ctx;
    (void)link;
    memcpy(w->message[w->count], data, (size_t)size);
    w->size[w->count++] = size;
}

static void send_all(Wire *w, uint8_t *setup, const uint8_t *trailer, int trailer_size)
{
    const RaceNetSetupSendOps ops = { w, wire_service, wire_send };
    uint8_t state[RACE_NET_STATE] = { 0 };
    setup[0x18] = 1, setup[0x19] = setup[0x1a] = setup[0x1b] = 0;   /* 004fb7d4 is 1: a link game's */
    race_net_send_setup_trailer(&ops, setup, state, trailer, trailer_size);
}

static void test_the_trailer_follows_the_setup_and_is_ignored_by_old_receivers(void)
{
    static uint8_t host_setup[RACE_NET_SETUP], guest_setup[RACE_NET_SETUP], old_setup[RACE_NET_SETUP];
    for (int i = 0; i < RACE_NET_SETUP; i++) host_setup[i] = (uint8_t)(i * 7 + 1);
    uint8_t rules[SESSION_RULES_SIZE];
    const SessionRules r = sample();
    session_rules_put(&r, rules);
    Wire w = { 0 };
    send_all(&w, host_setup, rules, SESSION_RULES_SIZE);
    CHECK_EQ_INT(w.count, 4 + 1 + 1);                                         /* the setup's 4 chunks, the trailer, the end */
    CHECK_EQ_INT(w.message[4][2], RACE_NET_SETUP_TRAILER_CHUNK);
    CHECK_EQ_INT(w.size[w.count - 1], RACE_NET_SETUP_END);

    int16_t flags[8] = { 0 };
    uint8_t state[RACE_NET_STATE] = { 0 };
    uint8_t trailer[RACE_NET_SETUP_TRAILER_MAX];
    int32_t trailer_size = 99;                                                  /* a stale one */
    RaceNetSetupIn newer = { flags, state, guest_setup, trailer, &trailer_size };
    RaceNetSetupIn older = { flags, state, old_setup, NULL, NULL };           /* a receiver that knows no trailer */
    for (int i = 0; i < w.count; i++) {
        race_net_on_setup(&newer, w.message[i], w.size[i]);
        race_net_on_setup(&older, w.message[i], w.size[i]);
    }
    CHECK_MEM(guest_setup, host_setup, 0x310);                                  /* +0x310 is the receiver's own slot */
    CHECK_MEM(old_setup, host_setup, 0x310);
    CHECK_EQ_INT(trailer_size, SESSION_RULES_SIZE);
    SessionRules got;
    CHECK_EQ_INT(session_rules_get(trailer, (size_t)trailer_size, &got), 1);
    CHECK_MEM(&got, &r, sizeof r);
}

static void test_a_host_without_the_trailer_clears_a_stale_one(void)
{
    static uint8_t host_setup[RACE_NET_SETUP], guest_setup[RACE_NET_SETUP];
    Wire w = { 0 };
    send_all(&w, host_setup, NULL, 0);
    CHECK_EQ_INT(w.count, 4 + 1);
    int16_t flags[8] = { 0 };
    uint8_t state[RACE_NET_STATE] = { 0 };
    uint8_t trailer[RACE_NET_SETUP_TRAILER_MAX];
    int32_t trailer_size = SESSION_RULES_SIZE;                                  /* the last race's */
    RaceNetSetupIn in = { flags, state, guest_setup, trailer, &trailer_size };
    for (int i = 0; i < w.count; i++) race_net_on_setup(&in, w.message[i], w.size[i]);
    CHECK_EQ_INT(trailer_size, 0);                                              /* two races in a row: the second does not wear the first's rules */
}

int main(void)
{
    test_round_trip();
    test_refuses_what_is_not_the_record();
    test_snow_weather_rides_at_the_end();
    test_a_later_version_still_reads();
    test_the_trailer_follows_the_setup_and_is_ignored_by_old_receivers();
    test_a_host_without_the_trailer_clears_a_stale_one();
    return TEST_REPORT();
}
