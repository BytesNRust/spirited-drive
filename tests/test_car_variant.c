/* The car showcase box's variant list on fake entries: the choices a car has, the cycle through them and which draws. */
#include "game/race/car_variant.h"
#include "test.h"

enum { MODELS = 4 };
static int offered_a[MODELS], offered_b[MODELS], chosen_a[MODELS], chosen_b[MODELS];
static int last_on, drawn_a, drawn_b;

static int has_a(int m) { return offered_a[m]; }
static int has_b(int m) { return offered_b[m]; }
static int is_a(int m) { return chosen_a[m]; }
static int is_b(int m) { return chosen_b[m]; }
static void put_a(int m, int on) { chosen_a[m] = on; }
static void put_b(int m, int on) { chosen_b[m] = on; }
static void draw_a(const CarVariantCanvas *c, int m, int on) { (void)c, (void)m, last_on = on, drawn_a++; }
static void draw_b(const CarVariantCanvas *c, int m, int on) { (void)c, (void)m, last_on = on, drawn_b++; }

static const CarVariant LIST[2] = {
    { "a", CAR_VARIANT_COLOUR, has_a, is_a, put_a, draw_a },
    { "b", CAR_VARIANT_DECAL, has_b, is_b, put_b, draw_b },
};

static void test_choices(void)
{
    CHECK_EQ_INT(car_variant_choices(LIST, 2, 0), 0);
    offered_a[1] = 1;
    CHECK_EQ_INT(car_variant_choices(LIST, 2, 1), 2);
    offered_b[1] = 1;
    CHECK_EQ_INT(car_variant_choices(LIST, 2, 1), 3);
    CHECK_EQ_INT(car_variant_choices(LIST, 2, 2), 0);
}

static void test_cycle(void)
{
    CHECK_EQ_INT(car_variant_current(LIST, 2, 1), 0);
    CHECK_EQ_INT(car_variant_cycle(LIST, 2, 1), 1);
    CHECK(chosen_a[1] == 1 && chosen_b[1] == 0);
    CHECK_EQ_INT(car_variant_current(LIST, 2, 1), 1);
    CHECK_EQ_INT(car_variant_cycle(LIST, 2, 1), 2);
    CHECK(chosen_a[1] == 0 && chosen_b[1] == 1);
    CHECK_EQ_INT(car_variant_cycle(LIST, 2, 1), 0);
    CHECK(chosen_a[1] == 0 && chosen_b[1] == 0);
    /* a car with only the second variant: its one choice is the second's */
    offered_b[2] = 1;
    CHECK_EQ_INT(car_variant_cycle(LIST, 2, 2), 1);
    CHECK(chosen_b[2] == 1 && chosen_a[2] == 0);
    CHECK_EQ_INT(car_variant_cycle(LIST, 2, 2), 0);
    /* no variant offered: nothing to cycle */
    CHECK_EQ_INT(car_variant_cycle(LIST, 2, 3), 0);
    CHECK(chosen_a[3] == 0 && chosen_b[3] == 0);
}

static void test_draw(void)
{
    const CarVariantCanvas canvas = { 0, 0, 0, 0, 1, 1 };
    CHECK_EQ_INT(car_variant_draw(LIST, 2, 3, &canvas), 0);
    CHECK_EQ_INT(car_variant_draw(LIST, 2, 1, &canvas), 1);      /* as it is: the first variant's preview, off */
    CHECK(drawn_a == 1 && last_on == 0);
    car_variant_cycle(LIST, 2, 1);
    car_variant_draw(LIST, 2, 1, &canvas);
    CHECK(drawn_a == 2 && last_on == 1);
    car_variant_cycle(LIST, 2, 1);
    car_variant_draw(LIST, 2, 1, &canvas);
    CHECK(drawn_b == 1 && last_on == 1);
}

int main(void)
{
    test_choices();
    test_cycle();
    test_draw();
    return TEST_REPORT();
}
