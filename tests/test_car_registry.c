/* dep59 step 1: the registry of added cars (game/race/car_registry.h). The exe's twelve keep their numbers and names; an added car gets the
 * lowest free number from CAR_ADDED_FIRST and keeps it while it stays registered (a number in another's hand never comes to name another car);
 * a second id or stem, a stem car_piece_stem_ok refuses (an original's, or one with the reduced model's 'M'), a model outside the originals, the 65th car: refused. */
#include "formats/car_piece.h"
#include "game/race/car_load.h"
#include "game/race/car_registry.h"
#include "test.h"

#include <string.h>

static CarPiece piece(const char *id, const char *stem, int sound, int ai)
{
    CarPiece p;
    memset(&p, 0, sizeof p);
    snprintf(p.id, sizeof p.id, "%s", id);
    snprintf(p.name, sizeof p.name, "%s", id);
    snprintf(p.stem, sizeof p.stem, "%s", stem);
    snprintf(p.short_name, sizeof p.short_name, "%s", stem);
    p.sound_model = sound, p.ai_model = ai, p.dash_model = -1;
    return p;
}

static int add(const char *id, const char *stem, int sound, int ai, char *why)
{
    const CarPiece p = piece(id, stem, sound, ai);
    why[0] = '\0';
    return car_registry_add(&p, why, 160);
}

static void test_originals_unchanged(void)
{
    car_registry_clear();
    for (int car = 0; car < CAR_MODELS; car++) {
        CHECK(car_registry_stem(car) == car_model_names[car]);
        CHECK_EQ_INT(car_registry_sound_model(car), car);
        CHECK_EQ_INT(car_registry_is_added(car), 0);
        CHECK(car_registry_piece(car) == NULL);
    }
    for (int car = 0; car < CAR_ORIGINAL_MODELS; car++) CHECK_EQ_INT(car_registry_ai_model(car), car);
    CHECK_EQ_INT(car_registry_ai_model(8), -1);
    CHECK(car_registry_stem(12) == NULL && car_registry_stem(16) == NULL && car_registry_stem(-1) == NULL);
    CHECK_EQ_INT(car_registry_sound_model(16), -1);
    CHECK_EQ_INT(car_registry_ai_model(16), -1);
    CHECK_EQ_INT(car_registry_is_added(16), 0);
}

static void test_add_and_look_up(void)
{
    char why[160];
    car_registry_clear();
    CHECK_EQ_INT(add("f40", "F40", 4, 1, why), 16);
    CHECK_EQ_INT(car_registry_find("f40"), 16);
    CHECK(strcmp(car_registry_stem(16), "F40") == 0);
    CHECK_EQ_INT(car_registry_sound_model(16), 4);
    CHECK_EQ_INT(car_registry_ai_model(16), 1);
    CHECK_EQ_INT(car_registry_is_added(16), 1);
    CHECK(car_registry_piece(16) != NULL && strcmp(car_registry_piece(16)->id, "f40") == 0);
    CHECK_EQ_INT(add("cobra", "COBRA", 0, 0, why), 17);
    CHECK_EQ_INT(car_registry_count(), 2);
    CHECK_EQ_INT(car_registry_find("nope"), -1);
    CHECK_EQ_INT(car_registry_find(NULL), -1);
}

static void test_numbers_stay(void)
{
    char why[160];
    car_registry_clear();
    CHECK_EQ_INT(add("a", "A1", 0, 0, why), 16);
    CHECK_EQ_INT(add("b", "B1", 0, 0, why), 17);
    CHECK_EQ_INT(add("c", "C1", 0, 0, why), 18);
    CHECK_EQ_INT(car_registry_remove("a"), 1);
    CHECK_EQ_INT(car_registry_remove("a"), 0);   /* not there any more */
    CHECK_EQ_INT(car_registry_find("b"), 17);    /* the others keep theirs */
    CHECK_EQ_INT(car_registry_find("c"), 18);
    CHECK_EQ_INT(car_registry_is_added(16), 0);
    CHECK(car_registry_stem(16) == NULL);
    CHECK_EQ_INT(car_registry_count(), 2);
    CHECK_EQ_INT(add("d", "D1", 0, 0, why), 16);  /* a new one takes the free number */
    CHECK_EQ_INT(add("a", "A1", 0, 0, why), 19);
    CHECK_EQ_INT(add("e", "A1", 0, 0, why), -1);  /* a removed car's stem is free only while it is removed */
}

