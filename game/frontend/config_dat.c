#include "game/frontend/config_dat.h"

#include "game/frontend/config_records.h"

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

static void set32(ConfigDat *c, uint32_t address, uint32_t value)
{
    uint8_t *const p = c->image + (address - BASE);
    p[0] = (uint8_t)value, p[1] = (uint8_t)(value >> 8), p[2] = (uint8_t)(value >> 16), p[3] = (uint8_t)(value >> 24);
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
    { ADDR_DIFFICULTY, 1 }, { 0x00524e77, 0 }, { 0x00524e79, 0 }
};

/* 00429278 on 004670e8's ticks (100 frames drawn): the graphics words 00524e6f, 00524e73, 00524e75, 00524e71 */
static const struct {
    int16_t below;
    uint16_t value[4];
} DETAIL[] = {
    { 300, { 0, 0, 0, 0 } }, { 0x1a4, { 0, 1, 1, 0 } }, { 600, { 0, 2, 2, 1 } },
    { 0x2d0, { 1, 0, 0, 0 } }, { 900, { 1, 0, 1, 0 } }, { INT16_MAX, { 1, 0, 2, 1 } }
};
static const uint32_t DETAIL_AT[4] = { 0x00524e6f, 0x00524e73, 0x00524e75, 0x00524e71 };

/* config.dat's records (best_times.h): per track 0xa6b bytes from 00524ea5, rows of 0x27 */
enum {
    RECORDS = 0x00524ea5,
    RECORD_TRACK = 0xa6b,
    RECORD_ROW = 0x27,
    RECORD_CHOICE = 0x186,  /* a laps choice's ten rows, past the first three tracks */
    RECORD_TIMES = 0xea,
    RECORD_TOP = 0x57c,
    RECORD_ZEROED = 0x5bb,  /* three tables of 100 dwords 00429278 clears */
    RECORD_ZEROED_TABLE = 0x190,
    RECORD_TRACKS = 9
};

/* 00429278's tables (the row addresses in the exe) for tracks 0..2 and 3..8 */
static const uint32_t FIRST_ROWS[3] = { 0x004b9710, 0x004b97a0, 0x004b9830 };
static const uint32_t TIMES_0_2[3] = { 0x004b9950, 0x004b9a40, 0x004b9b30 };
static const uint32_t TIMES_3_8[6] = { 0x004b9c20, 0x004b9ef0, 0x004ba490, 0x004ba1c0, 0x004ba760, 0x004baa30 };
enum { TOP_ROWS = 0x004b9638, FIRST_ROWS_3_8 = 0x004b98c0 };

/* the dwords it sets after the tables, by original address */
static const struct {
    uint32_t address, value;
} RECORD_WORDS[] = {
    { 0x00525448, 0x1ca4 }, { 0x0052544c, 0x3a50 }, { 0x00525450, 0x680a }, { 0x00525454, 0x5a66c6 },
    { 0x00525458, 0x4b4f38 }, { 0x0052545c, 0x4a0302 },
    { 0x00525eb3, 0x235e }, { 0x00525eb7, 0x49ec }, { 0x00525ebb, 0x770a }, { 0x00525ebf, 0x54b80e },
    { 0x00525ec3, 0x464366 }, { 0x00525ec7, 0x406a41 },
    { 0x0052691e, 0x267c }, { 0x00526922, 0x5bd6 }, { 0x00526926, 0x9892 }, { 0x0052692a, 0x529544 },
    { 0x0052692e, 0x49d4e7 }, { 0x00526932, 0x3a85af }
};

static const uint16_t CONTROL_DEFAULTS[13] = {
    0, 0, 0, 0x4b00, 0x4d00, 0x4800, 0x5000, 0x41, 0x5a, 0x20, 0x43, 0x48, 0x4d
};

/* a record: the row's name (00473d43), its numbers at +0x1b, +0xf, +0x1f, +0x13, +0x23 in that order, +0x17 = 1 */
static void put_record(ConfigDat *c, uint32_t at, uint32_t row)
{
    static const uint8_t FIELD[5] = { 0x1b, 0x0f, 0x1f, 0x13, 0x23 };
    const ConfigRecordRow *r = &CONFIG_RECORD_ROWS[(row - CONFIG_RECORD_FIRST) / CONFIG_RECORD_ROW_SIZE];
    memcpy(c->image + (at - BASE), r->name, strlen(r->name) + 1);
    for (int k = 0; k < 5; k++) set32(c, at + FIELD[k], (uint32_t)r->number[k]);
    set32(c, at + 0x17, 1);
}

static uint32_t track_at(int track) { return RECORDS + (uint32_t)(track * RECORD_TRACK); }

