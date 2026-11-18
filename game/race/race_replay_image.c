#include "game/race/race_replay_image.h"

#include "formats/mod_ini.h"
#include "game/race/car_registry.h"
#include "game/race/difficulty.h"
#include "game/race/rally_gravel.h"

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

enum {
    CAR_LAST = 11,  /* the Warrior */
    CHEATS_MARK = 0xa500, CHEATS_EXPLODE = 1, CHEATS_NITRO = 2,  /* the two padding bytes after the seed: 0 in the CD's replays */
    /* the setup, from 004fb7bc */
    AT_VARIANT = 0x2, AT_TRACK = 0x6, AT_NAME = 0xa, AT_TYPE = 0x14, AT_4FB7D4 = 0x18, AT_FLAGS = 0x1c,
    AT_RACERS = 0x20, AT_HUMANS = 0x24, AT_COPS = 0x28, AT_CARS = 0x2c, AT_TRAFFIC = 0x30, AT_4FB7F0 = 0x34,
    AT_SEED = 0x38, AT_PORT_CHEATS = 0x3c, AT_ENTRIES = 0x3e, AT_GRID = 0x294, AT_LETTERS = 0x2b4, AT_MODELS = 0x2d8,
    AT_CATCH_UP = 0x304, AT_LAPS = 0x308, AT_FOLLOWED = 0x310, AT_4FBAD0 = 0x314, AT_RECORD_SLOT = 0x318,
    AT_MIRRORED = 0x31c, AT_ARCADE = 0x320, AT_TIME_OF_DAY = 0x324, TRACKS = 9,
    /* the seek block, from 005120c4 */
    AT_END_CLOCK = 0x4, AT_COUNTDOWN = 0x10, AT_MARKS = 0x14, AT_SAVED_TIMER = 0xcbc, AT_SAVED_INPUT = 0xcc0,
    AT_SAVED_DUE = 0xcc4,
    /* the cuts, from 00512d94 */
    AT_CUT_COUNT = 0x1e0, AT_512F78 = 0x1e4, AT_512F7C = 0x1e8, AT_SAVED_KIND = 0x1ec, AT_MARK_TICKS = 0x1f0,
    AT_NOTES = 0x200, AT_NOTE_SECOND = 0x980, AT_FRAMES = 0x984, AT_51371C = 0x988, AT_UNTIL = 0x98c,
    AT_TARGET = 0x990, AT_513728 = 0x994, AT_51372C = 0x998, AT_513730 = 0x99c, AT_513734 = 0x9a0,
    /* the records start 8 bytes into 0051cc54's first and end at 0051dcbe: 6 bytes, then 0051dcbc's word */
    RECORDS_SKIP = 8, RECORDS_BYTES = REPLAY_RECORDS * REPLAY_RECORD_SIZE, AT_CBC = RECORDS_BYTES - 2
};

static int32_t get32(const uint8_t *b, int at)
{
    return (int32_t)((uint32_t)b[at] | (uint32_t)b[at + 1] << 8 | (uint32_t)b[at + 2] << 16 | (uint32_t)b[at + 3] << 24);
}

static void put32(uint8_t *b, int at, int32_t v)
{
    for (int i = 0; i < 4; i++) b[at + i] = (uint8_t)((uint32_t)v >> (8 * i));
}

static void get_words(const uint8_t *b, int at, int32_t *w, int n)
{
    for (int i = 0; i < n; i++) w[i] = get32(b, at + 4 * i);
}

static void put_words(uint8_t *b, int at, const int32_t *w, int n)
{
    for (int i = 0; i < n; i++) put32(b, at + 4 * i, w[i]);
}

