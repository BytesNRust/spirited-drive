/* See car_registry.h. */
#include "game/race/car_registry.h"

#include "formats/car_piece.h"
#include "game/race/car_load.h"
#include "game/race/car_specs.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include <stddef.h>

/* the PBS fields car_piece_check reads are the ones the race reads (formats/car_piece.h) */
typedef char car_piece_offsets_are_the_races[offsetof(CarSpecs, power) == CAR_PIECE_PBS_POWER && offsetof(CarSpecs, torque_points) == CAR_PIECE_PBS_TORQUE_POINTS
                                             && offsetof(CarSpecs, torque_curve) == CAR_PIECE_PBS_TORQUE_CURVE && offsetof(CarSpecs, shift_rpm) == CAR_PIECE_PBS_SHIFT_RPM
                                             && offsetof(CarSpecs, unknown_320) == CAR_PIECE_PBS_UNKNOWN_320 && offsetof(CarSpecs, force_scale) == CAR_PIECE_PBS_FORCE_SCALE ? 1 : -1];

static CarPiece pieces[CAR_ADDED_MAX];
static int used[CAR_ADDED_MAX];
static int race_has_added;

static int refuse(char *why, size_t cap, const char *format, ...) __attribute__((format(printf, 3, 4)));
static int refuse(char *why, size_t cap, const char *format, ...)
{
    if (why != NULL && cap > 0) {
        va_list ap;
        va_start(ap, format);
        vsnprintf(why, cap, format, ap);
        va_end(ap);
    }
    return -1;
}

void car_registry_clear(void)
{
    memset(used, 0, sizeof used);
    memset(pieces, 0, sizeof pieces);
}

int car_registry_count(void)
{
    int n = 0;
    for (int k = 0; k < CAR_ADDED_MAX; k++) n += used[k];
    return n;
}

int car_registry_is_added(int car)
{
    const int k = car - CAR_ADDED_FIRST;
    return k >= 0 && k < CAR_ADDED_MAX && used[k];
}

const CarPiece *car_registry_piece(int car)
{
    return car_registry_is_added(car) ? &pieces[car - CAR_ADDED_FIRST] : NULL;
}

int car_registry_cars(int16_t out[], int cap)
{
    int n = 0;
    for (int k = 0; k < CAR_ADDED_MAX && n < cap; k++)
        if (used[k]) out[n++] = (int16_t)(CAR_ADDED_FIRST + k);
    return n;
}

int car_registry_find(const char *id)
{
    if (id == NULL) return -1;
    for (int k = 0; k < CAR_ADDED_MAX; k++)
        if (used[k] && strcmp(pieces[k].id, id) == 0) return CAR_ADDED_FIRST + k;
    return -1;
}

/* 1 when an added car other than slot `self` (-1: any) has `stem`, whatever the case */
static int stem_added(const char *stem, int self)
{
    for (int k = 0; k < CAR_ADDED_MAX; k++)
        if (k != self && used[k] && strcasecmp(stem, pieces[k].stem) == 0) return 1;
    return 0;
}

/* -1 with the reason when `piece` may not be car `self`'s (-1: a new car's), else 0 */
static int piece_ok(const CarPiece *piece, int self, char *why, size_t why_cap)
{
    if (!car_piece_stem_ok(piece->stem)) return refuse(why, why_cap, "the stem \"%s\" is not 1 to 8 of A-Z and 0-9, or is an original car's", piece->stem);
    if (stem_added(piece->stem, self)) return refuse(why, why_cap, "the stem %s is another added car's", piece->stem);
    if (piece->sound_model < 0 || piece->sound_model >= CAR_MODELS) return refuse(why, why_cap, "sound_from is no original car");
    return 0;
}

int car_registry_add(const CarPiece *piece, char *why, size_t why_cap)
{
    if (piece == NULL || piece->id[0] == '\0') return refuse(why, why_cap, "a car piece has no id");
    if (car_registry_find(piece->id) >= 0) return refuse(why, why_cap, "the id %s is a car already", piece->id);
    if (piece_ok(piece, -1, why, why_cap) < 0) return -1;
    for (int k = 0; k < CAR_ADDED_MAX; k++) {
        if (used[k]) continue;
        pieces[k] = *piece;
        used[k] = 1;
        return CAR_ADDED_FIRST + k;
    }
    return refuse(why, why_cap, "there are %d added cars already, the most there may be", CAR_ADDED_MAX);
}

int car_registry_update(const CarPiece *piece, char *why, size_t why_cap)
{
    const int car = piece != NULL ? car_registry_find(piece->id) : -1;
    if (car < 0) return refuse(why, why_cap, "no added car has the id %s", piece != NULL ? piece->id : "");
    if (piece_ok(piece, car - CAR_ADDED_FIRST, why, why_cap) < 0) return -1;
    pieces[car - CAR_ADDED_FIRST] = *piece;
    return car;
}

int car_registry_remove(const char *id)
{
    const int car = car_registry_find(id);
    if (car < 0) return 0;
    used[car - CAR_ADDED_FIRST] = 0;
    memset(&pieces[car - CAR_ADDED_FIRST], 0, sizeof pieces[0]);
    return 1;
}

const char *car_registry_stem(int car)
{
    if (car >= 0 && car < CAR_MODELS) return car_model_names[car];
    const CarPiece *p = car_registry_piece(car);
    return p != NULL ? p->stem : NULL;
}

int car_registry_sound_model(int car)
{
    if (car >= 0 && car < CAR_MODELS) return car;
    const CarPiece *p = car_registry_piece(car);
    return p != NULL ? p->sound_model : -1;
}

int car_registry_dash_model(int car)
{
    if (car >= 0 && car < CAR_MODELS) return car;
    const CarPiece *p = car_registry_piece(car);
    return p == NULL ? -1 : p->dash_model;   /* port (dep59 3b): -1 without dash_from, never its sound_from's */
}

int car_registry_lamp_model(int car)
{
    if (car >= 0 && car < CAR_MODELS) return car;
    return car_registry_piece(car) != NULL ? CAR_LAMP_STANDARD : -1;   /* port (dep59 3b): not its sound_from's */
}

int car_registry_ai_model(int car)
{
    return car >= 0 && car < CAR_ORIGINAL_MODELS ? car : -1;   /* port (dep59 3b): an added car's AI is its own (car_registry_ai) */
}

const CarAiTuning *car_registry_ai(int car)
{
    const CarPiece *p = car_registry_piece(car);
    return p != NULL ? &p->ai : NULL;
}

const char *car_registry_short_name(int car, const char *const names[], int count)
{
    if (car >= 0 && car < count) return names[car];
    const CarPiece *p = car_registry_piece(car);
    return p != NULL ? p->short_name : "";
}

int car_registry_entries_added(const RaceEntry *entries, int count)
{
    if (entries == NULL) return 0;
    for (int i = 0; i < count && i < RACE_PLAYERS; i++)
        if (car_registry_is_added(entries[i].car)) return 1;
    return 0;
}

void car_registry_race_set(int has_added) { race_has_added = has_added != 0; }
static int race_player = -1;
void car_registry_race_player_set(int car) { race_player = car; }
int car_registry_race_player(void) { return race_player; }
int car_registry_race_has_added(void) { return race_has_added; }