static void put_records(ConfigDat *c)
{
    enum { ROW = CONFIG_RECORD_ROW_SIZE, TEN = 10, CHOICES = 3, SIX = 6 };
    for (int t = 0; t < RECORD_TRACKS; t++) put_record(c, track_at(t) + RECORD_TOP, TOP_ROWS + t * ROW);
    for (int t = 0; t < 3; t++)
        for (int r = 0; r < TEN; r++) put_record(c, track_at(t) + RECORD_TIMES + r * RECORD_ROW, TIMES_0_2[t] + r * ROW);
    for (int i = 0; i < 3; i++)
        for (int half = 0; half < 2; half++)
            for (int t = 0; t < 3; t++)
                put_record(c, track_at(t) + (i + 3 * half) * RECORD_ROW, FIRST_ROWS[t] + (i + 3 * half) * ROW);
    for (int r = 0; r < TEN; r++)
        for (int t = 0; t < SIX; t++)
            for (int ch = 0; ch < CHOICES; ch++)
                put_record(c, track_at(t + 3) + RECORD_TIMES + ch * RECORD_CHOICE + r * RECORD_ROW,
                           TIMES_3_8[t] + (ch * TEN + r) * ROW);
    for (int t = 0; t < SIX; t++) put_record(c, track_at(t + 3), FIRST_ROWS_3_8 + t * ROW);
    /* already zero: config_dat_defaults_measured starts from a zeroed image (the original from 00524df0's) */
    for (int i = 0; i < 100; i++)
        for (int t = 0; t < RECORD_TRACKS; t++)
            for (int k = 0; k < 3; k++) set32(c, track_at(t) + RECORD_ZEROED + k * RECORD_ZEROED_TABLE + i * 4, 0);
    for (size_t i = 0; i < sizeof RECORD_WORDS / sizeof RECORD_WORDS[0]; i++)
        set32(c, RECORD_WORDS[i].address, RECORD_WORDS[i].value);
}

/* 00429278 */
void config_dat_defaults_measured(ConfigDat *c, int german, int16_t ticks)
{
    memset(c->image, 0, sizeof c->image);
    config_dat_set_name(c, german ? "Spieler" : "Player");
    for (size_t i = 0; i < sizeof RACE_DEFAULTS / sizeof RACE_DEFAULTS[0]; i++) {
        set16(c, RACE_DEFAULTS[i].address - BASE, RACE_DEFAULTS[i].value);
    }
    size_t d = 0;
    while (ticks >= DETAIL[d].below && d + 1 < sizeof DETAIL / sizeof DETAIL[0]) d++;
    for (int k = 0; k < 4; k++) set16(c, DETAIL_AT[k] - BASE, DETAIL[d].value[k]);
    put_records(c);
    for (size_t i = 0; i < 13; i++) set16(c, OFFSET_CONTROLS + 2 * i, CONTROL_DEFAULTS[i]);
    if (german) {
        set16(c, OFFSET_CONTROLS + 2 * 8, 0x59);
        set16(c, OFFSET_CONTROLS + 2 * 10, 0x4b);
    }
    static const uint32_t NAME_AT[3] = { 0x0052ac68, 0x0052ac9a, 0x0052acd6 };
    for (int k = 0; k < 3; k++)
        memcpy(c->image + (NAME_AT[k] - BASE), CONFIG_RECORD_NAMES[k], strlen(CONFIG_RECORD_NAMES[k]) + 1);
    set16(c, 0x0052ac96 - BASE, 1), set16(c, 0x0052ac92 - BASE, 0), set16(c, 0x0052ac94 - BASE, 4);
    set16(c, OFFSET_UNLOCK, 0), set16(c, 0x0052ac90 - BASE, 9);
}

void config_dat_defaults(ConfigDat *c, int german)
{
    config_dat_defaults_measured(c, german, 0);
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

void config_dat_startup_volume(ConfigDat *c)
{
    set16(c, 0x00524e67 - BASE, 100);
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

/* 00464670: the division truncates toward 0 and only the top is clamped. */
int config_dat_level_e69(const ConfigDat *c)
{
    const int level = (int16_t)word_at(c, 0x00524e69) * 3 / 2;
    return level > 0x7f ? 0x7f : level;
}

/* 00464698 */
int config_dat_level_e67(const ConfigDat *c)
{
    const int level = (int16_t)word_at(c, 0x00524e67) * 12 / 10;
    return level > 0x7f ? 0x7f : level;
}

/* 00465d54 */
int config_dat_controls_clash(const ConfigDat *c)
{
    enum { PAIRED = 3, KEYS = 9 };
    int head[PAIRED], key[KEYS], clash = 0;
    for (int i = 0; i < PAIRED; i++) head[i] = (int16_t)get16(c, OFFSET_CONTROLS + 2 * i);
    for (int i = 0; i < PAIRED; i++) {
        const int a = head[i], b = head[(i + 1) % PAIRED];
        if (a && b && a == b) clash = 1;
    }
    for (int i = 0; i < PAIRED; i++)
        for (int k = 0; k < 2; k++)
            key[2 * i + k] = head[i] ? 2 * i + k : (int16_t)get16(c, OFFSET_CONTROLS + 2 * (PAIRED + 2 * i + k));
    for (int i = 2 * PAIRED; i < KEYS; i++) key[i] = (int16_t)get16(c, OFFSET_CONTROLS + 2 * (PAIRED + i));
    for (int i = 0; i < KEYS; i++)
        for (int j = i + 1; j < KEYS; j++)
            if (key[i] == key[j]) clash = 1;
    return clash;
}
