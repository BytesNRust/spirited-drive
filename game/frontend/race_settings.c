#include "game/frontend/race_settings.h"

#include <stdint.h>
#include <string.h>

/* 004baef8: tournament car class per track (first car of the class). */
static const int TOURNAMENT_CAR[10] = { 0, 3, 6, 0, 3, 3, 6, 6, 7, 3 };
/* 00461f1c / 00461f34: per-car tables (shorts) read by 00465ec8. */
static const int CAR_TABLE_A[12] = { 1, 1, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0 };
static const int CAR_TABLE_B[12] = { 0, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0 };

/* 00465ec8 */
void race_settings_car_flags(RaceSettings *s)
{
    s->car_flag_a = CAR_TABLE_A[s->player_car] ? 0 : 2;
    s->car_flag_b = (CAR_TABLE_B[s->player_car] == 0) + 1;
}

static int short_track(int track)
{
    return track >= 4 && track <= 7;
}

void track_changed(RaceSettings *s)
{
    if (s->race_type == RACE_TOURNAMENT) {
        s->player_car = TOURNAMENT_CAR[s->track];
        race_settings_car_flags(s);
        s->opponent = OPPONENT_TOURNAMENT;
    }
    static const int LAPS[3][2] = { { 4, 2 }, { 8, 6 }, { 16, 12 } };
    const int level = s->difficulty < 0 ? 0 : s->difficulty > 2 ? 2 : s->difficulty;
    s->laps = LAPS[level][short_track(s->track)];
}

/* 00432974 */
void race_settings_type_changed(RaceSettings *s, Unlocks *u, FrontendSession *f)
{
    const int earned = f->unknown_4baf1c || f->unlock_level;
    const int other = f->flag_52b312 && !f->unknown_4faa56;
    switch (s->race_type) {
    case RACE_TIME_TRIAL:
        f->unknown_4c5cc2 = 0, f->unknown_4bafb6 = 0x14;
        s->opponent = OPPONENT_CLOCK;
        u->lost_vegas = earned;
        break;
    case RACE_HEAD_TO_HEAD:
        u->lost_vegas = earned;
        if (s->opponent >= OPPONENT_FIELD && s->opponent <= OPPONENT_TOURNAMENT) s->opponent = s->player_car;
        break;
    case RACE_SINGLE:
        f->unknown_4c5cc2 = 0, f->unknown_4bafb6 = 0x14;
        u->lost_vegas = earned;
        if (f->flag_52b312 && f->unknown_4fbb0c) s->unknown_524e9d = 8;
        s->opponent = other ? s->other_car[f->other_index] : OPPONENT_FIELD;
        break;
    case RACE_TOURNAMENT:
        f->unknown_4c5cc2 = 0, f->unknown_4bafb6 = 0x14;
        s->variant_m = s->variant_r = 0;
        if (f->flag_52b312 || !f->unlock_level) u->lost_vegas = 0;
        if (!u->lost_vegas && s->track == TRACK_LOST_VEGAS) s->track = 0;
        if (s->track == TRACK_HIDDEN) s->track = 0;
        s->player_car = TOURNAMENT_CAR[s->track];
        race_settings_car_flags(s);
        s->opponent = other ? s->other_car[f->other_index] : OPPONENT_TOURNAMENT;
        s->difficulty = 1;
        s->laps = short_track(s->track) ? 6 : 8;
        break;
    default:
        break;
    }
}

/* TODO(00432720): the draft's race type step; the level and the 00432974
 * session are guessed (the level from Lost Vegas, no 0052b312 game) and the
 * Lost Vegas it writes is dropped. */
static void race_type_changed(RaceSettings *s, const Unlocks *u)
{
    Unlocks scratch = *u;
    FrontendSession f = { 0 };
    f.unlock_level = u->lost_vegas;
    race_settings_type_changed(s, &scratch, &f);
}

void race_settings_init(RaceSettings *s)
{
    static const Unlocks none = { 0, 0, 0, 0, 0 };
    memset(s, 0, sizeof *s);
    race_settings_car_flags(s);
    track_changed(s);
    race_type_changed(s, &none);
}

void race_type_step(RaceSettings *s, const Unlocks *u, int dir)
{
    const int track = s->track;
    s->race_type = (s->race_type + (dir > 0 ? 1 : RACE_TYPES - 1)) % RACE_TYPES;
    race_type_changed(s, u);
    if (s->track != track) track_changed(s);
}

