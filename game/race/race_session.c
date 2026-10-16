/* A race session (race_session.h). */
#include "game/race/race_session.h"

#include <string.h>

enum {
    /* the setup's (004fb7bc) */
    SETUP_TRACK = 0x6,                /* 004fb7c2 */
    SETUP_NAME = 0xa,                 /* 004fb7c6 */
    SETUP_LINKED = 0x18,              /* 004fb7d4: 1 a network race */
    SETUP_FLAGS = 0x1c,               /* 004fb7d8 */
    SETUP_HUMANS = 0x24,              /* 004fb7e0 */
    SETUP_TIME = 0x38,                /* 004fb7f4 */
    SETUP_ENTRIES = 0x3e, SETUP_ENTRY = 0x4b,
    SETUP_GRID = 0x2b4, SETUP_GRID_WORDS = 9, SETUP_GRID_FILL = 99,  /* 004fba70 */
    SETUP_LEVEL = 0x300,              /* 004fbabc */
    SETUP_W4FBAD8 = 0x31c, SETUP_W4FBADC = 0x320,
    SETUP_PER_TRACK = 0x324,          /* 004fbae0: one dword a track */
    /* an entry's */
    ENTRY_SOUND = 0x1d, ENTRY_MUSIC = 0x21, ENTRY_W25 = 0x25, ENTRY_SOUND_ON = 0x29, ENTRY_CAR = 0x31,
    CAR_BYTES = 12, CARS_MAX = 8,
    RECORD_CAR = 0x33,                /* a record's car bytes (its entry's +0x31) */
    /* the cars block's (004fb724) option bytes */
    OPT_784 = 0x60, OPT_785 = 0x61, OPT_786 = 0x62, OPT_787 = 0x63, OPT_789 = 0x65,
    /* the settings' (00524e5f) */
    SET_W61 = 0x2, SET_MUSIC = 0x4, SET_MUSIC_VOL = 0x6, SET_SOUND_VOL = 0xa, SET_LOW = 0x10,
    SET_OPTS = 0x12,                  /* 00524e71, 73, 75, 77, 79 */
    SET_VARIANT = 0x24,               /* 00524e83 */
    SET_TRACK = 0x26,                 /* 00524e85 */
    SET_REPLAY = 0x40,                /* 00524e9f */
    /* values */
    TRACK_TR1 = 3, LEVEL_FLAG = 8, LEVEL_FORCED = 3, NET_WAIT_SETUP = 2, STATE_NET = 2,
    REPLAY_ENDED_SAVE = 3, END_SAVE = 0x40
};