int race_replay_image_race(const uint8_t setup[REPLAY_SETUP_SIZE], ReplayRace *out)
{
    memset(out, 0, sizeof *out);
    for (int i = 0; i < REPLAY_TRACK_NAME && setup[AT_NAME + i] != 0; i++)
        out->stem[i] = (char)toupper(setup[AT_NAME + i]);
    RaceField *f = &out->field;
    f->racers = get32(setup, AT_RACERS), f->cops = get32(setup, AT_COPS), f->traffic = get32(setup, AT_TRAFFIC);
    f->seed = (uint32_t)get32(setup, AT_SEED), f->race_type = get32(setup, AT_TYPE), f->humans = get32(setup, AT_HUMANS);
    memcpy(f->entries, setup + AT_ENTRIES, sizeof f->entries);
    out->laps = get32(setup, AT_LAPS), out->followed = get32(setup, AT_FOLLOWED);
    out->track_flags = (uint32_t)get32(setup, AT_FLAGS);
    const unsigned cheats = (unsigned)(setup[AT_PORT_CHEATS] | setup[AT_PORT_CHEATS + 1] << 8);
    const int marked = (cheats & 0xff00) == CHEATS_MARK;
    out->explode = marked && (cheats & CHEATS_EXPLODE) != 0;
    out->nitro = marked && (cheats & CHEATS_NITRO) != 0;
    f->port_difficulty = marked ? difficulty_unstamp(cheats & 0xff) : difficulty_unstamp(0);   /* port: the race's difficulty, Original in the CD's */
    f->mirrored = get32(setup, AT_MIRRORED) != 0, out->arcade = get32(setup, AT_ARCADE) != 0;
    if (out->stem[0] == 0 || f->racers < 1 || f->racers > RACE_PLAYERS || f->cops < 0 || f->cops > 1) return 0;
    if (f->traffic < 0 || f->racers + f->cops + f->traffic > RACE_CARS + 6) return 0;
    if (f->humans < 1) f->humans = 1;  /* a setup that wrote none races its one human */
    if (f->humans > f->racers) return 0;
    if (f->race_type < 0 || f->race_type > 3 || out->laps < 1 || out->followed < 0 || out->followed >= f->racers) return 0;
    for (int i = 0; i < RACE_PLAYERS; i++)
        if (f->entries[i].car < 0 || f->entries[i].car > CAR_LAST) return 0;
    return 1;
}

static const char PAINT_MAGIC[8] = { 'T', 'N', 'F', 'S', 'P', 'N', 'T', '1' };

void race_replay_paint_put(const uint8_t paint[RACE_PLAYERS], uint8_t out[REPLAY_PAINT_TRAILER_SIZE])
{
    memcpy(out, PAINT_MAGIC, sizeof PAINT_MAGIC);
    memcpy(out + sizeof PAINT_MAGIC, paint, RACE_PLAYERS);
}

int race_replay_paint_get(const uint8_t *file, size_t size, uint8_t paint[RACE_PLAYERS])
{
    memset(paint, REPLAY_PAINT_NONE, RACE_PLAYERS);
    if (file == NULL || size < (size_t)REPLAY_FILE_SIZE + REPLAY_PAINT_TRAILER_SIZE || memcmp(file + REPLAY_FILE_SIZE, PAINT_MAGIC, sizeof PAINT_MAGIC) != 0) return 0;
    memcpy(paint, file + REPLAY_FILE_SIZE + sizeof PAINT_MAGIC, RACE_PLAYERS);
    return 1;
}

static const char SNOW_MAGIC[8] = { 'T', 'N', 'F', 'S', 'S', 'N', 'W', '1' };

void race_replay_snow_put(int snow, uint8_t out[REPLAY_SNOW_TRAILER_SIZE])
{
    memset(out, 0, REPLAY_SNOW_TRAILER_SIZE);
    memcpy(out, SNOW_MAGIC, sizeof SNOW_MAGIC);
    out[sizeof SNOW_MAGIC] = snow != 0;
}

int race_replay_snow_get(const uint8_t *file, size_t size, int *snow)
{
    const size_t at = (size_t)REPLAY_FILE_SIZE + REPLAY_PAINT_TRAILER_SIZE;
    *snow = 0;
    if (file == NULL || size < at + REPLAY_SNOW_TRAILER_SIZE || memcmp(file + at, SNOW_MAGIC, sizeof SNOW_MAGIC) != 0) return 0;
    *snow = file[at + sizeof SNOW_MAGIC] == 1;
    return 1;
}

static const char CONDITIONS_MAGIC[8] = { 'T', 'N', 'F', 'S', 'C', 'N', 'D', '1' };

