#include "game/frontend/config_dat.h"

#include <string.h>

enum {
    OFFSET_NAME = 0x0000,
    OFFSET_CONTROLS = 0x5f36,
    OFFSET_UNLOCK = 0x5f50,

    /* 00524df0, the start of the block, for turning addresses into offsets. */
    BASE = 0x00524df0
};

static uint16_t get16(const ConfigDat *c, size_t offset)
{
    return (uint16_t)(c->image[offset] | c->image[offset + 1] << 8);
}

static void set16(ConfigDat *c, size_t offset, int value)
{
    c->image[offset] = (uint8_t)value;
    c->image[offset + 1] = (uint8_t)(value >> 8);
}

/* Race words, by original address (00524e5d ...). */
enum {
    ADDR_RACE_TYPE = 0x00524e5d,
    ADDR_CAR_FLAG_B = 0x00524e7f,
    ADDR_OPPONENT = 0x00524e81,
    ADDR_TRACK = 0x00524e85,
    ADDR_CAR_FLAG_A = 0x00524e89,
    ADDR_PLAYER_CAR = 0x00524e8b,
    ADDR_LAPS = 0x00524ea1,
    ADDR_DIFFICULTY = 0x00524ea3
};

/* 00429278: every word it sets in the race block, with the value it stores. */
static const struct {
    uint32_t address;
    uint16_t value;
} RACE_DEFAULTS[] = {
    { ADDR_RACE_TYPE, 1 }, { 0x00524e5f, 0 }, { 0x00524e61, 0 }, { 0x00524e63, 1 },
    { 0x00524e65, 100 }, { 0x00524e67, 100 }, { 0x00524e69, 0x7f }, { 0x00524e6b, 1 },
    { 0x00524e6d, 1 }, { 0x00524e7b, 0 }, { 0x00524e7d, 1 }, { ADDR_CAR_FLAG_B, 2 },
    { 0x00524e83, 0 }, { ADDR_OPPONENT, 7 }, { ADDR_TRACK, 0 }, { 0x00524e87, 0 },
    { ADDR_CAR_FLAG_A, 0 }, { ADDR_PLAYER_CAR, 6 }, { 0x00524e9f, 0 }, { ADDR_LAPS, 4 },
    { ADDR_DIFFICULTY, 1 },
    /* 004670e8 picks these from a measured value (TODO(004670e8): unknown, the
     * lowest bracket is used). */
    { 0x00524e6f, 0 }, { 0x00524e71, 0 }, { 0x00524e73, 0 }, { 0x00524e75, 0 },
    { 0x00524e77, 0 }, { 0x00524e79, 0 }
};

static const uint16_t CONTROL_DEFAULTS[13] = {
    0, 0, 0, 0x4b00, 0x4d00, 0x4800, 0x5000, 0x41, 0x5a, 0x20, 0x43, 0x48, 0x4d
};

/* 00429278 */
void config_dat_defaults(ConfigDat *c, int german)
{
    memset(c->image, 0, sizeof c->image);
    config_dat_set_name(c, german ? "Spieler" : "Player");
    for (size_t i = 0; i < sizeof RACE_DEFAULTS / sizeof RACE_DEFAULTS[0]; i++) {
        set16(c, RACE_DEFAULTS[i].address - BASE, RACE_DEFAULTS[i].value);
    }
    for (size_t i = 0; i < 13; i++) set16(c, OFFSET_CONTROLS + 2 * i, CONTROL_DEFAULTS[i]);
    if (german) {
        set16(c, OFFSET_CONTROLS + 2 * 8, 0x59);
        set16(c, OFFSET_CONTROLS + 2 * 10, 0x4b);
    }
    /* TODO(00429278): the record tables from the string tables at
     * 004b9638..004baa44 (names and five numbers per row). */
}

/* 0042a010 */
ConfigResult config_dat_parse(ConfigDat *c, const uint8_t *data, size_t size)
{
    if (c == NULL || data == NULL) return CONFIG_NULL;
    if (size != CONFIG_DAT_SIZE) return CONFIG_BAD_SIZE;
    memcpy(c->image, data, CONFIG_DAT_SIZE);
    return CONFIG_OK;
}

