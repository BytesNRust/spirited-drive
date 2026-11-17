/* The host's settings name the track by its menu number (RACE_NET_SET_TRACK). A client takes it only when it is one of
 * the nine tracks (0..8); a word past them (9, 15, 0xFFFF from a host that is not this version, or not honest) takes none
 * of the settings and sets bad_track, which the glue turns into leaving the game. */
#include "tests/test.h"

#include "game/race/race_net.h"
#include "game/race/race_net_words.h"

#include <string.h>

static int menus;

static void nothing(void *ctx) { (void)ctx; }
static void lock_nothing(void *ctx, uint32_t lock) { (void)ctx, (void)lock; }
static void send_nothing(void *ctx, uint32_t link, const uint8_t *data, int32_t size) { (void)ctx, (void)link, (void)data, (void)size; }
static void menu(void *ctx) { (void)ctx, menus++; }

/* a client whose host's settings came in with TRACK and race type 2 (the client is on 1, track 3) */
static RaceNetSync client_after(int16_t track, uint8_t *settings, uint8_t *state, int16_t *cars, int16_t *words)
{
    memset(settings, 0, RACE_NET_SETTINGS), memset(state, 0, RACE_NET_STATE);
    put_state_word(settings, 0, RACE_NET_SETTINGS_KIND), put_state_word(settings, RACE_NET_SET_TYPE, 2);
    put_state_word(settings, RACE_NET_SET_TRACK, track), put_state_word(settings, RACE_NET_SET_524EA1, 6);
    RaceNetSync s = { 0 };
    s.play = 1, s.host = 0, s.unknown_50a5ba = 1, s.unknown_52b310 = 1;
    s.state = state, s.in = state, s.settings = settings;
    s.race_type = 1, s.track = 3, s.unknown_524ea1 = 2, s.unknown_524e8b = cars, s.unknown_52b2b4 = words;
    const RaceNetSyncOps ops = { NULL, nothing, lock_nothing, lock_nothing, nothing, send_nothing, menu };
    menus = 0;
    race_net_sync(&ops, &s);
    return s;
}

static void taken(int16_t track)
{
    uint8_t settings[RACE_NET_SETTINGS], state[RACE_NET_STATE];
    int16_t cars[1 + RACE_NET_SET_WORDS] = { 0 }, words[RACE_NET_SET_WORDS] = { 0 };
    const RaceNetSync s = client_after(track, settings, state, cars, words);
    CHECK_EQ_INT(s.track, track);
    CHECK_EQ_INT(s.race_type, 2);
    CHECK_EQ_INT(s.unknown_524ea1, 6);
    CHECK_EQ_INT(s.unknown_50a5ba, 0);
    CHECK_EQ_INT(s.bad_track, 0);
    CHECK_EQ_INT(menus, 1);
}

static void refused(int16_t track)
{
    uint8_t settings[RACE_NET_SETTINGS], state[RACE_NET_STATE];
    int16_t cars[1 + RACE_NET_SET_WORDS] = { 0 }, words[RACE_NET_SET_WORDS] = { 0 };
    const RaceNetSync s = client_after(track, settings, state, cars, words);
    CHECK_EQ_INT(s.track, 3);            /* none of the settings taken */
    CHECK_EQ_INT(s.race_type, 1);
    CHECK_EQ_INT(s.unknown_524ea1, 2);
    CHECK_EQ_INT(menus, 0);
    CHECK_EQ_INT(s.unknown_50a5ba, 0);   /* not taken again on the next call */
    CHECK_EQ_INT(s.bad_track, 1);
}

/* TNFS_FAKE_HOST_TRACK's text: a whole number from -32768 to 0xFFFF, or nothing changes */
static void fake_word(void)
{
    int16_t w = 7;
    CHECK(!race_net_fake_track_word(NULL, &w) && w == 7);
    CHECK(!race_net_fake_track_word("", &w) && w == 7);
    CHECK(!race_net_fake_track_word("abc", &w) && w == 7);
    CHECK(!race_net_fake_track_word("9x", &w) && w == 7);
    CHECK(!race_net_fake_track_word(" ", &w) && w == 7);
    CHECK(!race_net_fake_track_word("65536", &w) && w == 7);
    CHECK(!race_net_fake_track_word("-32769", &w) && w == 7);
    CHECK(race_net_fake_track_word("9", &w) && w == 9);
    CHECK(race_net_fake_track_word("15", &w) && w == 15);
    CHECK(race_net_fake_track_word("0xFFFF", &w) && w == -1);
    CHECK(race_net_fake_track_word("-1", &w) && w == -1);
}

int main(void)
{
    fake_word();
    for (int16_t t = 0; t < 9; t++) taken(t);
    refused(9);
    refused(15);
    refused((int16_t)0xFFFF);
    refused(0x7fff);
    CHECK(race_net_track_ok(8) && !race_net_track_ok(9) && !race_net_track_ok(-1));
    return TEST_REPORT();
}