void race_replay_conditions_put(const ReplayConditions *c, uint8_t out[REPLAY_CONDITIONS_TRAILER_SIZE])
{
    memset(out, 0, REPLAY_CONDITIONS_TRAILER_SIZE);
    memcpy(out, CONDITIONS_MAGIC, sizeof CONDITIONS_MAGIC);
    out[8] = (uint8_t)c->light_time, out[9] = (uint8_t)c->weather, out[10] = c->reverse != 0;
    if (!c->extras_known) return;   /* port (replay extras): behind their marker, else the padding an older port wrote */
    out[11] = 1, out[12] = c->moon != 0, out[13] = c->lunar_gravity != 0, out[14] = (uint8_t)c->gravel;
}

int race_replay_conditions_get(const uint8_t *file, size_t size, ReplayConditions *c)
{
    const size_t at = (size_t)REPLAY_FILE_SIZE + REPLAY_PAINT_TRAILER_SIZE + REPLAY_SNOW_TRAILER_SIZE;
    *c = (ReplayConditions){ 0, 0, 0, 0, 0, 0, 0 };
    if (file == NULL || size < at + REPLAY_CONDITIONS_TRAILER_SIZE || memcmp(file + at, CONDITIONS_MAGIC, sizeof CONDITIONS_MAGIC) != 0) return 0;
    if (file[at + 8] > 3 || file[at + 9] > 3) return 0;   /* not a value of ours */
    c->light_time = file[at + 8], c->weather = file[at + 9], c->reverse = file[at + 10] == 1;
    const uint8_t *x = file + at + 11;   /* port (replay extras): known only with the marker and values of ours */
    if (x[0] == 1 && x[1] <= 1 && x[2] <= 1 && x[3] <= RALLY_GRAVEL_SNOW)
        c->extras_known = 1, c->moon = x[1], c->lunar_gravity = x[2], c->gravel = x[3];
    return 1;
}

static const char CARS_MAGIC[REPLAY_CARS_MAGIC] = { 'T', 'N', 'F', 'S', 'C', 'A', 'R', '1' };
enum { CARS_ENTRY = REPLAY_CARS_ID + 4 };

void race_replay_cars_write(uint8_t block[REPLAY_CARS_BLOCK], const RaceField *field)
{
    memset(block, 0, REPLAY_CARS_BLOCK);
    memcpy(block, CARS_MAGIC, REPLAY_CARS_MAGIC);
    for (int i = 0; i < RACE_PLAYERS; i++) {
        const CarPiece *p = car_registry_piece(field->entries[i].car);
        if (p == NULL) continue;
        uint8_t *const at = block + REPLAY_CARS_MAGIC + CARS_ENTRY * i;
        memcpy(at, p->id, strnlen(p->id, REPLAY_CARS_ID - 1));
        put32(at, REPLAY_CARS_ID, (int32_t)p->fingerprint);
    }
}

/* entry `i` of `block`: 0 an original's (all zeros), 1 an added car's (its id in `id`), -1 neither */
static int cars_entry(const uint8_t block[REPLAY_CARS_BLOCK], int i, char id[REPLAY_CARS_ID], uint32_t *print)
{
    const uint8_t *const at = block + REPLAY_CARS_MAGIC + CARS_ENTRY * i;
    *print = (uint32_t)get32(at, REPLAY_CARS_ID);
    if (at[0] == 0) {
        for (int k = 0; k < CARS_ENTRY; k++)
            if (at[k] != 0) return -1;
        return 0;
    }
    if (memchr(at, 0, REPLAY_CARS_ID) == NULL) return -1;
    memcpy(id, at, REPLAY_CARS_ID);
    return mod_ini_id_ok(id) ? 1 : -1;
}

