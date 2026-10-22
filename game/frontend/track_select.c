#include "game/frontend/track_select.h"
#include "game/race/track_fam.h"

#include <stdio.h>
#include <string.h>

enum {
    TRACKS = 9, ITEMS = 10, DONE = 0, PREV = 1, NEXT = 2, BEST = 3, SEGMENT_1 = 4, LAPS = 7, NOTHING = 8,
    TIME_OF_DAY = 9, LOST_VEGAS = 8, TOURNAMENT = 3, OPPONENT_TOURNAMENT = 10, SOUND_MOVE = 0x47,
    SOUND_SELECT = 0x48, WAIT = 0x2ee0, KEY_SPACE = 0x20, KEY_ENTER = 0xd, KEY_ESCAPE = 0x1b, KEY_UP = 0x4800,
    KEY_LEFT = 0x4b00, KEY_RIGHT = 0x4d00, KEY_DOWN = 0x5000, PATH = 0x100, TEXT = 0x28, VARIANT_X = 0xc8,
    VARIANT_Y = 0x13c
};

/* the tracks' files (0046751c) and music (00467540) */
static const char *const TRACK_FILE[TRACKS] = { "citysel", "coastsel", "alpsel", "rustsel", "autsel",
                                               "brunsel", "vertsel", "tropsel", "lostsel" };
static const char *const MUSIC[TRACKS] = { "city.eas", "coastal.eas", "alpine.eas", "loop1.eas", "loop2.eas",
                                          "goldrash.eas", "loop3.eas", "industri.eas", "loopx.eas" };
/* the first three tracks' lengths, whole then by segment (00467564, 004675d0), the others' (004675b8, 00467600) */
static const char *const MILES_SEGMENTED[12] = { "16.0 miles", "5.2 miles", "4.7 miles", "6.1 miles",
                                                 "15.2 miles", "5.2 miles", "5.0 miles", "5.0 miles",
                                                 "18.1 miles", "5.7 miles", "6.4 miles", "6.0 miles" };
static const char *const KM_SEGMENTED[12] = { "25.5 km", "8.3 km", "7.5 km", "9.7 km", "24.1 km", "8.3 km",
                                              "7.9 km", "7.9 km", "28.8 km", "9.1 km", "10.2 km", "9.5 km" };
static const char *const MILES[6] = { "1.5 miles", "3.0 miles", "3.0 miles", "3.0 miles", "3.0 miles",
                                      "3.0 miles" };
static const char *const KM[6] = { "2.4 km", "4.9 km", "4.9 km", "4.9 km", "4.9 km", "4.9 km" };
/* the time of day lit (0046743c) and not (00467448); the one past midday per track (00467594) */
static const char *const TIME_LIT[3] = { "morl", "midl", "evel" };
static const char *const TIME_DIM[3] = { "mord", "midd", "eved" };
static const int32_t TIME_OTHER[TRACKS] = { 0, 2, 2, 2, 0, 2, 2, 2, 2 };
/* the items lit and not */
static const char *const LIT[8] = { "donl", "prel", "nexl", "besl", "ssg1", "ssg2", "ssg3", "lapl" };
static const char *const DIM[4] = { "dond", "pred", "nexd", "besd" };
/* where the pointer goes per item (004674f4, 00467508) */
static const int16_t POINTER_X[ITEMS] = { 38, 231, 390, 568, 256, 256, 256, 80, 0, 80 };
static const int16_t POINTER_Y[ITEMS] = { 455, 455, 455, 455, 275, 205, 119, 380, 0, 404 };
/* the arrows' moves per item, past the first three tracks and on them */
static const int16_t UP[2][ITEMS] = { { 9, 9, 9, 9, 9, 9, 9, 0, 0, 7 }, { 9, 9, 9, 9, 5, 6, 0, 0, 0, 4 } };
static const int16_t DOWN[2][ITEMS] = { { 7, 7, 7, 7, 9, 9, 9, 9, 0, 0 }, { 6, 6, 6, 6, 9, 4, 5, 0, 0, 0 } };
static const int16_t LEFT[2][ITEMS] = { { 3, 0, 1, 2, 9, 9, 9, 3, 0, 3 }, { 3, 0, 1, 2, 0, 0, 0, 0, 0, 3 } };
static const int16_t RIGHT[2][ITEMS] = { { 1, 2, 3, 0, 9, 9, 9, 2, 0, 2 }, { 1, 2, 3, 0, 2, 2, 2, 0, 0, 2 } };
/* the segments' boxes per track: x (00467684, 00467696), y from and to (00467618, 0046764e) */
static const int16_t SEGMENT_X0[TRACKS] = { 228, 213, 212, 213, 213, 213, 213, 213, 213 };
static const int16_t SEGMENT_X1[TRACKS] = { 301, 301, 300, 301, 301, 301, 301, 301, 301 };
static const int16_t SEGMENT_Y0[3 * TRACKS] = { 209, 142, 67, 245, 164, 78, 211, 121, 32, 500, 500, 500, 500, 500,
                                                500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500 };
