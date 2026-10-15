/* The race's recording for the replay (race_replay.h). */
#include "game/race/race_replay.h"

#include <string.h>

enum { CLOCK_STEP = 4, BYTE = 8 };
enum { END_NO_SAVE = 0x40, END_NO_SAVE_TOO = 0x20 };  /* 0044eef8: the end states (00509d70) that save no replay */

static void put(uint8_t *recording, size_t size, int64_t at, uint8_t value)
{
    if (at >= 0 && (uint64_t)at < size) recording[at] = value;  /* port guard: the original writes unchecked */
}

void race_replay_record(const RaceReplayWords *w, const uint32_t *inputs, uint8_t *recording, size_t size)
{
    if (w->w4fbb0c == 0 || w->playback == 1) return;
    const int32_t slot = w->clock / CLOCK_STEP;
    for (int32_t i = 0; i < w->humans; i++) {
        const int64_t at = (int64_t)i * REPLAY_SLOTS + slot;
        put(recording, size, at, (uint8_t)inputs[i]);
        put(recording, size, at + REPLAY_HIGH, (uint8_t)(inputs[i] >> BYTE));
    }
}

static uint8_t get(const uint8_t *recording, size_t size, int64_t at)
{
    return at >= 0 && (uint64_t)at < size ? recording[at] : 0;  /* port guard: the original reads unchecked */
}

void race_replay_play(const RaceReplayWords *w, uint32_t *inputs, const uint8_t *recording, size_t size)
{
    const int32_t slot = w->clock / CLOCK_STEP;
    for (int32_t i = 0; i < w->humans; i++) {
        const int64_t at = (int64_t)i * REPLAY_SLOTS + slot;
        inputs[i] = (uint32_t)get(recording, size, at) | (uint32_t)get(recording, size, at + REPLAY_HIGH) << BYTE;
    }
}

/* 0044dc40's copies: snapshot offset, car offset, bytes (the body's 0x94
 * bytes are read where the car's +0xc0 points, its own +0xc4) */
static const struct {
    uint16_t to, from, size;
} SAVED[] = {
    { 0x000, 0x50e, 4 }, { 0x004, 0x512, 4 }, { 0x00c, 0x000, 0xc }, { 0x018, 0x00c, 0xc }, { 0x024, 0x018, 0xc },
    { 0x030, 0x024, 0x24 }, { 0x054, 0x048, 4 }, { 0x058, 0x050, 4 }, { 0x05c, 0x054, 0xc }, { 0x068, 0x060, 0xc },
    { 0x074, 0x070, 0x18 }, { 0x08c, 0x08c, 0x24 }, { 0x0b0, 0x0b0, 0xc }, { 0x0bc, 0x174, 4 }, { 0x0c0, 0x0bc, 4 },
    { 0x0c4, 0x0c4, 0x94 }, { 0x158, 0x15c, 4 }, { 0x15c, 0x158, 4 }, { 0x160, 0x33c, 4 }, { 0x164, 0x4f2, 4 }
};

enum { SLOT_AT = 0x516, FLAGS_AT = 0x51a, PACKED_AT = 8, SLOT_SHIFT = 24, FLAGS_SHIFT = 8, FLAGS_MASK = 0xffff,
       CLEARED_A = 0x405, CLEARED_B = 0x409, NODE_AT = 0x48, PROGRESS_AT = 0x4c, LAP_AT = 0x50, NODES_A_SEGMENT = 4 };