ReplayCars race_replay_cars_read(const uint8_t block[REPLAY_CARS_BLOCK], RaceField *field, char *id_out)
{
    int car[RACE_PLAYERS];
    id_out[0] = '\0';
    if (memcmp(block, CARS_MAGIC, REPLAY_CARS_MAGIC) != 0) return REPLAY_CARS_BAD;
    for (int i = 0; i < RACE_PLAYERS; i++) {   /* the whole block first: a bad one changes nothing */
        char id[REPLAY_CARS_ID];
        uint32_t print = 0;
        const int kind = cars_entry(block, i, id, &print);
        if (kind < 0) return REPLAY_CARS_BAD;
        car[i] = field->entries[i].car;
        if (kind == 0) continue;
        car[i] = car_registry_find(id);
        const CarPiece *p = car_registry_piece(car[i]);
        if (p == NULL || p->fingerprint != print) {
            snprintf(id_out, REPLAY_CARS_ID, "%s", id);
            return p == NULL ? REPLAY_CARS_MISSING : REPLAY_CARS_CHANGED;
        }
    }
    for (int i = 0; i < RACE_PLAYERS; i++) field->entries[i].car = car[i];
    return REPLAY_CARS_OK;
}

ReplayCars race_replay_image_race_cars(const uint8_t setup[REPLAY_SETUP_SIZE], const uint8_t block[REPLAY_CARS_BLOCK], ReplayRace *out,
                                       char *id_out)
{
    static uint8_t copy[REPLAY_SETUP_SIZE];
    id_out[0] = '\0';
    memset(out, 0, sizeof *out);
    if (memcmp(block, CARS_MAGIC, REPLAY_CARS_MAGIC) != 0) return REPLAY_CARS_BAD;
    memcpy(copy, setup, sizeof copy);
    for (int i = 0; i < RACE_PLAYERS; i++) {   /* a named entry's car word is the run's number: 0 for the setup's own check */
        char id[REPLAY_CARS_ID];
        uint32_t print = 0;
        const int kind = cars_entry(block, i, id, &print);
        if (kind < 0) return REPLAY_CARS_BAD;
        if (kind == 1) put32(copy, AT_ENTRIES + (int)(sizeof(RaceEntry) * (size_t)i + offsetof(RaceEntry, car)), 0);
    }
    if (!race_replay_image_race(copy, out)) return REPLAY_CARS_BAD;
    return race_replay_cars_read(block, &out->field, id_out);
}

void race_replay_apply_cheats(RaceState *race, const ReplayRace *replay)
{
    race->unknown_4c5cd4 = replay->explode;
    race->start_input = replay->nitro;
}

void race_replay_image_setup(const RaceState *race, uint32_t seed, uint8_t setup[REPLAY_SETUP_SIZE])
{
    memset(setup, 0, REPLAY_SETUP_SIZE);
    put32(setup, AT_VARIANT, race->track_variant), put32(setup, AT_TRACK, race->track);
    for (int i = 0; i < REPLAY_TRACK_NAME - 1 && race->track_name[i] != 0; i++)
        setup[AT_NAME + i] = (uint8_t)tolower((unsigned char)race->track_name[i]);
    put32(setup, AT_TYPE, race->race_type), put32(setup, AT_4FB7D4, race->unknown_4fb7d4);
    put32(setup, AT_FLAGS, (int32_t)race->track_flags), put32(setup, AT_RACERS, race->racers);
    put32(setup, AT_HUMANS, race->humans), put32(setup, AT_COPS, race->cops), put32(setup, AT_CARS, race->car_count);
    put32(setup, AT_TRAFFIC, race->traffic_setting), put32(setup, AT_4FB7F0, race->unknown_4fb7f0);
    put32(setup, AT_SEED, (int32_t)seed);
    const unsigned difficulty = difficulty_stamp(&race->port_difficulty);
    if (race->unknown_4c5cd4 != 0 || race->start_input != 0 || difficulty != 0) {   /* port: the EXPLODE and NITRO cheats' words are not in the
                                                                   * exe's setup; a replay plays without them */
        setup[AT_PORT_CHEATS] = (uint8_t)((race->unknown_4c5cd4 != 0 ? CHEATS_EXPLODE : 0) | (race->start_input != 0 ? CHEATS_NITRO : 0) | difficulty);
        setup[AT_PORT_CHEATS + 1] = CHEATS_MARK >> 8;
    }
    memcpy(setup + AT_ENTRIES, race->entries, sizeof race->entries);
    put_words(setup, AT_GRID, race->grid, RACE_PLAYERS);  /* over the last entry's last 2 bytes, as in memory */
    put_words(setup, AT_LETTERS, race->driver_letters, RACE_PLAYERS);
    put_words(setup, AT_MODELS, race->models, RACE_CARS);
    put32(setup, AT_CATCH_UP, race->catch_up), put32(setup, AT_LAPS, race->laps_setting);
    put32(setup, AT_FOLLOWED, race->player), put32(setup, AT_4FBAD0, race->unknown_4fbad0);
    put32(setup, AT_RECORD_SLOT, race->record_slot), put32(setup, AT_MIRRORED, race->mirrored);
    put32(setup, AT_ARCADE, race->arcade);
    /* 004fbae0, the menus' time of day a track: the exe keeps all nine; the port has the raced track's, the rest
     * midday as the CD's replays hold them */
    for (int i = 0; i < TRACKS; i++) put32(setup, AT_TIME_OF_DAY + 4 * i, i == race->track ? race->port_time_of_day : 1);
}