static const int16_t SEGMENT_Y1[3 * TRACKS] = { 288, 208, 141, 323, 244, 163, 313, 210, 120, 520, 520, 520, 520,
                                                520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520, 520,
                                                520 };
/* 004baef8: a tournament's car per track */
static const int16_t TOURNAMENT_CAR[TRACKS] = { 0, 3, 6, 0, 3, 3, 6, 6, 7 };

static int16_t g16(const uint8_t *config, int at)
{
    return (int16_t)(uint16_t)(config[at] | config[at + 1] << 8);
}

static void p16(uint8_t *config, int at, int v)
{
    config[at] = (uint8_t)v, config[at + 1] = (uint8_t)((unsigned)v >> 8);
}

static int track_of(const TrackSelectState *st)
{
    return g16(st->config, TRACK_SELECT_TRACK);
}

/* the laps for the choice: tracks 4..7 are the short ones */
static void set_laps(TrackSelectState *st, int choice)
{
    const int track = track_of(st), short_track = track >= 4 && track <= 7;
    p16(st->config, TRACK_SELECT_LAPS, choice == 0 ? (short_track ? 2 : 4)
                                       : choice == 1 ? (short_track ? 6 : 8) : (short_track ? 0xc : 0x10));
}

static int32_t load_track(const TrackSelectInputs *in, TrackSelectState *st, uint8_t *pal, int feed,
                          const TrackSelectOps *ops)
{
    char path[PATH];
    const char *file = TRACK_FILE[track_of(st)];
    snprintf(path, sizeof path, "%stracksel/%s.qfs", in->paths, file);
    const int32_t bank = ops->load_bank(ops->ctx, path, 0);
    snprintf(path, sizeof path, "tracksel/%s.inv", file);
    st->inverse = ops->load_inverse(ops->ctx, path);
    if (feed) ops->music_feed(ops->ctx);
    const uint8_t *pal_shape = ops->shape(ops->ctx, bank, "!pal");
    palette_put(pal, 0, 0x100, pal_shape + 0x10, pal_shape[0], st->screen_palette);
    ops->flush(ops->ctx);
    ops->palette_set(ops->ctx, 0, 0x100, pal);
    ops->inverse_build(ops->ctx);
    ops->text_styles(ops->ctx);
    return bank;
}

/* port: the variants on offer, and the one the flags name (0 normal, 1 mirror, 2 rally) */
static int offered(const TrackSelectState *st, int track)
{
    return st->plus && track != LOST_VEGAS && g16(st->config, TRACK_SELECT_RACE_TYPE) != TOURNAMENT;
}

static int variant_of(const TrackSelectState *st)
{
    return !st->plus ? 0 : *st->plus->variant_r ? 2 : *st->plus->variant_m ? 1 : 0;
}

static int no_time_of_day(const TrackSelectInputs *in, const TrackSelectState *st, int track)
{
    if (st->plus)
        return !track_fam_time_offered(track, *st->plus->variant_m, *st->plus->variant_r, st->plus->variant_time);
    return track == LOST_VEGAS || in->variant_m || in->variant_r;
}

