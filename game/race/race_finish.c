/* A car's finish, checked once a tick for each car racing (00440c30). */
#include "game/race/race_finish.h"

#include "game/race/car_control.h"

#include <stddef.h>
#include <string.h>

enum {
    RECORD = 0x1d2,               /* sizeof(PlayerRecord) */
    FINISH = 0x1c4,               /* PlayerRecord.finish */
    MARK_FINISHED = 4,            /* the lap mark the finish goes into: +4 + lap x 4 */
    MARK_OUT = 0xc,               /* out before the start: +0xc + lap x 4 */
    PLACE = RECORD,               /* the place is kept one record on, at the next player's +0 (player 7's
                                   * in the 9th record, 0051d0e4) */
    OUT = -999,
    FINISH_OFFSET = 300,          /* ticks taken off the finish, as the lap marks (0043fa3c) */
    BEFORE_START = 1,             /* car_race_window */
    PAST_FINISH = 2,
    PLACES = 8
};

static const char *const PLACE_SPEECH[PLACES] = {
    "first.eas", "second.eas", "third.eas", "fourth.eas", "fifth.eas", "sixth.eas", "seventh.eas", "eighth.eas"
};

/* The player records as the original's bytes: a word at `at`, kept inside
 * the records (port guard: the original reads and writes past them). */
static int in_records(int64_t at)
{
    return at >= 0 && at + 4 <= (int64_t)sizeof(((RaceState *)0)->players);
}

static int32_t read_word(const RaceState *race, int64_t at)
{
    int32_t value = 0;
    if (in_records(at)) memcpy(&value, (const uint8_t *)race->players + at, sizeof value);
    return value;
}

static void write_word(RaceState *race, int64_t at, int32_t value)
{
    if (in_records(at)) memcpy((uint8_t *)race->players + at, &value, sizeof value);
}

/* How many of the racers finished before `finish`. */
static int32_t finished_ahead(const RaceState *race, int32_t finish)
{
    int32_t ahead = 0;
    for (int32_t i = 0; i < race->racers; i++) {
        const int32_t other = read_word(race, (int64_t)i * RECORD + FINISH);
        if (other != 0 && finish > other) ahead++;
    }
    return ahead;
}

void race_car_finish(Car *car, RaceState *race)
{
    const int32_t player = car->player;
    const int64_t record = (int64_t)player * RECORD;
    if (read_word(race, record + FINISH) != 0) return;
    int32_t place = -1;
    const int window = car_race_window(car, race);
    if (window == BEFORE_START && player < race->finish_humans) {
        write_word(race, record + MARK_OUT + (int64_t)car->lap * 4, OUT);
        write_word(race, record + FINISH, OUT);
    } else if (window == PAST_FINISH) {
        const int32_t finish = race->tick - FINISH_OFFSET;
        write_word(race, record + MARK_FINISHED + (int64_t)car->lap * 4, finish);
        write_word(race, record + FINISH, finish);
        if (race->race_type != 0) {
            if (race->replay != 0) {
                if (player == race->viewed) place = read_word(race, record + PLACE) - 1;
            } else if (player == race->player) {
                place = finished_ahead(race, finish);
                write_word(race, record + PLACE, place + 1);
            }
        }
    }
    if (place == -1) return;
    /* port guard: the original takes the name from its stack past the 8 */
    if (place >= 0 && place < PLACES && race->sounds.speech != NULL)
        race->sounds.speech(race->sounds.user, PLACE_SPEECH[place]);
}