void race_replay_image_put(const RaceState *race, const RaceReplayDirector *dir, uint8_t cuts[REPLAY_CUTS_SIZE],
                           uint8_t seek[REPLAY_SEEK_SIZE], uint8_t records[REPLAY_RECORDS * REPLAY_RECORD_SIZE])
{
    memset(seek, 0, REPLAY_SEEK_SIZE);
    put32(seek, AT_END_CLOCK, dir->end_clock), put32(seek, AT_COUNTDOWN, race->replay_mark_countdown);
    memcpy(seek + AT_MARKS, race->replay_marks, sizeof race->replay_marks);
    put32(seek, AT_SAVED_TIMER, dir->saved_timer), put32(seek, AT_SAVED_INPUT, dir->saved_input_clock);
    put32(seek, AT_SAVED_DUE, dir->saved_due);

    memset(cuts, 0, REPLAY_CUTS_SIZE);
    for (int i = 0; i < REPLAY_CUTS; i++) {
        const ReplayCut *c = &race->replay_cuts[i];
        put32(cuts, 12 * i, c->clock), put32(cuts, 12 * i + 4, c->until), put32(cuts, 12 * i + 8, c->word);
    }
    put32(cuts, AT_CUT_COUNT, race->replay_cut_count), put32(cuts, AT_512F78, race->unknown_512f78);
    put32(cuts, AT_512F7C, race->unknown_512f7c), put32(cuts, AT_SAVED_KIND, dir->saved_kind);
    put_words(cuts, AT_MARK_TICKS, race->replay_mark_ticks, 4);
    put_words(cuts, AT_NOTES, race->notes, RACE_NOTES);
    put32(cuts, AT_NOTE_SECOND, race->note_second), put32(cuts, AT_FRAMES, dir->frames_played);
    put32(cuts, AT_51371C, race->unknown_51371c), put32(cuts, AT_UNTIL, dir->until), put32(cuts, AT_TARGET, dir->target);
    put32(cuts, AT_513728, race->unknown_513728), put32(cuts, AT_51372C, race->unknown_51372c);
    put32(cuts, AT_513730, dir->unknown_513730), put32(cuts, AT_513734, race->unknown_513734);

    memset(records, 0, RECORDS_BYTES);
    memcpy(records, (const uint8_t *)race->players + RECORDS_SKIP, sizeof race->players - RECORDS_SKIP);
    records[AT_CBC] = (uint8_t)race->results_unknown_cbc, records[AT_CBC + 1] = (uint8_t)(race->results_unknown_cbc >> 8);
}