static int has_variant(const TrackSelectPlus *plus, int track, int variant)
{
    return variant == 0 || (track != LOST_VEGAS && (variant == 1 ? plus->unlock_m : plus->unlock_r));
}

void track_select_entry(int track, int variant, int direction, int lost_vegas, int unlock_m, int unlock_r,
                        int *to_track, int *to_variant)
{
    const TrackSelectPlus plus = { unlock_m, unlock_r, NULL, NULL, 0 };
    const int last = lost_vegas ? LOST_VEGAS : LOST_VEGAS - 1;
    for (int v = variant + direction; v >= 0 && v <= 2; v += direction)
        if (has_variant(&plus, track, v)) {
            *to_track = track, *to_variant = v;
            return;
        }
    const int to = direction > 0 ? (track + 1) % (last + 1) : track == 0 ? last : track - 1;
    *to_track = to;
    *to_variant = 0;
    if (direction < 0)
        for (int v = 2; v > 0; v--)
            if (has_variant(&plus, to, v)) {
                *to_variant = v;
                break;
            }
}

static void masked(int32_t bank, const char *tag, const TrackSelectOps *ops)
{
    ops->blit_masked(ops->ctx, ops->shape(ops->ctx, bank, tag));
}

static void plain(int32_t bank, const char *tag, const TrackSelectOps *ops)
{
    ops->blit(ops->ctx, ops->shape(ops->ctx, bank, tag));
}

/* the length, the laps and the segment's art */
static void draw_texts(const TrackSelectInputs *in, const TrackSelectState *st, int32_t bank, int item,
                       int segment, const TrackSelectOps *ops)
{
    const int track = track_of(st);
    char text[TEXT];
    ops->text_select(ops->ctx, 0);
    if (track > 2) {
        ops->text_shadowed(ops->ctx, (in->kmh ? KM : MILES)[track - 3], 0xc8, 0x153);
        if (item == LAPS) ops->text_select(ops->ctx, 1);
        snprintf(text, sizeof text, "%d", g16(st->config, TRACK_SELECT_LAPS));
        ops->text_shadowed(ops->ctx, text, 0xdc, 0x16b);
    } else {
        const char *const *lengths = in->kmh ? KM_SEGMENTED : MILES_SEGMENTED;
        ops->text_shadowed(ops->ctx, lengths[4 * track + segment], 0xc8, 0x153);
        ops->text_shadowed(ops->ctx, lengths[4 * track], 0xaf, 0x16a);
    }
    if (st->plus && variant_of(st) != 0)   /* port: the variant's name, with the game's text routine */
        ops->text_shadowed(ops->ctx, variant_of(st) == 1 ? "Mirror" : "Rally", VARIANT_X, VARIANT_Y);
    if (track < 3) {
        snprintf(text, sizeof text, "bsg%d", segment);
        plain(bank, text, ops);
        snprintf(text, sizeof text, "ssg%d", segment);
        plain(bank, text, ops);
    }
}

static void draw_screen(const TrackSelectInputs *in, const TrackSelectState *st, int32_t bank, int item,
                        int segment, const TrackSelectOps *ops)
{
    const int track = track_of(st);
    ops->select(ops->ctx, in->screen);
    plain(bank, "bgnd", ops);
    if (!no_time_of_day(in, st, track)) masked(bank, TIME_DIM[st->time_of_day[track]], ops);
    draw_texts(in, st, bank, item, segment, ops);
    if (item < 8) {
        masked(bank, LIT[item], ops);
    } else if (item == TIME_OF_DAY && track != LOST_VEGAS) {
        masked(bank, "timl", ops);
        masked(bank, TIME_LIT[st->time_of_day[track]], ops);
    }
}

