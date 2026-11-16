/* dep59 step 3/4: the menus' cars (game/frontend/car_list.h): the standard edition's are the original's (the eight, then the Warrior
 * when it is on offer), an edition's are its cars= in its order (T6). The pick config.dat's image takes (car_pick.h): never an added car,
 * the last original held standing for it while the image keeps it. */
#include "game/frontend/car_list.h"
#include "game/frontend/car_pick.h"
#include "game/race/car_registry.h"
#include "formats/edition_ini.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

static void add(const char *id, const char *stem)
{
    CarPiece p;
    memset(&p, 0, sizeof p);
    snprintf(p.id, sizeof p.id, "%s", id);
    snprintf(p.stem, sizeof p.stem, "%s", stem);
    p.sound_model = 0, p.ai = (CarAiTuning){ CAR_AI_PACE_MIN, CAR_AI_COAST_DEFAULT }, p.dash_model = -1;
    CHECK(car_registry_add(&p, NULL, 0) >= CAR_ADDED_FIRST);
}

static int listed(const CarList *l, const int16_t *want, int n)
{
    if (l->count != n) return 0;
    for (int i = 0; i < n; i++)
        if (l->car[i] != want[i]) return 0;
    return 1;
}

static void test_registry_cars(void)
{
    car_registry_clear();
    int16_t out[4];
    CHECK_EQ_INT(car_registry_cars(out, 4), 0);
    add("a", "A1"), add("b", "B1"), add("c", "C1");
    CHECK_EQ_INT(car_registry_cars(out, 4), 3);
    CHECK(out[0] == 16 && out[1] == 17 && out[2] == 18);
    CHECK_EQ_INT(car_registry_cars(out, 2), 2);   /* no more than cap */
    CHECK(car_registry_remove("a"));
    CHECK_EQ_INT(car_registry_cars(out, 4), 2);
    CHECK(out[0] == 17 && out[1] == 18);
}

/* an edition of `n` cars by id */
static Edition edition_of(const char *const *ids, int n)
{
    Edition e;
    memset(&e, 0, sizeof e);
    for (int i = 0; i < n; i++) snprintf(e.car[i], sizeof e.car[i], "%s", ids[i]);
    e.cars = n;
    return e;
}

static void test_the_list(void)
{
    car_registry_clear();
    add("a", "A1"), add("b", "B1");   /* 16 and 17 */
    CarList l;
    car_list_build(&l, NULL, 1);   /* the standard edition: the original's, never an added car */
    CHECK(listed(&l, (const int16_t[]){ 0, 1, 2, 3, 4, 5, 6, 7, 11 }, 9));
    car_list_build(&l, NULL, 0);
    CHECK(listed(&l, (const int16_t[]){ 0, 1, 2, 3, 4, 5, 6, 7 }, 8));
    CHECK_EQ_INT(car_list_step(&l, 7, 1), 0);
    CHECK_EQ_INT(car_list_step(&l, 0, -1), 7);
    /* an edition: its cars in its order, the stock ones by number, the added ones by their registered number, the missing ones out;
     * the Warrior when it lists it, whatever `warrior` says */
    Edition e = edition_of((const char *const[]){ "supra", "rx7", "warrior" }, 3);
    car_list_build(&l, &e, 0);
    CHECK(listed(&l, (const int16_t[]){ 2, 0, 11 }, 3));
    e = edition_of((const char *const[]){ "b", "viper", "gone", "a" }, 4);
    car_list_build(&l, &e, 1);
    CHECK(listed(&l, (const int16_t[]){ 17, 5, 16 }, 3));
    CHECK_EQ_INT(car_list_step(&l, 16, 1), 17);
    CHECK_EQ_INT(car_list_step(&l, 17, -1), 16);
    CHECK_EQ_INT(car_list_step(&l, 3, 1), 17);   /* not in the list: its first */
    e = edition_of((const char *const[]){ "rx7", "nsx" }, 2);
    car_list_build(&l, &e, 1);
    CHECK(listed(&l, (const int16_t[]){ 0, 1 }, 2));
    CHECK_EQ_INT(car_list_step(&l, 0, 1), 1);
    CHECK_EQ_INT(car_list_step(&l, 1, 1), 0);
    car_registry_clear();
}