static uint32_t raw32(const uint8_t *b)
{
    return (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24;
}

static void put32(uint8_t *b, uint32_t v)
{
    for (int k = 0; k < 4; k++) b[k] = (uint8_t)(v >> (8 * k));
}

void race_replay_save_car(int32_t w4fbb0c, int16_t playback, const Car *car, uint8_t *snapshot)
{
    if (w4fbb0c == 0 || playback == 1) return;
    const uint8_t *from = (const uint8_t *)car;
    for (size_t i = 0; i < sizeof SAVED / sizeof SAVED[0]; i++)
        memcpy(snapshot + SAVED[i].to, from + SAVED[i].from, SAVED[i].size);
    put32(snapshot + PACKED_AT, raw32(from + SLOT_AT) << SLOT_SHIFT | raw32(from + FLAGS_AT) << FLAGS_SHIFT);
}

void race_replay_load_car(Car *car, const uint8_t *snapshot, int32_t segments)
{
    uint8_t *to = (uint8_t *)car;
    for (size_t i = 0; i < sizeof SAVED / sizeof SAVED[0]; i++)
        memcpy(to + SAVED[i].from, snapshot + SAVED[i].to, SAVED[i].size);
    const int32_t packed = (int32_t)raw32(snapshot + PACKED_AT);
    put32(to + SLOT_AT, (uint32_t)(packed >> SLOT_SHIFT));
    put32(to + FLAGS_AT, (uint32_t)(packed >> FLAGS_SHIFT) & FLAGS_MASK);
    put32(to + CLEARED_A, 0);
    put32(to + CLEARED_B, 0);
    const uint32_t lap = raw32(to + LAP_AT), node = raw32(to + NODE_AT);
    put32(to + PROGRESS_AT, (lap - 1u) * (uint32_t)segments * NODES_A_SEGMENT + node);
}

/* 0044df20's human fields: record offset, car offset (dwords as they are) */
static const struct {
    uint16_t from, to;
} HUMAN_WORDS[] = {
    { 0x170, 0x3c9 }, { 0x174, 0x3cd }, { 0x178, 0x3d1 }, { 0x190, 0x3d9 }, { 0x194, 0x3dd }, { 0x198, 0x3e1 },
    { 0x19c, 0x3e5 }, { 0x18c, 0x3d5 }, { 0x180, 0x40d }, { 0x17c, 0x06c }, { 0x188, 0x401 }, { 0x1a0, 0x4a2 }
};

/* the byte +0x186's bits to the car's dwords */
static const struct {
    uint8_t bit;
    uint16_t to;
} HUMAN_BITS[] = { { 3, 0x3f1 }, { 2, 0x3f5 }, { 1, 0x3f9 }, { 0, 0x3fd } };

enum { HALF = 16, HALF_MASK = 0xffff, BRAKE_BIT = 5, BRAKE_ON = 2, NONE = -1 };

static void restore_human(RaceState *race, Car *car, const uint8_t *r)
{
    uint8_t *c = (uint8_t *)car;
    put32(c + 0x3b1, r[0x168]);
    put32(c + 0x3b5, r[0x169]);
    put32(c + 0x3b9, r[0x16a]);
    for (size_t i = 0; i < sizeof HUMAN_WORDS / sizeof HUMAN_WORDS[0]; i++)
        put32(c + HUMAN_WORDS[i].to, raw32(r + HUMAN_WORDS[i].from));
    put32(c + 0x3e9, r[0x184]);
    put32(c + 0x3bd, (uint32_t)r[0x16b] - 100u);
    put32(c + 0x3ed, (uint32_t)r[0x185] - 2u);
    const uint32_t split = raw32(r + 0x16c);
    put32(c + 0x3c5, (uint32_t)((int32_t)split >> HALF));
    put32(c + 0x3c1, split & HALF_MASK);
    const uint8_t bits = r[0x186];
    c[0x426] = (bits >> BRAKE_BIT & 1) != 0 ? BRAKE_ON : 0;
    for (size_t i = 0; i < sizeof HUMAN_BITS / sizeof HUMAN_BITS[0]; i++)
        put32(c + HUMAN_BITS[i].to, bits >> HUMAN_BITS[i].bit & 1u);
    race->unknown_4c5f98 = NONE;
    c[0x425] = 0;
    race->unknown_4c5f9c = NONE;
}

void race_replay_restore_car(RaceState *race, int32_t human, int32_t minute, int32_t index,
                             const uint8_t *recording, size_t size)
{
    const int64_t at = human != 0
        ? REPLAY_HUMANS_AT + (int64_t)index * REPLAY_HUMAN + (int64_t)minute * REPLAY_HUMAN_MINUTE
        : REPLAY_OTHERS_AT + (int64_t)minute * REPLAY_OTHER_MINUTE + (int64_t)index * REPLAY_OTHER;
    const int64_t length = human != 0 ? REPLAY_HUMAN_MINUTE : REPLAY_OTHER;
    if (at < 0 || (uint64_t)(at + length) > size) return;  /* port guard: the original reads unchecked */
    const uint8_t *r = recording + at;
    const int32_t slot = human != 0 ? index : (int32_t)raw32(r + PACKED_AT) >> SLOT_SHIFT;
    if (slot < 0 || slot >= RACE_CARS || race->cars[slot] == NULL) return;  /* port guard */
    Car *car = race->cars[slot];
    race_replay_load_car(car, r, race->segments);
    if (human != 0) {
        restore_human(race, car, r);
        return;
    }
    uint8_t *c = (uint8_t *)car;
    put32(c + 0x160, raw32(r + 0x168));
    put32(c + 0x164, raw32(r + 0x16c));
    put32(c + 0x3fd, r[0x170]);
    c[0x340] = r[0x171];
}

enum { REPLAY_MINUTE_TICKS = 0x708, BRAKE_FLAG = 0x20 };

static void save_human(const Car *car, uint8_t *r)
{
    const uint8_t *c = (const uint8_t *)car;
    for (size_t i = 0; i < sizeof HUMAN_WORDS / sizeof HUMAN_WORDS[0]; i++)
        put32(r + HUMAN_WORDS[i].from, raw32(c + HUMAN_WORDS[i].to));
    r[0x184] = c[0x3e9];
    r[0x168] = c[0x3b1];
    r[0x169] = c[0x3b5];
    r[0x16a] = c[0x3b9];
    put32(r + 0x16c, raw32(c + 0x3c5) << HALF | raw32(c + 0x3c1));
    uint8_t bits = 0;
    for (size_t i = 0; i < sizeof HUMAN_BITS / sizeof HUMAN_BITS[0]; i++)
        bits |= (uint8_t)(c[HUMAN_BITS[i].to] << HUMAN_BITS[i].bit);
    if ((c[0x426] & BRAKE_ON) != 0) bits |= BRAKE_FLAG;
    r[0x186] = bits;
    r[0x185] = (uint8_t)(c[0x3ed] + 2);
    r[0x16b] = (uint8_t)(c[0x3bd] + 100);
}

void race_replay_save_minute(const RaceState *race, const Car *car, int32_t index, uint8_t *recording, size_t size)
{
    if (race->unknown_4fbb0c == 0 || race->playback == 1) return;
    const int32_t minute = race->clock / REPLAY_MINUTE_TICKS;
    const int32_t slot = (int32_t)raw32((const uint8_t *)car + SLOT_AT);
    const int human = slot < race->humans;
    const int64_t at = human
        ? REPLAY_HUMANS_AT + (int64_t)slot * REPLAY_HUMAN + (int64_t)minute * REPLAY_HUMAN_MINUTE
        : REPLAY_OTHERS_AT + (int64_t)minute * REPLAY_OTHER_MINUTE + (int64_t)index * REPLAY_OTHER;
    const int64_t length = human ? REPLAY_HUMAN_MINUTE : REPLAY_OTHER;
    if (at < 0 || (uint64_t)(at + length) > size) return;  /* port guard: the original writes unchecked */
    uint8_t *r = recording + at;
    race_replay_save_car(race->unknown_4fbb0c, race->playback, car, r);
    if (human) {
        save_human(car, r);
        return;
    }
    const uint8_t *c = (const uint8_t *)car;
    put32(r + 0x168, raw32(c + 0x160));
    put32(r + 0x16c, raw32(c + 0x164));
    r[0x171] = c[0x340];
    r[0x170] = c[0x3fd];
}

enum {
    START_VIEWS = 3, START_52B8F0 = 4, MARK_EVERY = 0x1c2, NO_CAMERA_ROW = -2, START_513730 = 6
};

void race_replay_start(RaceReplayStart *w, uint8_t *(*alloc)(void *ctx, int32_t size), void *ctx)
{
    w->view_taken = 1;
    w->split_views[0] = w->split_views[1] = START_VIEWS;
    w->unknown_52b8ec = 1;
    w->unknown_52b8e8 = START_VIEWS;
    w->unknown_52b8e4 = 1;
    w->unknown_52b8e0 = 0;
    w->unknown_52b8f0 = START_52B8F0;
    if (w->w4fbb0c == 0 || w->playback == 1) return;
    w->recording = alloc(ctx, REPLAY_RECORDING_SIZE);
    w->mark_countdown = MARK_EVERY;
    w->unknown_512f78 = NO_CAMERA_ROW;
    w->unknown_513898 = w->unknown_513718 = 1;
    w->unknown_5120c4 = w->unknown_5120c8 = w->unknown_512f7c = 0;
    w->unknown_513728 = w->unknown_51372c = w->unknown_51371c = 0;
    w->unknown_513894 = w->note_second = 0;
    w->unknown_513730 = w->unknown_513888 = START_513730;
    for (int i = 0; i < RACE_NOTES; i++) w->notes[i] = 0;
    w->camera_roll_add = w->unknown_4c5388 = w->unknown_51371c = 0;
    w->unknown_5137e4 = 0;
}

void race_replay_end(const RaceReplayEndOps *ops, int32_t w4fbb0c, int16_t playback, int32_t end_state,
                     uint8_t *recording)
{
    if (w4fbb0c == 0 || playback == 1) return;
    if (end_state != END_NO_SAVE && end_state != END_NO_SAVE_TOO) ops->save(ops->ctx);
    if (recording != NULL) ops->free(ops->ctx, recording);
}