/* a change of item: the old one dimmed, the new one lit (no flip until the redraw) */
static void draw_change(const TrackSelectInputs *in, const TrackSelectState *st, int32_t bank, int was, int item,
                        const TrackSelectOps *ops)
{
    const int track = track_of(st);
    char text[TEXT];
    ops->select(ops->ctx, in->screen);
    ops->sound(ops->ctx, SOUND_MOVE);
    ops->pointer_hide(ops->ctx);
    ops->lock(ops->ctx);
    ops->text_select(ops->ctx, 0);
    if (was < BEST) {
        plain(bank, DIM[was], ops);
    } else if (was == BEST) {
        masked(bank, DIM[was], ops);
    } else if (was == LAPS) {
        masked(bank, "lapd", ops);
        snprintf(text, sizeof text, "%d", g16(st->config, TRACK_SELECT_LAPS));  /* and never drawn */
    } else if (was == TIME_OF_DAY && track != LOST_VEGAS) {
        masked(bank, "timd", ops);
        masked(bank, TIME_DIM[st->time_of_day[track]], ops);
    }
    ops->text_select(ops->ctx, 1);
    if (item < BEST) {
        plain(bank, LIT[item], ops);
    } else if (item < 8) {
        masked(bank, LIT[item], ops);
        if (item == LAPS) snprintf(text, sizeof text, "%d", g16(st->config, TRACK_SELECT_LAPS));
    } else if (item == TIME_OF_DAY && track != LOST_VEGAS) {
        masked(bank, "timl", ops);
        masked(bank, TIME_LIT[st->time_of_day[track]], ops);
    }
}

/* the item under the pointer */
static int pointed(const TrackSelectInputs *in, const TrackSelectState *st, const TrackSelectOps *ops)
{
    const int track = track_of(st);
    if (ops->pointer_in_rect(ops->ctx, 0, 0x82, 0x1b3, 0x1e0)) return DONE;
    if (ops->pointer_in_rect(ops->ctx, 0xc8, 0xfa, 0x1b3, 0x1e0)) return PREV;
    if (ops->pointer_in_rect(ops->ctx, 0x172, 0x1a4, 0x1b3, 0x1e0)) return NEXT;
    if (ops->pointer_in_rect(ops->ctx, 0x1e0, 0x280, 0x1b3, 0x1df)) return BEST;
    for (int k = 0; k < 3; k++) {
        if (ops->pointer_in_rect(ops->ctx, SEGMENT_X0[track], SEGMENT_X1[track], SEGMENT_Y0[3 * track + k],
                                 SEGMENT_Y1[3 * track + k]))
            return SEGMENT_1 + k;
    }
    if (ops->pointer_in_rect(ops->ctx, 0, 0x140, 0x15e, 0x183) && track > 2) return LAPS;
    if (ops->pointer_in_rect(ops->ctx, 0, 0x140, 0x184, 0x1a0) && !no_time_of_day(in, st, track)) return TIME_OF_DAY;
    return NOTHING;
}

/* a new track: a tournament's car, the laps for the choice, the screen loaded again */
static int32_t change_track(const TrackSelectInputs *in, TrackSelectState *st, int32_t bank, int track,
                            uint8_t *pal, const TrackSelectOps *ops)
{
    p16(st->config, TRACK_SELECT_TRACK, track);
    if (g16(st->config, TRACK_SELECT_RACE_TYPE) == TOURNAMENT) {
        p16(st->config, TRACK_SELECT_OPPONENT, OPPONENT_TOURNAMENT);
        p16(st->config, TRACK_SELECT_CAR, TOURNAMENT_CAR[track_of(st)]);
        ops->car_flags(ops->ctx);
    }
    set_laps(st, g16(st->config, TRACK_SELECT_LAPS_CHOICE));
    ops->music_stop(ops->ctx);
    ops->music_free(ops->ctx);
    ops->fade_out(ops->ctx, 0, 0x100);
    ops->pointer_hide(ops->ctx);
    ops->music_feed(ops->ctx);
    ops->free(ops->ctx, bank);
    ops->free_inverse(ops->ctx);
    return load_track(in, st, pal, 1, ops);
}

