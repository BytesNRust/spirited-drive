#include "game/frontend/record_check.h"

#include <string.h>

enum {
    SEGMENT = 0x93, TRACK = 0x95, CAR = 0x9b, NO_RECORDS_CAR = 0xb, TRACK_RECORDS = 0xa6b, SEGMENT_RECORDS = 0x27,
    PLAYER = 0x95, NAME_AT = 0, LAPS = 9, LAP_COUNT = 20, TIME = 0x59, SPEED = 0x61, TOTAL = 0x65, TOP = 0x6d, CAR_A = 0x53a,
    CAR_B = 0x536, GEARBOX = 0x50c
};

/* the two records' fields, from the segment's row */
typedef struct { int name, time, speed, car_a, car_b, gearbox, set; } RecordAt;
static const RecordAt TIME_RECORD = { 0xb5, 0xd0, 0xd4, 0xc4, 0xc8, 0xd8, 0xcc };
static const RecordAt SPEED_RECORD = { 0x12a, 0x145, 0x149, 0x139, 0x13d, 0x14d, 0x141 };
/* a circuit's top speed record, past the segments' rows */
static const RecordAt TOP_RECORD = { 0x631, 0x64c, 0x650, 0x640, 0x644, 0x654, 0x648 };

static int16_t g16(const uint8_t *at) { return (int16_t)(uint16_t)(at[0] | at[1] << 8); }

static int32_t g32(const uint8_t *at)
{
    return (int32_t)((uint32_t)at[0] | (uint32_t)at[1] << 8 | (uint32_t)at[2] << 16 | (uint32_t)at[3] << 24);
}

static void p32(uint8_t *at, int32_t v) { memcpy(at, &(uint32_t){ (uint32_t)v }, 4); }

/* the player's name (strcpy; the original's sprintf, the names having no '%'), time, speed and car into the record */
static void take(uint8_t *row, const RecordAt *r, const uint8_t game[RECORD_CHECK_GAME], const uint8_t *me,
                 int32_t time, int32_t speed)
{
    size_t n = 0;
    do row[r->name + n] = me[NAME_AT + n];
    while (me[NAME_AT + n++] != 0);
    p32(row + r->time, time);
    p32(row + r->speed, speed);
    p32(row + r->car_a, g16(game + CAR_A));
    p32(row + r->car_b, g16(game + CAR_B));
    p32(row + r->gearbox, g16(game + GEARBOX));
    p32(row + r->set, 1);
}

/* 0045bad8 */
int32_t record_check(uint8_t image[RECORD_CHECK_IMAGE], const uint8_t game[RECORD_CHECK_GAME], int32_t player)
{
    const int16_t segment_word = g16(image + SEGMENT);
    const int segment = segment_word ? (int16_t)(segment_word - 1) : 2;
    if (g16(image + CAR) == NO_RECORDS_CAR) return 0;
    uint8_t *const row = image + TRACK_RECORDS * g16(image + TRACK) + SEGMENT_RECORDS * segment;
    const uint8_t *const me = game + PLAYER * player;
    int32_t beaten = 0;
    const int32_t time = g32(me + TIME);
    if (time < g32(row + TIME_RECORD.time) && time > 0) {
        take(row, &TIME_RECORD, game, me, g32(me + TIME), g32(me + SPEED));
        beaten += 1;
    }
    if (g32(row + SPEED_RECORD.speed) < g32(me + SPEED)) {
        take(row, &SPEED_RECORD, game, me, g32(me + TIME), g32(me + SPEED));
        beaten += 2;
    }
    return beaten;
}

/* 0045b908 */
int32_t record_check_lap(uint8_t image[RECORD_CHECK_IMAGE], const uint8_t game[RECORD_CHECK_GAME], int32_t player)
{
    const uint8_t *const me = game + PLAYER * player;
    int best = 0;
    for (int i = 0; i < LAP_COUNT; i++) {
        const int32_t lap = g32(me + LAPS + 4 * i);
        if (lap < g32(me + LAPS + 4 * best) && lap > 0) best = i;
    }
    if (g16(image + CAR) == NO_RECORDS_CAR) return 0;
    uint8_t *const row = image + TRACK_RECORDS * g16(image + TRACK);
    const int32_t lap = g32(me + LAPS + 4 * best);
    if (!(lap < g32(row + TIME_RECORD.time) && lap > 0)) return 0;
    take(row, &TIME_RECORD, game, me, lap, g32(me + TOP));
    return 1;
}

/* 0045c2d0 */
int32_t record_check_top(uint8_t image[RECORD_CHECK_IMAGE], const uint8_t game[RECORD_CHECK_GAME], int32_t player)
{
    if (g16(image + CAR) == NO_RECORDS_CAR) return 0;
    uint8_t *const row = image + TRACK_RECORDS * g16(image + TRACK);
    const uint8_t *const me = game + PLAYER * player;
    if (!(g32(row + TOP_RECORD.speed) < g32(me + TOP))) return 0;
    take(row, &TOP_RECORD, game, me, g32(me + TOTAL), g32(me + TOP));
    return 1;
}
