#include "game/race/race_replay_image.h"

#include <ctype.h>
#include <string.h>

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
    if (race->unknown_4c5cd4 != 0 || race->start_input != 0) {   /* port: the EXPLODE and NITRO cheats' words are not in the
                                                                   * exe's setup; a replay plays without them */
        setup[AT_PORT_CHEATS] = (uint8_t)((race->unknown_4c5cd4 != 0 ? CHEATS_EXPLODE : 0) | (race->start_input != 0 ? CHEATS_NITRO : 0));
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