void race_replay_image_get(RaceState *race, RaceReplayDirector *dir, const uint8_t cuts[REPLAY_CUTS_SIZE],
                           const uint8_t seek[REPLAY_SEEK_SIZE],
                           const uint8_t records[REPLAY_RECORDS * REPLAY_RECORD_SIZE])
{
    dir->end_clock = get32(seek, AT_END_CLOCK), race->replay_mark_countdown = get32(seek, AT_COUNTDOWN);
    memcpy(race->replay_marks, seek + AT_MARKS, sizeof race->replay_marks);
    dir->saved_timer = get32(seek, AT_SAVED_TIMER), dir->saved_input_clock = get32(seek, AT_SAVED_INPUT);
    dir->saved_due = get32(seek, AT_SAVED_DUE);

    for (int i = 0; i < REPLAY_CUTS; i++)
        race->replay_cuts[i] = (ReplayCut){ get32(cuts, 12 * i), get32(cuts, 12 * i + 4), get32(cuts, 12 * i + 8) };
    race->replay_cut_count = get32(cuts, AT_CUT_COUNT), race->unknown_512f78 = get32(cuts, AT_512F78);
    race->unknown_512f7c = get32(cuts, AT_512F7C), dir->saved_kind = get32(cuts, AT_SAVED_KIND);
    get_words(cuts, AT_MARK_TICKS, race->replay_mark_ticks, 4);
    get_words(cuts, AT_NOTES, race->notes, RACE_NOTES);
    race->note_second = get32(cuts, AT_NOTE_SECOND), dir->frames_played = get32(cuts, AT_FRAMES);
    race->unknown_51371c = get32(cuts, AT_51371C), dir->until = get32(cuts, AT_UNTIL), dir->target = get32(cuts, AT_TARGET);
    race->unknown_513728 = get32(cuts, AT_513728), race->unknown_51372c = get32(cuts, AT_51372C);
    dir->unknown_513730 = get32(cuts, AT_513730), race->unknown_513734 = get32(cuts, AT_513734);

    memcpy((uint8_t *)race->players + RECORDS_SKIP, records, sizeof race->players - RECORDS_SKIP);
    race->results_unknown_cbc = (int16_t)(records[AT_CBC] | records[AT_CBC + 1] << 8);
}

static const char LOOKS_MAGIC[8] = { 'T', 'N', 'F', 'S', 'L', 'O', 'K', '1' };
enum { LOOK_ENTRY = REPLAY_LOOK_SKIN + REPLAY_LOOK_PAINT };

void race_replay_looks_put(const ReplayLook looks[RACE_PLAYERS], uint8_t out[REPLAY_LOOKS_BLOCK])
{
    memset(out, 0, REPLAY_LOOKS_BLOCK);
    memcpy(out, LOOKS_MAGIC, sizeof LOOKS_MAGIC);
    for (int i = 0; i < RACE_PLAYERS; i++) {
        uint8_t *const at = out + sizeof LOOKS_MAGIC + LOOK_ENTRY * i;
        memcpy(at, looks[i].skin, strnlen(looks[i].skin, REPLAY_LOOK_SKIN - 1));
        memcpy(at + REPLAY_LOOK_SKIN, looks[i].paint, strnlen(looks[i].paint, REPLAY_LOOK_PAINT - 1));
    }
}

/* the name of `n` bytes at `at` into `out`: 0 when it has no end or a byte before it that is not printable ASCII */
static int look_name(const uint8_t *at, size_t n, char *out)
{
    const uint8_t *const end = memchr(at, 0, n);
    if (end == NULL) return 0;
    for (const uint8_t *c = at; c < end; c++)
        if (*c < 0x20 || *c > 0x7e) return 0;
    memcpy(out, at, (size_t)(end - at) + 1);
    return 1;
}

int race_replay_looks_get(const uint8_t block[REPLAY_LOOKS_BLOCK], ReplayLook looks[RACE_PLAYERS])
{
    ReplayLook got[RACE_PLAYERS];
    memset(looks, 0, sizeof(ReplayLook) * RACE_PLAYERS);
    if (memcmp(block, LOOKS_MAGIC, sizeof LOOKS_MAGIC) != 0) return 0;
    for (int i = 0; i < RACE_PLAYERS; i++) {   /* the whole block first: a bad one says nothing */
        const uint8_t *const at = block + sizeof LOOKS_MAGIC + LOOK_ENTRY * i;
        if (!look_name(at, REPLAY_LOOK_SKIN, got[i].skin) || !look_name(at + REPLAY_LOOK_SKIN, REPLAY_LOOK_PAINT, got[i].paint)) return 0;
    }
    memcpy(looks, got, sizeof got);
    return 1;
}

