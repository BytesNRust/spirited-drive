#include "game/race/car_progress.h"

#include <string.h>

enum { LAP_TIME_OFFSET = 300 };  /* the lap marks are stored 300 ticks early */

/* Squared distance from the car to the middle of the road edge between nodes
 * a and a + 1, in 1/256 units, 32-bit and compared unsigned as the original. */
static uint32_t edge_distance(const Car *car, const RaceState *race, int32_t a)
{
    const TriNode p = race_node(race, a), q = race_node(race, a + 1);
    const int32_t mx = (p.position.x + q.position.x) >> 1, mz = (p.position.z + q.position.z) >> 1;
    const int32_t dz = (car->z - mz) >> 8, dx = (car->x - mx) >> 8;
    return (uint32_t)(dz * dz) + (uint32_t)(dx * dx);
}

int car_find_node(const Car *car, const RaceState *race, int32_t *node)
{
    const int32_t mask = (int32_t)(race->tri->ring_nodes - 1u);
    int moved = 0;
    if (*node != -1) {
        int32_t at;
        do {
            at = *node;
            const uint32_t here = edge_distance(car, race, at);
            if (edge_distance(car, race, at + 1) < here) {
                *node = at + 1;
                moved = 1;
            } else if ((at > 0 || race->circuit) && edge_distance(car, race, at - 1) < here) {
                *node = at - 1;
                moved = 1;
            }
        } while (at != *node);
    }
    if (*node & ~mask) {
        *node &= mask;
        moved = 1;
    }
    return moved;
}

/* The lap marks as the original addresses them: a 32-bit word at a byte
 * offset into the player records (0051cc54), so lap 0 and other edge cases
 * read the neighbouring fields. Offsets outside the records are dropped. */
static int mark_offset(const Car *car, int32_t lap, int32_t *offset)
{
    const int64_t at = (int64_t)car->player * (int64_t)sizeof(PlayerRecord) + 4 + (int64_t)lap * 4;
    if (at < 0 || at + 4 > (int64_t)sizeof(((RaceState *)0)->players)) return 0;
    *offset = (int32_t)at;
    return 1;
}

static int32_t read_mark(const RaceState *race, const Car *car, int32_t lap)
{
    int32_t offset, value = 0;
    if (mark_offset(car, lap, &offset)) memcpy(&value, (const uint8_t *)race->players + offset, sizeof value);
    return value;
}

static void write_mark(RaceState *race, const Car *car, int32_t lap, int32_t value)
{
    int32_t offset;
    if (mark_offset(car, lap, &offset)) memcpy((uint8_t *)race->players + offset, &value, sizeof value);
}

static void speak(const RaceState *race, const char *name)
{
    if (race->sounds.speech != NULL) race->sounds.speech(race->sounds.user, name);
}

/* Crossing the line forwards: the lap's end is marked, and a lap record or
 * the last lap is announced for the car on screen. */
static void lap_done(Car *car, RaceState *race)
{
    write_mark(race, car, car->lap + 1, race->tick - LAP_TIME_OFFSET);
    const int32_t lap = ++car->lap;
    const int watched = race->replay == 0 ? race->player == car->player : race->viewed == car->player;
    if (!watched) return;
    const int32_t time = read_mark(race, car, lap) - read_mark(race, car, lap - 1);
    if (time < race->best_lap) {
        race->best_lap = time;
        race->best_lap_set = 1;
        if (lap < race->laps) {
            speak(race, "besttime.eas");
        } else if (lap == race->laps) {
            speak(race, "bestlast.eas");
        }
    } else if (lap == race->laps) {
        speak(race, "finallap.eas");
    }
}

int car_progress(Car *car, RaceState *race)
{
    const int32_t before = car->node;
    int32_t node = before;
    const int moved = car_find_node(car, race, &node);
    car->node = node;
    if (!race->circuit) {
        car->progress = node;
        return moved;
    }
    if (before > race->loop_nodes - 2 && node < 2) {
        lap_done(car, race);
    } else if (before < 2 && node > race->loop_nodes - 2) {
        car->lap--;
    }
    car->progress = (car->lap - 1) * race->segments * 4 + car->node;
    return moved;
}