/* 0042a504 */
size_t config_dat_serialise(const ConfigDat *c, uint8_t *out, size_t cap)
{
    if (c == NULL || out == NULL || cap < CONFIG_DAT_SIZE) return 0;
    memcpy(out, c->image, CONFIG_DAT_SIZE);
    return CONFIG_DAT_SIZE;
}

void config_dat_name(const ConfigDat *c, char out[CONFIG_NAME_SIZE])
{
    size_t n = 0;
    while (n < CONFIG_NAME_SIZE - 1 && c->image[OFFSET_NAME + n] != 0) {
        out[n] = (char)c->image[OFFSET_NAME + n];
        n++;
    }
    out[n] = 0;
}

void config_dat_set_name(ConfigDat *c, const char *name)
{
    size_t n = strlen(name);
    if (n > CONFIG_NAME_SIZE - 1) n = CONFIG_NAME_SIZE - 1;
    memset(c->image + OFFSET_NAME, 0, CONFIG_NAME_SIZE);
    memcpy(c->image + OFFSET_NAME, name, n);
}

static int word_at(const ConfigDat *c, uint32_t address)
{
    return get16(c, address - BASE);
}

static int in_range(int v, int lo, int hi)
{
    return v >= lo && v <= hi;
}

int config_dat_get_race(const ConfigDat *c, RaceSettings *s)
{
    RaceSettings r = { 0 };
    r.race_type = word_at(c, ADDR_RACE_TYPE);
    r.track = word_at(c, ADDR_TRACK);
    r.player_car = word_at(c, ADDR_PLAYER_CAR);
    r.opponent = word_at(c, ADDR_OPPONENT);
    r.laps = word_at(c, ADDR_LAPS);
    r.difficulty = word_at(c, ADDR_DIFFICULTY);
    r.car_flag_a = word_at(c, ADDR_CAR_FLAG_A);
    r.car_flag_b = word_at(c, ADDR_CAR_FLAG_B);
    const int valid = in_range(r.race_type, 0, RACE_TYPES - 1) && in_range(r.track, 0, TRACK_HIDDEN)
        && (in_range(r.player_car, 0, CAR_COUNT - 1) || r.player_car == CAR_WARRIOR)
        && in_range(r.opponent, 0, CAR_WARRIOR) && in_range(r.laps, 1, 99)
        && in_range(r.difficulty, 0, 2) && in_range(r.car_flag_a, 0, 2) && in_range(r.car_flag_b, 0, 2);
    if (valid) *s = r;
    return valid;
}

void config_dat_set_race(ConfigDat *c, const RaceSettings *s)
{
    set16(c, ADDR_RACE_TYPE - BASE, s->race_type);
    set16(c, ADDR_TRACK - BASE, s->track);
    set16(c, ADDR_PLAYER_CAR - BASE, s->player_car);
    set16(c, ADDR_OPPONENT - BASE, s->opponent);
    set16(c, ADDR_LAPS - BASE, s->laps);
    set16(c, ADDR_DIFFICULTY - BASE, s->difficulty);
    set16(c, ADDR_CAR_FLAG_A - BASE, s->car_flag_a);
    set16(c, ADDR_CAR_FLAG_B - BASE, s->car_flag_b);
}

int config_dat_unlock_level(const ConfigDat *c)
{
    return (int16_t)get16(c, OFFSET_UNLOCK);
}

void config_dat_set_unlock_level(ConfigDat *c, int level)
{
    set16(c, OFFSET_UNLOCK, level);
}

/* 00432ff4: the level feeds Lost Vegas as it is (any nonzero value), the "m"
 * variants and the Warrior at > 0, the "r" variants at > 1. The hidden track
 * (004baf14) is never set by the code this port covers. */
Unlocks config_dat_unlocks(const ConfigDat *c)
{
    const int level = config_dat_unlock_level(c);
    Unlocks u = { 0, 0, 0, 0, 0 };
    u.variant_m = level > 0;
    u.variant_r = level > 1;
    u.lost_vegas = level != 0;
    u.warrior = level > 0;
    return u;
}
