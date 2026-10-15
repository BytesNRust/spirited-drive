/* The replay director's cuts (race_replay_cuts.h). */
#include "game/race/race_replay_cuts.h"

enum {
    SECOND_TICKS = 0x3c,
    CLOCK_TICKS = 0x7080,
    PICK_ROUNDS = 30,
    LEVEL_MASK = 0xff,
    LEVEL_HEARD = 0x80,
    START_LEAST = -2,
    START_SECONDS = 60,
    CUT_BEFORE = 2,
    CUT_AFTER = 5,
    CUT_ROUND = 4,
    LONE_CUT_WORD = 200
};

/* 00512efc: the picks, rows 30..39 of the cuts (10 rows of 3 words) */
static int32_t *picks_of(RaceState *race) { return (int32_t *)&race->replay_cuts[REPLAY_PICKS]; }

static int32_t level_of(const RaceState *race, int32_t second)
{
    return second >= 0 && second < RACE_NOTES ? race->notes[second] & LEVEL_MASK : 0;
}

/* The picks, at most PICK_ROUNDS; their count */
static int32_t pick(RaceState *race, int32_t *picks, int32_t seconds, int32_t start)
{
    int32_t count = 0, best = 0;
    for (int round = 0; round < PICK_ROUNDS; round++) {
        int32_t loudest = LEVEL_HEARD;
        for (int32_t i = 0; i < seconds; i++) {
            const int32_t level = race->notes[i] & LEVEL_MASK;
            if (level >= loudest && race->notes[i] >> 8 >= start) {
                picks[count] = race->notes[i] >> 8;
                loudest = level;
                best = i;
            }
        }
        if (loudest > LEVEL_HEARD) {
            count++;
            race->notes[best] -= LEVEL_HEARD;
        }
    }
    return count;
}

static void sort(int32_t *picks, int32_t count)
{
    for (int32_t pass = 0; pass < count; pass++) {
        int swapped = 0;
        for (int32_t j = 1; j < count; j++) {
            if (picks[j] < picks[j - 1]) {
                const int32_t t = picks[j];
                picks[j] = picks[j - 1];
                picks[j - 1] = t;
                swapped = 1;
            }
        }
        if (!swapped) break;
    }
}

static int32_t times60(int32_t s) { return (int32_t)((uint32_t)s * SECOND_TICKS); }

/* One pick's cut at row race->replay_cut_count, merged into the one before
 * when they overlap */
static void cut_at(RaceState *race, int32_t second, int32_t end)
{
    const int32_t row = race->replay_cut_count;
    ReplayCut *cut = &race->replay_cuts[row];
    cut->clock = times60(second - CUT_BEFORE);
    cut->until = times60(second + CUT_AFTER);
    cut->word = level_of(race, second);
    cut->clock -= cut->clock % CUT_ROUND;
    cut->until -= cut->until % CUT_ROUND;
    if (cut->clock < 0) cut->clock = 0;
    if (cut->until > end) cut->until = end;
    if (row >= 1 && race->replay_cuts[row - 1].until > cut->clock) {
        ReplayCut *before = &race->replay_cuts[row - 1];
        before->until = cut->until;
        if (cut->word > before->word) before->word = cut->word;
        return;
    }
    race->replay_cut_count = row + 1;
}

void race_replay_cuts_build(RaceState *race, int32_t end)
{
    int32_t seconds = end / SECOND_TICKS;
    if (seconds > RACE_NOTES) seconds = RACE_NOTES;
    int32_t start = (int32_t)((uint32_t)end - CLOCK_TICKS) / SECOND_TICKS;
    start = start < START_LEAST ? START_LEAST : start + START_SECONDS;
    int32_t *picks = picks_of(race);
    const int32_t count = pick(race, picks, seconds, start);
    for (int32_t i = 0; i < seconds; i++) {
        const int32_t level = race->notes[i] & LEVEL_MASK;
        if (level > 0 && level < LEVEL_HEARD) race->notes[i] += LEVEL_HEARD;
    }
    sort(picks, count);
    race->replay_cut_count = 0;
    for (int32_t k = 0; k < count; k++)
        if (picks[k] - CUT_BEFORE >= start) cut_at(race, picks[k], end);
    if (race->replay_cut_count == 0) {
        race->replay_cuts[0] = (ReplayCut){ 0, end, LONE_CUT_WORD };
        race->replay_cut_count = 1;
    }
}