/* the item lit acted on: 0 when the screen goes on, else 1 (Done, or Best's screen said so) */
static int act(const TrackSelectInputs *in, TrackSelectState *st, DialogPointer *p, int32_t *bank, int item,
               int *segment, int16_t *best, uint8_t *pal, int *redraw, int *changed, const TrackSelectOps *ops)
{
    const int track = track_of(st);
    if (item == DONE) return 1;
    if ((item == PREV || item == NEXT) && offered(st, track)) {   /* port: the track's variants are entries too */
        int to, variant;
        track_select_entry(track, variant_of(st), item == PREV ? -1 : 1, in->lost_vegas, st->plus->unlock_m,
                           st->plus->unlock_r, &to, &variant);
        *st->plus->variant_m = variant == 1, *st->plus->variant_r = variant == 2;
        if (to == track) {   /* the same track: its art stays, the screen is drawn again */
            *redraw = 1;
            return 0;
        }
        *bank = change_track(in, st, *bank, to, pal, ops);
        *redraw = 1;
        return -1;
    }
    if (item == PREV || item == NEXT) {
        int to;
        if (item == PREV) {
            to = (int16_t)(track - 1);
            if (to < 0) to = in->lost_vegas ? LOST_VEGAS : 7;
        } else {
            to = (int16_t)((track + 1) % (in->lost_vegas ? 9 : 8));
        }
        *bank = change_track(in, st, *bank, to, pal, ops);
        *redraw = 1;
        return -1;
    }
    if (item == BEST) {
        ops->music_stop(ops->ctx);
        ops->music_free(ops->ctx);
        ops->free_inverse(ops->ctx);
        ops->free(ops->ctx, *bank);
        ops->fade_out(ops->ctx, 0, 0x100);
        ops->music_feed(ops->ctx);
        *best = (int16_t)ops->best_times(ops->ctx);
        if (*best) return 1;
        *bank = load_track(in, st, pal, 1, ops);
        if (!p->active) ops->pointer_move_to(ops->ctx, POINTER_X[item], POINTER_Y[item]);
        *redraw = 1;
        return -1;
    }
    if (item >= SEGMENT_1 && item < LAPS) {
        *segment = item - SEGMENT_1 + 1;
        *changed = 1;
    } else if (item == LAPS) {
        if (g16(st->config, TRACK_SELECT_RACE_TYPE) != TOURNAMENT) {
            const int choice = (g16(st->config, TRACK_SELECT_LAPS_CHOICE) + 1) % 3;
            p16(st->config, TRACK_SELECT_LAPS_CHOICE, choice);
            set_laps(st, (int16_t)choice);
        }
        *changed = 1;
    } else if (item == TIME_OF_DAY && track != LOST_VEGAS) {
        int32_t *t = &st->time_of_day[track];
        *t = *t == 1 ? TIME_OTHER[track] : 1;
        *changed = 1;
    }
    return 0;
}

/* an arrow: the item it leads to (9, the time of day, skipped where there is none) */
static int arrow(const TrackSelectInputs *in, const TrackSelectState *st, int key, int item)
{
    const int track = track_of(st), low = track <= 2;
    int to;
    if (key == KEY_LEFT) return LEFT[low][item];
    if (key == KEY_RIGHT) return RIGHT[low][item];
    if (key == KEY_UP) {
        to = UP[low][item];
        if (to == TIME_OF_DAY && no_time_of_day(in, st, track)) to = low ? SEGMENT_1 : LAPS;
    } else {
        to = DOWN[low][item];
        if (to == TIME_OF_DAY && no_time_of_day(in, st, track)) to = DONE;
    }
    return to;
}