static void test_refusals(void)
{
    char why[160];
    car_registry_clear();
    CHECK_EQ_INT(add("f40", "F40", 4, 1, why), 16);
    CHECK_EQ_INT(add("f40", "OTHER", 4, 1, why), -1);   /* the same id */
    CHECK(strstr(why, "id") != NULL);
    CHECK_EQ_INT(add("f40b", "F40", 4, 1, why), -1);   /* the same stem */
    CHECK(strstr(why, "stem") != NULL);
    CHECK_EQ_INT(add("f40c", "f40", 4, 1, why), -1);   /* not A-Z 0-9 */
    static const char *const TAKEN[] = { "DVIPER", "MRX7", "MTSUPRA", "TRAFFC", "COPMUST", "P911T", "MP911T", "WAGON", "MJEEP" };
    for (size_t i = 0; i < sizeof TAKEN / sizeof TAKEN[0]; i++) CHECK_EQ_INT(add("x", TAKEN[i], 0, 0, why), -1);
    CHECK_EQ_INT(add("x", "", 0, 0, why), -1);
    CHECK_EQ_INT(add("", "X1", 0, 0, why), -1);
    CHECK_EQ_INT(add("x", "X1", 12, 0, why), -1);   /* the sound of a car past the twelve */
    CHECK_EQ_INT(add("x", "X1", -1, 0, why), -1);
    CHECK_EQ_INT(add("x", "X1", 0, 8, why), -1);    /* the pace of one past the eight */
    CHECK_EQ_INT(add("x", "X1", 0, -1, why), -1);
    CHECK_EQ_INT(car_registry_count(), 1);
    car_registry_clear();
    for (int i = 0; i < CAR_ADDED_MAX; i++) {
        char id[16], stem[9];
        snprintf(id, sizeof id, "car%d", i);
        snprintf(stem, sizeof stem, "CAR%d", i);
        CHECK_EQ_INT(add(id, stem, 0, 0, why), CAR_ADDED_FIRST + i);
    }
    CHECK_EQ_INT(add("one-more", "ONEMORE", 0, 0, why), -1);
    CHECK(strstr(why, "64") != NULL);
    CHECK_EQ_INT(car_registry_count(), CAR_ADDED_MAX);
    CHECK_EQ_INT(car_registry_is_added(CAR_ADDED_FIRST + CAR_ADDED_MAX), 0);
}

/* every root the race loads an original by is one no added car may take (car_piece.c keeps its own list, from slots.h) */
static void test_the_race_stems_are_kept(void)
{
    for (int car = 0; car < CAR_MODELS; car++) {
        char upper[16];
        size_t i = 0;
        for (; car_model_names[car][i] != '\0' && i + 1 < sizeof upper; i++)
            upper[i] = (char)(car_model_names[car][i] >= 'a' && car_model_names[car][i] <= 'z' ? car_model_names[car][i] - 32 : car_model_names[car][i]);
        upper[i] = '\0';
        CHECK_EQ_INT(car_piece_stem_ok(upper), 0);
        char reduced[18];
        snprintf(reduced, sizeof reduced, "M%s", upper);
        CHECK_EQ_INT(car_piece_stem_ok(reduced), 0);
    }
}

static void test_clear(void)
{
    char why[160];
    car_registry_clear();
    CHECK_EQ_INT(add("f40", "F40", 4, 1, why), 16);
    car_registry_clear();
    CHECK_EQ_INT(car_registry_count(), 0);
    CHECK_EQ_INT(car_registry_find("f40"), -1);
    CHECK_EQ_INT(car_registry_is_added(16), 0);
}

int main(void)
{
    test_originals_unchanged();
    test_add_and_look_up();
    test_numbers_stay();
    test_refusals();
    test_the_race_stems_are_kept();
    test_clear();
    return TEST_REPORT();
}
