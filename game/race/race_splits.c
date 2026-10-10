/* The split times (race_splits.h). */
#include "game/race/race_split_times.h"

#include <stddef.h>
#include <string.h>

/* The block as the original's bytes: the counters index it unchecked, so a
 * word is read or written at its byte offset, kept inside the block (port
 * guard: the original reaches the globals around it). */
static int32_t word_at(const RaceSplits *s, int64_t at)
{
    int32_t v = 0;
    if (at >= 0 && at + 4 <= (int64_t)sizeof *s) memcpy(&v, (const uint8_t *)s + at, sizeof v);
    return v;
}

static void set_word(RaceSplits *s, int64_t at, int32_t v)
{
    if (at >= 0 && at + 4 <= (int64_t)sizeof *s) memcpy((uint8_t *)s + at, &v, sizeof v);
}

/* The tick into `ticks`[*counter] once `progress` is past checkpoint
 * [*counter], then the counter on (read again: the tick may have landed on
 * it). */
static void pass(RaceSplits *s, int32_t progress, size_t ticks, size_t counter, int32_t tick)
{
    const int64_t at = (int32_t)((uint32_t)word_at(s, (int64_t)counter) << 2);  /* shl 2, as the original */
    if (progress <= word_at(s, offsetof(RaceSplits, checkpoints) + at)) return;
    set_word(s, (int64_t)ticks + at, tick);
    set_word(s, (int64_t)counter, word_at(s, (int64_t)counter) + 1);
}

void race_splits_pass(RaceState *race, int32_t progress, int32_t rival_progress)
{
    RaceSplits *s = &race->splits;
    pass(s, progress, offsetof(RaceSplits, passed_ticks), offsetof(RaceSplits, passed), race->tick);
    if (race->race_type != 0)
        pass(s, rival_progress, offsetof(RaceSplits, rival_ticks), offsetof(RaceSplits, rival_passed), race->tick);
}
