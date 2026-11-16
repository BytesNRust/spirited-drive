/* dep59 step 4: the stock cars' and tracks' ids an edition names them by (formats/stock_content.h): the eight race cars and Warrior by
 * their numbers in the menus, the ten tracks by theirs; each id back to its number, and no id for anything else. */
#include "formats/stock_content.h"
#include "formats/edition_ini.h"
#include "test.h"

#include <string.h>

static void test_cars(void)
{
    static const char *const CARS[] = { "rx7", "nsx", "supra", "911", "zr1", "viper", "512tr", "diablo" };
    for (int car = 0; car < 8; car++) {
        CHECK(stock_car_id(car) != NULL && strcmp(stock_car_id(car), CARS[car]) == 0);
        CHECK_EQ_INT(stock_car_of_id(CARS[car]), car);
    }
    CHECK(stock_car_id(11) != NULL && strcmp(stock_car_id(11), "warrior") == 0);
    CHECK_EQ_INT(stock_car_of_id("warrior"), 11);
    for (int car = 8; car <= 10; car++) CHECK(stock_car_id(car) == NULL);   /* PACK, CLOCK and CLASS are no car of an edition */
    CHECK(stock_car_id(-1) == NULL && stock_car_id(12) == NULL && stock_car_id(16) == NULL);
    CHECK_EQ_INT(stock_car_of_id("vette"), -1);
    CHECK_EQ_INT(stock_car_of_id("Viper"), -1);
    CHECK_EQ_INT(stock_car_of_id(""), -1);
    CHECK_EQ_INT(stock_car_of_id(NULL), -1);
}

static void test_tracks(void)
{
    static const char *const TRACKS[] = { "city", "coastal", "alpine", "rusty", "autumn", "burnt", "vertigo", "transtropolis", "vegas",
                                          "hidden" };
    for (int track = 0; track < 10; track++) {
        CHECK(stock_track_id(track) != NULL && strcmp(stock_track_id(track), TRACKS[track]) == 0);
        CHECK_EQ_INT(stock_track_of_id(TRACKS[track]), track);
    }
    CHECK_EQ_INT(stock_track_of_id("hidden"), 9);
    CHECK(stock_track_id(-1) == NULL && stock_track_id(10) == NULL);
    CHECK_EQ_INT(stock_track_of_id("coast"), -1);
    CHECK_EQ_INT(stock_track_of_id(NULL), -1);
}

/* every stock id can stand in an edition's file, and no car's id is a track's */
static void test_ids_fit_an_edition(void)
{
    for (int car = 0; car < 12; car++)
        if (stock_car_id(car) != NULL) {
            CHECK_EQ_INT(edition_id_ok(stock_car_id(car)), 1);
            CHECK_EQ_INT(stock_track_of_id(stock_car_id(car)), -1);
        }
    for (int track = 0; track < 10; track++) CHECK_EQ_INT(edition_id_ok(stock_track_id(track)), 1);
}

int main(void)
{
    test_cars();
    test_tracks();
    test_ids_fit_an_edition();
    return TEST_REPORT();
}
