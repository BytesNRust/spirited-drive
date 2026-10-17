#include "game/frontend/record_check.h"

#include <string.h>

enum {
    SEGMENT = 0x93, TRACK = 0x95, CAR = 0x9b, NO_RECORDS_CAR = 0xb, TRACK_RECORDS = 0xa6b, SEGMENT_RECORDS = 0x27,
    PLAYER = 0x95, TIME = 0x59, SPEED = 0x61, CAR_A = 0x53a, CAR_B = 0x536, GEARBOX = 0x50c
};

/* the two records' fields, from the segment's row */
typedef struct { int name, time, speed, car_a, car_b, gearbox, set; } RecordAt;
static const RecordAt TIME_RECORD = { 0xb5, 0xd0, 0xd4, 0xc4, 0xc8, 0xd8, 0xcc };
static const RecordAt SPEED_RECORD = { 0x12a, 0x145, 0x149, 0x139, 0x13d, 0x14d, 0x141 };

static int16_t g16(const uint8_t *at) { return (int16_t)(uint16_t)(at[0] | at[1] << 8); }

static int32_t g32(const uint8_t *at)
{
    return (int32_t)((uint32_t)at[0] | (uint32_t)at[1] << 8 | (uint32_t)at[2] << 16 | (uint32_t)at[3] << 24);
}

static void p32(uint8_t *at, int32_t v) { memcpy(at, &(uint32_t){ (uint32_t)v }, 4); }

/* the player's name (strcpy), times and car into the record */
static void take(uint8_t *row, const RecordAt *r, const uint8_t game[RECORD_CHECK_GAME], const uint8_t *me)
{
    size_t n = 0;
    do row[r->name + n] = me[n];
    while (me[n++] != 0);
    p32(row + r->time, g32(me + TIME));
    p32(row + r->speed, g32(me + SPEED));
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
        take(row, &TIME_RECORD, game, me);
        beaten += 1;
    }
    if (g32(row + SPEED_RECORD.speed) < g32(me + SPEED)) {
        take(row, &SPEED_RECORD, game, me);
        beaten += 2;
    }
    return beaten;
}