int track_select_screen(const TrackSelectInputs *in, TrackSelectState *st, DialogPointer *p,
                        const TrackSelectOps *ops)
{
    uint8_t saved[TRACK_SELECT_CONFIG], pal[PALETTE_PUT_BYTES], black[PALETTE_PUT_BYTES];
    memcpy(saved, st->config, sizeof saved);
    const int32_t saved_m = st->plus ? *st->plus->variant_m : 0, saved_r = st->plus ? *st->plus->variant_r : 0;
    int quit = 0, refresh = 1, redraw = 1, changed = 0, was = 0, item = DONE, keys_armed = 1, buttons_armed = 1;
    int segment = 1;
    int16_t best = 0;
    uint32_t waited = 0;  /* the ticks summed, compared as a short */
    if (!p->active) ops->pointer_move_to(ops->ctx, POINTER_X[item], POINTER_Y[item]);
    int32_t bank = load_track(in, st, pal, 0, ops);
    if (p->active) ops->pointer_poll(ops->ctx);
    ops->pointer_mark(ops->ctx, p);
    while (!quit) {
        const int now = pointed(in, st, ops);
        if (now != item) was = item, item = now, changed = 1;
        if (redraw) {
            draw_screen(in, st, bank, item, segment, ops);
            if (refresh) {
                palette_put_clear(black);
                ops->palette_set(ops->ctx, 0, 0x100, black);
            }
            ops->select_default(ops->ctx);
            ops->pointer_hide(ops->ctx);
            ops->lock(ops->ctx);
            ops->blit_word(ops->ctx, in->screen_shape);
            ops->unlock(ops->ctx);
            ops->flip(ops->ctx);
            ops->select(ops->ctx, in->screen);
            ops->pointer_show(ops->ctx);
            if (refresh) {
                ops->pointer_mark(ops->ctx, p);
                if (p->active) ops->pointer_buttons(ops->ctx);
                ops->music_start(ops->ctx, MUSIC[track_of(st)]);
                ops->fade_in(ops->ctx, 0, 0x100, pal);
                ops->text_styles(ops->ctx);
                ops->music_play(ops->ctx);
                refresh = 0;
            }
            ops->delay(ops->ctx, WAIT);
            redraw = 0, changed = 0;
        }
        if (changed) {
            redraw = 1;
            draw_change(in, st, bank, was, item, ops);
            changed = 0;
        }
        if (!keys_armed) {
            waited += (uint32_t)ops->ticks(ops->ctx);
            if ((int16_t)(uint16_t)waited > 0x19) keys_armed = 1;
        }
        if (p->active) p->buttons = (int16_t)ops->pointer_buttons(ops->ctx);
        if (!p->buttons) buttons_armed = 1;
        ops->pointer_activate_if_moved(ops->ctx);
        const uint16_t key = (uint16_t)ops->key(ops->ctx);
        if (key == KEY_SPACE || key == KEY_ENTER || (p->buttons == 1 && buttons_armed)) {
            const int armed = keys_armed;
            ops->pointer_click(ops->ctx);
            if (armed) {
                ops->sound(ops->ctx, SOUND_SELECT);
                const int r = act(in, st, p, &bank, item, &segment, &best, pal, &redraw, &changed, ops);
                if (r > 0) quit = 1;
                if (r < 0) refresh = 1;
            }
            buttons_armed = 0, keys_armed = 0, waited = 0;
            ops->ticks(ops->ctx);
            ops->delay(ops->ctx, WAIT);
        } else if (key != 0 && keys_armed) {
            if (key == KEY_ESCAPE) {
                memcpy(st->config, saved, sizeof saved);
                if (st->plus) *st->plus->variant_m = saved_m, *st->plus->variant_r = saved_r;   /* port */
                ops->sound(ops->ctx, SOUND_SELECT);
                quit = 1;
            } else if (key == KEY_LEFT || key == KEY_RIGHT || key == KEY_UP || key == KEY_DOWN) {
                ops->pointer_off(ops->ctx);
                was = item;
                item = arrow(in, st, key, item);
                changed = 1;
                ops->pointer_move_to(ops->ctx, POINTER_X[item], POINTER_Y[item]);
            }
            keys_armed = 0, waited = 0;
            ops->ticks(ops->ctx);
        } else if (key == 0) {
            keys_armed = 1;
        }
        ops->music_service(ops->ctx);
        if (!refresh && (ops->deadline_passed(ops->ctx) || (p->buttons == 2 && buttons_armed))) {
            ops->sound(ops->ctx, SOUND_SELECT);
            quit = 1;
        }
    }
    if (!best) {
        ops->music_stop(ops->ctx);
        ops->music_free(ops->ctx);
        ops->music_fade_out(ops->ctx);
        ops->fade_out(ops->ctx, 0, 0x100);
        ops->music_end(ops->ctx);
        ops->free_inverse(ops->ctx);
        ops->free(ops->ctx, bank);
    }
    ops->pointer_mark(ops->ctx, p);
    return quit;
}
