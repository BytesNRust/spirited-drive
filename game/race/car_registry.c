/* See car_registry.h. */
#include "game/race/car_registry.h"

#include "formats/car_piece.h"
#include "game/race/car_load.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

static CarPiece pieces[CAR_ADDED_MAX];
static int used[CAR_ADDED_MAX];

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

int car_registry_find(const char *id)
{
    if (id == NULL) return -1;
    for (int k = 0; k < CAR_ADDED_MAX; k++)
        if (used[k] && strcmp(pieces[k].id, id) == 0) return CAR_ADDED_FIRST + k;
    return -1;
}

/* 1 when another added car has `stem`, whatever the case */
static int stem_added(const char *stem)
{
    for (int k = 0; k < CAR_ADDED_MAX; k++)
        if (used[k] && strcasecmp(stem, pieces[k].stem) == 0) return 1;
    return 0;
}

int car_registry_add(const CarPiece *piece, char *why, size_t why_cap)
{
    if (piece == NULL || piece->id[0] == '\0') return refuse(why, why_cap, "a car piece has no id");
    if (car_registry_find(piece->id) >= 0) return refuse(why, why_cap, "the id %s is a car already", piece->id);
    if (!car_piece_stem_ok(piece->stem)) return refuse(why, why_cap, "the stem \"%s\" is not 1 to 8 of A-Z and 0-9, or is an original car's", piece->stem);
    if (stem_added(piece->stem)) return refuse(why, why_cap, "the stem %s is another added car's", piece->stem);
    if (piece->sound_model < 0 || piece->sound_model >= CAR_MODELS) return refuse(why, why_cap, "sound_from is no original car");
    if (piece->ai_model < 0 || piece->ai_model >= CAR_ORIGINAL_MODELS) return refuse(why, why_cap, "ai_from is no original car");
    for (int k = 0; k < CAR_ADDED_MAX; k++) {
        if (used[k]) continue;
        pieces[k] = *piece;
        used[k] = 1;
        return CAR_ADDED_FIRST + k;
    }
    return refuse(why, why_cap, "there are %d added cars already, the most there may be", CAR_ADDED_MAX);
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

int car_registry_ai_model(int car)
{
    if (car >= 0 && car < CAR_ORIGINAL_MODELS) return car;
    const CarPiece *p = car_registry_piece(car);
    return p != NULL ? p->ai_model : -1;
}