/* Tournament classes: tracks 0/3 race cars 0-2, tracks 1/4/5 cars 3-5,
 * the others cars 6-7. */
static void tournament_class(int track, int *low, int *high)
{
    *low = track == 0 || track == 3 ? 0 : track == 1 || track == 4 || track == 5 ? 3 : 6;
    *high = *low == 6 ? 7 : *low + 2;
}

void player_car_step(RaceSettings *s, const Unlocks *u, int dir)
{
    if (s->race_type == RACE_TOURNAMENT) {
        int low, high;
        tournament_class(s->track, &low, &high);
        s->player_car += dir > 0 ? 1 : -1;
        if (s->player_car > high) s->player_car = low;
        if (s->player_car < low) s->player_car = high;
    } else if (dir < 0) {
        s->player_car = s->player_car == CAR_WARRIOR ? CAR_COUNT - 1 : s->player_car - 1;
        if (s->player_car < 0) s->player_car = u->warrior ? CAR_WARRIOR : CAR_COUNT - 1;
    } else if (u->warrior && s->player_car == CAR_COUNT - 1) {
        s->player_car = CAR_WARRIOR;
    } else {
        s->player_car = s->player_car + 1 >= CAR_COUNT ? 0 : s->player_car + 1;
    }
    race_settings_car_flags(s);
}

void opponent_step(RaceSettings *s, int dir)
{
    const int last = s->race_type == RACE_SINGLE ? OPPONENT_FIELD : CAR_COUNT - 1;
    if (dir < 0) {
        s->opponent = s->opponent == CAR_WARRIOR ? last : s->opponent - 1;
        if (s->opponent < 0) s->opponent = last;
    } else {
        s->opponent = s->opponent == CAR_WARRIOR || s->opponent + 1 > last ? 0 : s->opponent + 1;
    }
}

void race_settings_next_track(RaceSettings *s, const Unlocks *u)
{
    if (s->track == TRACK_HIDDEN) {
        s->track = 0;
        return;
    }
    s->track = (s->track + 1) % (u->lost_vegas ? TRACK_COUNT + 1 : TRACK_COUNT);
    if (u->hidden && s->track == 0) s->track = TRACK_HIDDEN;
}

void race_settings_track_forward(RaceSettings *s, const Unlocks *u)
{
    const int variants_allowed = s->race_type != RACE_TOURNAMENT;
    if (s->variant_m) {
        s->variant_m = 0;
        if (u->variant_r && variants_allowed) {
            s->variant_r = 1;
            return;
        }
    } else if (s->variant_r) {
        s->variant_r = 0;
    } else if (u->variant_m && variants_allowed) {
        s->variant_m = 1;
        return;
    } else if (u->variant_r && variants_allowed) {
        s->variant_r = 1;
        return;
    }
    race_settings_next_track(s, u);
}

/* 00432d48: the same order backwards. */
void race_settings_track_backward(RaceSettings *s, const Unlocks *u)
{
    const int last = u->lost_vegas ? TRACK_LOST_VEGAS : TRACK_COUNT - 1;
    if (s->variant_r) {
        s->variant_r = 0;
        if (u->variant_m) s->variant_m = 1;
        return;
    }
    if (s->variant_m) {
        s->variant_m = 0;
        return;
    }
    /* the short's dec: -32768 wraps to 32767 */
    s->track = s->track == TRACK_HIDDEN ? last : (int16_t)(s->track - 1);
    if (s->track < 0) s->track = u->hidden ? TRACK_HIDDEN : last;
    if (s->track == TRACK_HIDDEN || s->race_type == RACE_TOURNAMENT) return;
    if (u->variant_r) s->variant_r = 1;
    else if (u->variant_m) s->variant_m = 1;
}

/* 00432e48 */
void unlocks_raise(Unlocks *u, int level)
{
    if (!u->lost_vegas) u->lost_vegas = level;
    if (level >= 1) {
        u->warrior = 1;
        u->variant_m = 1;
    }
    if (level >= 2) u->variant_r = 1;
}

void track_step(RaceSettings *s, const Unlocks *u, int dir)
{
    const int track = s->track;
    if (dir > 0) race_settings_track_forward(s, u);
    else race_settings_track_backward(s, u);
    if (s->track != track) track_changed(s);
}
