#include "game/frontend/race_settings.h"

#include <string.h>

/* 004baef8: tournament car class per track (first car of the class). */
static const int TOURNAMENT_CAR[10] = { 0, 3, 6, 0, 3, 3, 6, 6, 7, 3 };
/* 00461f1c / 00461f34: per-car tables read by 00465ec8. */
static const int CAR_TABLE_A[12] = { 1, 1, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0 };
static const int CAR_TABLE_B[12] = { 0, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0 };

/* 00465ec8 */
static void update_car_flags(RaceSettings *s)
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
        update_car_flags(s);
        s->opponent = OPPONENT_TOURNAMENT;
    }
    static const int LAPS[3][2] = { { 4, 2 }, { 8, 6 }, { 16, 12 } };
    const int level = s->difficulty < 0 ? 0 : s->difficulty > 2 ? 2 : s->difficulty;
    s->laps = LAPS[level][short_track(s->track)];
}

/* 00432974, single player. */
static void race_type_changed(RaceSettings *s, const Unlocks *u)
{
    switch (s->race_type) {
    case RACE_TIME_TRIAL:
        s->opponent = OPPONENT_CLOCK;
        break;
    case RACE_HEAD_TO_HEAD:
        if (s->opponent >= OPPONENT_FIELD && s->opponent <= OPPONENT_TOURNAMENT) s->opponent = s->player_car;
        break;
    case RACE_SINGLE:
        s->opponent = OPPONENT_FIELD;
        break;
    default:
        s->variant_m = s->variant_r = 0;
        if ((s->track == TRACK_LOST_VEGAS && !u->lost_vegas) || s->track == TRACK_HIDDEN) s->track = 0;
        s->player_car = TOURNAMENT_CAR[s->track];
        update_car_flags(s);
        s->opponent = OPPONENT_TOURNAMENT;
        s->difficulty = 1;
        s->laps = short_track(s->track) ? 6 : 8;
        break;
    }
}

void race_settings_init(RaceSettings *s)
{
    static const Unlocks none = { 0, 0, 0, 0, 0 };
    memset(s, 0, sizeof *s);
    update_car_flags(s);
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
    update_car_flags(s);
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

/* 00432cd4 */
static void next_track(RaceSettings *s, const Unlocks *u)
{
    if (s->track == TRACK_HIDDEN) {
        s->track = 0;
        return;
    }
    s->track = (s->track + 1) % (u->lost_vegas ? TRACK_COUNT + 1 : TRACK_COUNT);
    if (u->hidden && s->track == 0) s->track = TRACK_HIDDEN;
}

/* 00432c40: base -> "m" -> "r" -> next track, variants only when unlocked
 * and never in a tournament. */
static void track_forward(RaceSettings *s, const Unlocks *u)
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
    next_track(s, u);
}

/* 00432d48: the same order backwards. */
static void track_backward(RaceSettings *s, const Unlocks *u)
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
    s->track = s->track == TRACK_HIDDEN ? last : s->track - 1;
    if (s->track < 0) s->track = u->hidden ? TRACK_HIDDEN : last;
    if (s->track == TRACK_HIDDEN || s->race_type == RACE_TOURNAMENT) return;
    if (u->variant_r) s->variant_r = 1;
    else if (u->variant_m) s->variant_m = 1;
}

void track_step(RaceSettings *s, const Unlocks *u, int dir)
{
    const int track = s->track;
    if (dir > 0) track_forward(s, u);
    else track_backward(s, u);
    if (s->track != track) track_changed(s);
}
