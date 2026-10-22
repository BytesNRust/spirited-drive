/* config.dat parsing and serialising (0042a010 / 0042a504 / 00429278).
 * With a path argument (registered by CMake when original/ holds the CD) it
 * also checks the real file. */
#include "game/frontend/config_dat.h"
#include "test.h"

#include <stdlib.h>

static void test_defaults(void)
{
    ConfigDat c;
    char name[CONFIG_NAME_SIZE];
    RaceSettings s;
    config_dat_defaults(&c, 0);
    config_dat_name(&c, name);
    CHECK(strcmp(name, "Player") == 0);
    CHECK(config_dat_get_race(&c, &s));
    CHECK_EQ_INT(s.race_type, RACE_HEAD_TO_HEAD);
    CHECK_EQ_INT(s.player_car, 6);
    CHECK_EQ_INT(s.opponent, 7);
    CHECK_EQ_INT(s.laps, 4);
    CHECK_EQ_INT(s.difficulty, 1);
    CHECK_EQ_INT(config_dat_unlock_level(&c), 0);
    /* 00429278's records: track 0's top speed row (00525421) is "Greenbeard", 65000 at +0x1b, car 7 at +0xf */
    CHECK(strcmp((const char *)c.image + (0x525421 - 0x524df0), "Greenbeard") == 0);
    CHECK_EQ_INT(c.image[0x525421 - 0x524df0 + 0x1b] | c.image[0x525421 - 0x524df0 + 0x1c] << 8, 65000);
    CHECK_EQ_INT(c.image[0x525421 - 0x524df0 + 0xf], 7);
    config_dat_defaults(&c, 1);
    config_dat_name(&c, name);
    CHECK(strcmp(name, "Spieler") == 0);
}

static void test_round_trip(void)
{
    ConfigDat a, b;
    uint8_t buf[CONFIG_DAT_SIZE];
    RaceSettings s, back;
    config_dat_defaults(&a, 0);
    a.image[0x1234] = 0xab;  /* a region we do not interpret must survive */
    config_dat_set_name(&a, "Edgar");
    config_dat_set_unlock_level(&a, 2);
    race_settings_init(&s);
    s.race_type = RACE_TOURNAMENT;
    s.track = 5;
    s.player_car = CAR_WARRIOR;
    s.opponent = OPPONENT_TOURNAMENT;
    s.laps = 6;
    s.difficulty = 2;
    s.car_flag_a = 2;
    s.car_flag_b = 1;
    config_dat_set_race(&a, &s);

    CHECK_EQ_INT(config_dat_serialise(&a, buf, sizeof buf), CONFIG_DAT_SIZE);
    CHECK_EQ_INT(config_dat_parse(&b, buf, sizeof buf), CONFIG_OK);
    CHECK_MEM(a.image, b.image, CONFIG_DAT_SIZE);
    CHECK(config_dat_get_race(&b, &back));
    CHECK_MEM(&s, &back, sizeof s);
    CHECK_EQ_INT(b.image[0x1234], 0xab);
    CHECK_EQ_INT(config_dat_unlock_level(&b), 2);
    const Unlocks u = config_dat_unlocks(&b);
    CHECK(u.variant_m && u.variant_r && u.lost_vegas && u.warrior);
}

/* 00466ae4 clears the segment (00524e83) for a new session: a stage saved mid-way (the window closed on segment
 * 1's results) must not start the next race on segment 2 */
static void test_new_session_clears_the_segment(void)
{
    ConfigDat c;
    config_dat_defaults(&c, 0);
    c.image[0x93] = 1;
    c.image[0x95] = 1;
    config_dat_new_session(&c);
    CHECK_EQ_INT(c.image[0x93] | c.image[0x94] << 8, 0);
    CHECK_EQ_INT(c.image[0x95], 1);
}

static void test_bounds(void)
{
    ConfigDat c;
    uint8_t buf[CONFIG_DAT_SIZE + 1] = { 0 };
    uint8_t small[CONFIG_DAT_SIZE];
    CHECK_EQ_INT(config_dat_parse(&c, buf, 0), CONFIG_BAD_SIZE);
    CHECK_EQ_INT(config_dat_parse(&c, buf, CONFIG_DAT_SIZE - 1), CONFIG_BAD_SIZE);
    CHECK_EQ_INT(config_dat_parse(&c, buf, CONFIG_DAT_SIZE + 1), CONFIG_BAD_SIZE);
    CHECK_EQ_INT(config_dat_parse(&c, NULL, CONFIG_DAT_SIZE), CONFIG_NULL);
    config_dat_defaults(&c, 0);
    CHECK_EQ_INT(config_dat_serialise(&c, small, CONFIG_DAT_SIZE - 1), 0);

    /* An unterminated name is cut, not read past the field. */
    memset(c.image, 'x', CONFIG_NAME_SIZE);
    char name[CONFIG_NAME_SIZE];
    config_dat_name(&c, name);
    CHECK_EQ_INT(strlen(name), CONFIG_NAME_SIZE - 1);

    /* Values out of range are refused rather than handed to the selection code. */
    RaceSettings s = { 0 };
    config_dat_defaults(&c, 0);
    c.image[0x6d] = 9;  /* race type */
    CHECK(!config_dat_get_race(&c, &s));
    CHECK_EQ_INT(s.race_type, 0);
    config_dat_defaults(&c, 0);
    c.image[0x9b] = 9;  /* player car: only 0-7 and the Warrior exist */
    CHECK(!config_dat_get_race(&c, &s));
}

static void test_real_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        fprintf(stderr, "cannot open %s\n", path);
        test_failures++;
        return;
    }
    static uint8_t data[CONFIG_DAT_SIZE + 1];
    const size_t n = fread(data, 1, sizeof data, f);
    fclose(f);
    ConfigDat c;
    uint8_t out[CONFIG_DAT_SIZE];
    RaceSettings s;
    char name[CONFIG_NAME_SIZE];
    CHECK_EQ_INT(n, CONFIG_DAT_SIZE);
    CHECK_EQ_INT(config_dat_parse(&c, data, n), CONFIG_OK);
    CHECK_EQ_INT(config_dat_serialise(&c, out, sizeof out), CONFIG_DAT_SIZE);
    CHECK_MEM(out, data, CONFIG_DAT_SIZE);
    config_dat_name(&c, name);
    CHECK(strcmp(name, "Player") == 0);
    CHECK(config_dat_get_race(&c, &s));
    CHECK_EQ_INT(s.race_type, RACE_HEAD_TO_HEAD);
    CHECK_EQ_INT(s.track, 1);
    CHECK_EQ_INT(s.player_car, 6);
    CHECK_EQ_INT(s.opponent, 7);
    CHECK_EQ_INT(s.laps, 8);
    CHECK_EQ_INT(s.difficulty, 1);
    CHECK_EQ_INT(config_dat_unlock_level(&c), 0);
}

int main(int argc, char **argv)
{
    test_defaults();
    test_round_trip();
    test_bounds();
    test_new_session_clears_the_segment();
    if (argc > 1) test_real_file(argv[1]);
    return TEST_REPORT();
}
