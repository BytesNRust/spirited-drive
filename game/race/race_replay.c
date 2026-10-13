/* The race's recording for the replay (race_replay.h). */
#include "game/race/race_replay.h"

#include <string.h>

enum { CLOCK_STEP = 4, BYTE = 8 };

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