static int32_t get32(const uint8_t *b) { return (int32_t)((uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24); }
static void put32(uint8_t *b, int32_t v) { for (int k = 0; k < 4; k++) b[k] = (uint8_t)((uint32_t)v >> 8 * k); }
static int16_t get16(const uint8_t *b) { return (int16_t)(uint16_t)(b[0] | b[1] << 8); }
static void put16(uint8_t *b, int16_t v) { b[0] = (uint8_t)v, b[1] = (uint8_t)((uint16_t)v >> 8); }

/* The option bytes 004fb784, 789, 785, 787, 786 and the settings' words 00524e71.. they come from, in that order */
static const int OPTS[RACE_SESSION_SAVED] = { OPT_784, OPT_789, OPT_785, OPT_787, OPT_786 };

static int16_t replay_kind(const RaceSession *s) { return get16(s->settings + SET_REPLAY); }

/* The settings taken in: the words reset, high, the option bytes, the rolls */
static void session_start(const RaceSessionOps *ops, RaceSession *s)
{
    s->unknown_4b95c0 = 0;
    s->high = 0;
    s->unknown_4fa0cc = s->unknown_4fa0c4 = s->unknown_4fa0c8 = 0;
    if (get16(s->settings + SET_LOW) == 0) s->high = 1, s->unknown_4b95b4 = 0;
    else s->unknown_4b95b4 = 1;
    for (int k = 0; k < RACE_SESSION_SAVED; k++) s->cars[OPTS[k]] = s->settings[SET_OPTS + 2 * k];
    ops->seed(ops->ctx, ops->time(ops->ctx, NULL));
    for (int k = 0; k < RACE_SESSION_ROLLS; k++) {
        const uint32_t a = (uint32_t)ops->roll(ops->ctx);
        s->rolls[k] = (int32_t)(a * (uint32_t)ops->roll(ops->ctx));
    }
    ops->setup_4381ac(ops->ctx);
    if (s->high != 0) ops->window_size(ops->ctx, 0x40, 0x1cc, 0x240, 0x1e0);
    else ops->window_size(ops->ctx, 0x20, 0xb4, 0x120, 0xc8);
    ops->keys_setup(ops->ctx);
}

/* argv[1] to the setup's name, within the setup (port guard) */
static void session_name(RaceSession *s, const char *name)
{
    for (int k = 0; SETUP_NAME + k < RACE_SESSION_SETUP; k++)
        if ((s->setup[SETUP_NAME + k] = (uint8_t)name[k]) == 0) break;
}

/* The network's handshake (a network race, not a replay) */
static void session_link(const RaceSessionOps *ops, RaceSession *s)
{
    const int32_t linked = get32(s->setup + SETUP_LINKED);
    s->once = 0;
    if (linked != 1 || replay_kind(s) != 0) return;
    ops->schedule_clear(ops->ctx);
    ops->net_deadline(ops->ctx, -1);
    if (ops->net_wait(ops->ctx, NET_WAIT_SETUP) == 0) s->once = linked;
    else s->latency = ops->ping_all(ops->ctx);
}

/* The replay's request and setup, or none */
static void session_replay(const RaceSessionOps *ops, RaceSession *s)
{
    if (replay_kind(s) == 0) {
        ops->replay_request(ops->ctx, 0);
        return;
    }
    ops->replay_name(ops->ctx);
    const int16_t kind = replay_kind(s);
    ops->replay_request(ops->ctx, kind == 1 ? 4 : kind == 2 ? 5 : 8);
    ops->setup_read(ops->ctx);
}

/* The followed player's entry, within the setup (port guard); NULL outside */
static uint8_t *followed_entry(const RaceSession *s)
{
    const int64_t at = SETUP_ENTRIES + (int64_t)s->followed * SETUP_ENTRY;
    if (at < 0 || at + ENTRY_CAR + CAR_BYTES > RACE_SESSION_SETUP) return NULL;
    return s->setup + at;
}

/* The followed entry's sound words, 004fa0ec, the entry's +0x25 */
static void session_entry(RaceSession *s)
{
    uint8_t *e = followed_entry(s);
    if (e != NULL) {
        int32_t sound_on = 0;
        if ((int16_t)((uint32_t)s->sound >> 16) == 0) {  /* 0052b30e: the sound's open word */
            put32(e + ENTRY_SOUND, 0), put32(e + ENTRY_MUSIC, 0);
        } else {
            put32(e + ENTRY_SOUND, get16(s->settings + SET_SOUND_VOL));
            put32(e + ENTRY_MUSIC, get16(s->settings + SET_MUSIC_VOL));
            sound_on = (int16_t)((uint32_t)s->sound >> 16);
        }
        put32(e + ENTRY_SOUND_ON, sound_on);
    }
    if (get32(s->setup + SETUP_W4FBADC) != 0 || get32(s->setup + SETUP_W4FBAD8) != 0) s->unknown_4fa0ec = 1;
    else {
        const int32_t track = get32(s->setup + SETUP_TRACK);
        const int64_t at = SETUP_PER_TRACK + 4 * (int64_t)track;
        s->unknown_4fa0ec = at >= 0 && at + 4 <= RACE_SESSION_SETUP ? get32(s->setup + at)
                            : at == RACE_SESSION_SETUP ? s->viewed : 0;  /* port guard: 004fbb04 is the viewed word */
    }
    e = followed_entry(s);
    if (e != NULL) put32(e + ENTRY_W25, get16(s->settings + SET_W61));
}

/* Each human's 12 bytes (entry +0x31) to 004fb724 + 12 i: at least once with any human, 8 at most (port guard) */
static void session_cars(RaceSession *s)
{
    const int32_t humans = get32(s->setup + SETUP_HUMANS);
    for (int32_t i = 0; i < humans && i < CARS_MAX; i++)
        memcpy(s->cars + CAR_BYTES * i, s->setup + SETUP_ENTRIES + SETUP_ENTRY * i + ENTRY_CAR, CAR_BYTES);
}

/* After the race: the options back to 0052b264.. and the settings, the state */
static void session_back(const RaceSessionOps *ops, RaceSession *s)
{
    if (s->high == 0 && s->cars[OPT_789] > 1) s->cars[OPT_789] = 1;
    int32_t a = 0, b = 0;
    ops->window(ops->ctx, &a, &b);
    s->saved[0] = (int16_t)a;
    for (int k = 1; k < RACE_SESSION_SAVED; k++) s->saved[k] = s->cars[OPTS[k]];
    put16(s->settings + SET_OPTS, (int16_t)a);
    for (int k = 1; k < RACE_SESSION_SAVED; k++) put16(s->settings + SET_OPTS + 2 * k, s->saved[k]);
    if (s->play == 0 && get32(s->setup + SETUP_LINKED) == 1 && replay_kind(s) == 0) s->state = STATE_NET;
    s->unknown_52b2fe = (int16_t)s->state;
}

int32_t race_session(const RaceSessionOps *ops, RaceSession *s, int32_t argc, const char *const *argv)
{
    uint8_t first[RACE_SESSION_RECORD], second[RACE_SESSION_RECORD];
    ops->from_menu(ops->ctx, first, second);
    session_start(ops, s);
    if (s->followed >= 0 && s->followed < CARS_MAX)  /* port guard */
        memcpy(first + RECORD_CAR, s->cars + CAR_BYTES * s->followed, CAR_BYTES);
    if (replay_kind(s) == 0 && argc > 1) session_name(s, argv[1]);
    ops->rrsm_alloc(ops->ctx);
    ops->timer_start(ops->ctx);
    session_link(ops, s);
    if (get16(s->settings + SET_VARIANT) == 0)
        for (int k = 0; k < SETUP_GRID_WORDS; k++) put32(s->setup + SETUP_GRID + 4 * k, SETUP_GRID_FILL);
    ops->grid(ops->ctx);
    int32_t now = 0;
    const int32_t t = ops->time(ops->ctx, &now);
    (void)t;
    put32(s->setup + SETUP_TIME, now);
    if (replay_kind(s) == 0) ops->exchange(ops->ctx, first, second);
    ops->end_clear(ops->ctx);
    session_replay(ops, s);
    session_entry(s);
    ops->random_seed(ops->ctx, get32(s->setup + SETUP_TIME));
    ops->models(ops->ctx);
    session_cars(s);
    if (get32(s->setup + SETUP_W4FBADC) != 0 && get16(s->settings + SET_TRACK) == TRACK_TR1 &&
        get32(s->setup + SETUP_TRACK) == TRACK_TR1)
        memcpy(s->setup + SETUP_NAME, "tr5", 4);  /* the dword 004ae1e0 */
    ops->stem_set(ops->ctx, s->setup + SETUP_NAME);
    if ((s->setup[SETUP_FLAGS] & LEVEL_FLAG) != 0) {
        put32(s->setup + SETUP_LEVEL, LEVEL_FORCED);
        ops->timer_level(ops->ctx);
    }
    ops->stand_in_start(ops->ctx);
    ops->service(ops->ctx);
    ops->run(ops->ctx);
    ops->service(ops->ctx);
    ops->timer_stop(ops->ctx);
    session_back(ops, s);
    ops->results_end(ops->ctx);
    if (get16(s->settings + SET_MUSIC) != 0) {
        ops->menu_4644f0(ops->ctx);
        ops->menu_464568(ops->ctx);
    }
    ops->flush(ops->ctx);
    return replay_kind(s) == REPLAY_ENDED_SAVE && s->unknown_509d70 == END_SAVE;
}