static const char TRACK_MAGIC[REPLAY_TRACK_MAGIC] = { 'T', 'N', 'F', 'S', 'T', 'R', 'K', '1' };

void race_replay_track_write(uint8_t block[REPLAY_TRACK_BLOCK], const TrackPiece *piece)
{
    memset(block, 0, REPLAY_TRACK_BLOCK);
    memcpy(block, TRACK_MAGIC, REPLAY_TRACK_MAGIC);
    if (piece == NULL) return;
    memcpy(block + REPLAY_TRACK_MAGIC, piece->id, strnlen(piece->id, REPLAY_TRACK_ID - 1));
    put32(block, REPLAY_TRACK_MAGIC + REPLAY_TRACK_ID, (int32_t)piece->fingerprint);
}

ReplayCars race_replay_track_read(const uint8_t block[REPLAY_TRACK_BLOCK], int *track, char *id_out)
{
    char id[REPLAY_TRACK_ID];
    id_out[0] = '\0', *track = 0;
    if (memcmp(block, TRACK_MAGIC, REPLAY_TRACK_MAGIC) != 0) return REPLAY_CARS_BAD;
    const uint8_t *const at = block + REPLAY_TRACK_MAGIC;
    if (memchr(at, 0, REPLAY_TRACK_ID) == NULL) return REPLAY_CARS_BAD;
    memcpy(id, at, REPLAY_TRACK_ID);
    if (!mod_ini_id_ok(id)) return REPLAY_CARS_BAD;
    const int found = track_registry_find(id);
    const TrackPiece *const p = found >= 0 ? track_registry_piece(found) : NULL;
    if (p == NULL || p->fingerprint != (uint32_t)get32(block, REPLAY_TRACK_MAGIC + REPLAY_TRACK_ID)) {
        snprintf(id_out, REPLAY_TRACK_ID, "%s", id);
        return p == NULL ? REPLAY_CARS_MISSING : REPLAY_CARS_CHANGED;
    }
    *track = found;
    return REPLAY_CARS_OK;
}

int race_replay_file_track_changed(const uint8_t *file, size_t size)
{
    const size_t looks_end = (size_t)REPLAY_CARS_AT + REPLAY_CARS_BLOCK + REPLAY_LOOKS_BLOCK;
    int track;
    char id[REPLAY_TRACK_ID];
    return file != NULL && size == looks_end + REPLAY_TRACK_BLOCK && race_replay_track_read(file + looks_end, &track, id) == REPLAY_CARS_CHANGED;
}

/* 1 when the setup's stem `stem` (upper case) is the piece's and a stage's digit */
static int piece_stem(const char *stem, const TrackPiece *p)
{
    const size_t n = strlen(p->stem);
    return n > 0 && strncasecmp(stem, p->stem, n) == 0 && stem[n] >= '1' && stem[n] <= '9' && stem[n + 1] == '\0';
}

ReplayCars race_replay_file_cars(const uint8_t *file, size_t size, ReplayRace *out, char *id_out)
{
    const size_t cars_end = (size_t)REPLAY_CARS_AT + REPLAY_CARS_BLOCK, looks_end = cars_end + REPLAY_LOOKS_BLOCK;
    const int track = size == looks_end + REPLAY_TRACK_BLOCK, looks = size == looks_end || track;
    id_out[0] = '\0';
    memset(out, 0, sizeof *out);
    if (file == NULL || (size != cars_end && !looks)) return REPLAY_CARS_BAD;
    int added = 0;
    if (track) {   /* T5: the track first, so the list names it when it is the one missing */
        const ReplayCars t = race_replay_track_read(file + looks_end, &added, id_out);
        if (t != REPLAY_CARS_OK) return t;
    }
    const ReplayCars r = race_replay_image_race_cars(file, file + REPLAY_CARS_AT, out, id_out);
    if (r != REPLAY_CARS_OK) return r;
    if (track && !piece_stem(out->stem, track_registry_piece(added))) return memset(out, 0, sizeof *out), REPLAY_CARS_BAD;
    out->looks_known = looks && race_replay_looks_get(file + cars_end, out->looks);
    out->added_track = added;
    return r;
}