/* the gallery's Previous and Next: the cars with a gallery (an original's 0..7, an added car's), the Warrior (none yet) passed over */
static void test_the_galleries_step(void)
{
    car_registry_clear();
    add("a", "A1");   /* 16 */
    CarList l;
    Edition e = edition_of((const char *const[]){ "diablo", "a", "warrior" }, 3);
    car_list_build(&l, &e, 0);
    CHECK_EQ_INT(car_list_step_gallery(&l, 16, 1), 11);   /* dep59 section 8: the Warrior has its page */
    CHECK_EQ_INT(car_list_step_gallery(&l, 16, -1), 7);
    CHECK_EQ_INT(car_list_step_gallery(&l, 7, -1), 11);   /* round back to it */
    e = edition_of((const char *const[]){ "a", "warrior", "rx7", "nsx" }, 4);
    car_list_build(&l, &e, 0);
    CHECK_EQ_INT(car_list_step_gallery(&l, 16, 1), 11);
    CHECK_EQ_INT(car_list_step_gallery(&l, 0, -1), 11);
    CHECK_EQ_INT(car_list_step_gallery(&l, 0, 1), 1);
    e = edition_of((const char *const[]){ "a", "warrior" }, 2);
    car_list_build(&l, &e, 0);
    CHECK_EQ_INT(car_list_step_gallery(&l, 16, 1), 11);
    CHECK_EQ_INT(car_list_step_gallery(&l, 16, -1), 11);
    e = edition_of((const char *const[]){ "a" }, 1);
    car_list_build(&l, &e, 0);
    CHECK_EQ_INT(car_list_step_gallery(&l, 16, 1), 16);   /* no other gallery: the car's own again */
    car_list_build(&l, NULL, 1);
    CHECK_EQ_INT(car_list_step_gallery(&l, 7, 1), 11);    /* the standard edition: 7, the Warrior, 0 */
    CHECK_EQ_INT(car_list_step_gallery(&l, 11, 1), 0);
    CHECK_EQ_INT(car_list_step_gallery(&l, 0, -1), 11);
    car_registry_clear();
}

/* Review Focus 7: config.dat's image only ever holds an original */
static void test_the_pick(void)
{
    CarPick p = { 0 };
    CHECK_EQ_INT(car_pick_from_image(&p, 5), 5);   /* nothing added picked yet */
    CHECK_EQ_INT(car_pick_to_image(&p, 5), 5);
    CHECK_EQ_INT(car_pick_to_image(&p, 16), 5);    /* the last original held */
    CHECK_EQ_INT(car_pick_from_image(&p, 5), 16);  /* while the image keeps it, it stands for the added pick */
    CHECK_EQ_INT(car_pick_from_image(&p, 3), 3);   /* the image changed: that car */
    CHECK_EQ_INT(car_pick_to_image(&p, 17), 5);
    CHECK_EQ_INT(car_pick_from_image(&p, 5), 17);
    CHECK_EQ_INT(car_pick_to_image(&p, 3), 3);     /* an original picked: the added one is let go */
    CHECK_EQ_INT(car_pick_from_image(&p, 3), 3);
    CHECK_EQ_INT(car_pick_from_image(&p, 5), 5);
    CarPick q = { 0 };
    CHECK_EQ_INT(car_pick_to_image(&q, 16), 0);    /* no original held yet: car 0 */
    CHECK_EQ_INT(car_pick_to_image(&q, 11), 11);   /* the Warrior and the race types' 8..10 are the image's own */
    CHECK_EQ_INT(car_pick_to_image(&q, 8), 8);
}

int main(void)
{
    test_registry_cars();
    test_the_list();
    test_the_galleries_step();
    test_the_pick();
    return TEST_REPORT();
}
