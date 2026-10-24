/* port: see records_route.h. */
#include "game/frontend/records_route.h"
#include "game/sys/port_log.h"

#include <stdio.h>
#include <string.h>

enum {
    TRACK = 0x95, CAR = 0x9b, SEGMENT = 0x93, DIFFICULTY = 0xb3, FIRST_BLOCK = 0xb5, CIRCUIT_FROM = 3, ROWS = 10,
    WARRIOR = 11
};

static int16_t g16(const uint8_t *at) { return (int16_t)(uint16_t)(at[0] | at[1] << 8); }

int records_slot_totals(int track, int choice, int row)
{
    if (row < 0 || row >= ROWS || choice < 0 || choice > 2) return -1;
    return SLOT_TOTALS + (track >= CIRCUIT_FROM ? SLOT_CHOICE * choice : 0) + row;
}

int records_has_cheats(RaceKind kind) { return kind.marks != 0; }

RaceKind records_kind_with_car(RaceKind kind, int car)
{
    if (car == WARRIOR) kind.marks |= EXTRAS_MARK_WARRIOR;
    return kind;
}

RaceKind records_race_kind(int variant_m, int variant_r, int explode, int nitro, int turbo)
{
    RaceKind k = { variant_r ? RECORDS_RALLY : variant_m ? RECORDS_MIRROR : RECORDS_NORMAL, 0 };
    if (nitro) k.marks |= EXTRAS_MARK_NITRO;
    if (explode) k.marks |= EXTRAS_MARK_EXPLODE;
    if (turbo) k.marks |= EXTRAS_MARK_TURBO;
    return k;
}

static int32_t original(RouteOp op, uint8_t *image, const uint8_t *game, int32_t player, const int32_t *times)
{
    switch (op) {
    case ROUTE_SEGMENT: return record_check(image, game, player);
    case ROUTE_LAP: return record_check_lap(image, game, player);
    case ROUTE_TOP: return record_check_top(image, game, player);
    default: return record_check_totals(image, game, player, times);
    }
}

/* the answer when there was nothing to take */
static int32_t nothing(RouteOp op) { return op == ROUTE_TOTALS ? RECORD_CHECK_NONE : 0; }

/* marks follow the rows: a totals row taken at `row` pushes the ones below it down, the tenth gone. A segment check
 * marks the time (1) and the top speed (2) rows it took, as its answer says. */
static void mark(RaceKind kind, RouteOp op, int track, int segment_word, int choice, int32_t answer, uint8_t *marks)
{
    if (op == ROUTE_SEGMENT) {
        const int segment = segment_word ? segment_word - 1 : 2;
        if (answer & 1) marks[SLOT_SEGMENT + segment] = (uint8_t)kind.marks;
        if (answer & 2) marks[SLOT_SPEED_SEGMENT + segment] = (uint8_t)kind.marks;
    } else if (op == ROUTE_LAP) {
        marks[SLOT_SEGMENT] = (uint8_t)kind.marks;
    } else if (op == ROUTE_TOP) {
        marks[SLOT_TOP] = (uint8_t)kind.marks;
    } else {
        const int base = records_slot_totals(track, choice, 0);
        for (int j = ROWS - 1; j > answer; j--) marks[base + j] = marks[base + j - 1];
        marks[base + answer] = (uint8_t)kind.marks;
    }
}

int32_t records_route(RaceKind kind, RouteOp op, uint8_t image[RECORD_CHECK_IMAGE],
                      const uint8_t game[RECORD_CHECK_GAME], int32_t player, const int32_t times[RECORD_CHECK_TIMES])
{
    kind = records_kind_with_car(kind, g16(image + CAR));
    if (kind.variant == RECORDS_NORMAL && !records_has_cheats(kind)) return original(op, image, game, player, times);
    static uint8_t scratch[RECORD_CHECK_IMAGE];
    const int track = g16(image + TRACK);
    uint8_t *const block = port_records_block(track, kind.variant, records_has_cheats(kind));
    uint8_t *const marks = port_records_marks(track, kind.variant);
    if (block == NULL || marks == NULL) return nothing(op);
    memcpy(scratch, image, sizeof scratch);
    memcpy(scratch + FIRST_BLOCK + RECORDS_BLOCK * track, block, RECORDS_BLOCK);
    /* record_check's early return is the original's no-records car: the Warrior's times are the port's, so the check
     * runs as a car the original records (the image's car word is read by nothing else in the check) */
    if (g16(scratch + CAR) == WARRIOR) scratch[CAR] = 0, scratch[CAR + 1] = 0;
    const int32_t answer = original(op, scratch, game, player, times);
    if (answer == nothing(op)) return answer;
    memcpy(block, scratch + FIRST_BLOCK + RECORDS_BLOCK * track, RECORDS_BLOCK);
    if (records_has_cheats(kind))
        mark(kind, op, track, g16(image + SEGMENT), g16(image + DIFFICULTY), answer, marks);
    if (!port_records_save()) port_log("records: records.dat could not be saved\n");
    return answer;
}

void records_mark_text(int marks, char out[5])
{
    int n = 0;
    if (marks & EXTRAS_MARK_NITRO) out[n++] = 'N';
    if (marks & EXTRAS_MARK_EXPLODE) out[n++] = 'E';
    if (marks & EXTRAS_MARK_TURBO) out[n++] = 'T';
    if (marks & EXTRAS_MARK_WARRIOR) out[n++] = 'W';
    out[n] = '\0';
}
