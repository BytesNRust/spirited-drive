/* The car showcase box's variant list on fake entries: the choices a car has, the cycle through them and which draws. */
#include "game/race/car_variant.h"
#include "test.h"

enum { MODELS = 4 };
static int offered_a[MODELS], offered_b[MODELS], offered_c[MODELS], chosen_a[MODELS], chosen_b[MODELS], chosen_c[MODELS];
static int last_on, drawn_a, drawn_b;

static int has_a(int m) { return offered_a[m]; }
static int has_b(int m) { return offered_b[m]; }
static int has_c(int m) { return offered_c[m]; }
static int is_a(int m) { return chosen_a[m]; }
static int is_b(int m) { return chosen_b[m]; }
static int is_c(int m) { return chosen_c[m]; }
static void put_a(int m, int on) { chosen_a[m] = on; }
static void put_b(int m, int on) { chosen_b[m] = on; }
static void put_c(int m, int on) { chosen_c[m] = on; }
static void draw_a(const CarVariantCanvas *c, int m, int on) { (void)c, (void)m, last_on = on, drawn_a++; }
static void draw_c(const CarVariantCanvas *c, int m, int on) { (void)c, (void)m, (void)on; }
static void draw_b(const CarVariantCanvas *c, int m, int on) { (void)c, (void)m, last_on = on, drawn_b++; }

static const CarVariant LIST[2] = {
    { "a", CAR_VARIANT_COLOUR, has_a, is_a, put_a, draw_a },
    { "b", CAR_VARIANT_DECAL, has_b, is_b, put_b, draw_b },
};
static const CarVariant LIST3[3] = {
    { "a", CAR_VARIANT_COLOUR, has_a, is_a, put_a, draw_a },
    { "b", CAR_VARIANT_DECAL, has_b, is_b, put_b, draw_b },
    { "c", CAR_VARIANT_DECAL, has_c, is_c, put_c, draw_c },
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
    CHECK_EQ_INT(car_variant_step(LIST, 2, 1, 1, 1), 1);
    CHECK(chosen_a[1] == 1 && chosen_b[1] == 0);
    CHECK_EQ_INT(car_variant_current(LIST, 2, 1), 1);
    CHECK_EQ_INT(car_variant_step(LIST, 2, 1, 1, 1), 2);
    CHECK(chosen_a[1] == 0 && chosen_b[1] == 1);
    CHECK_EQ_INT(car_variant_step(LIST, 2, 1, 1, 1), 0);
    CHECK(chosen_a[1] == 0 && chosen_b[1] == 0);
    /* backwards: from the car as it is to the last choice, and back */
    CHECK_EQ_INT(car_variant_step(LIST, 2, 1, -1, 1), 2);
    CHECK(chosen_a[1] == 0 && chosen_b[1] == 1);
    CHECK_EQ_INT(car_variant_step(LIST, 2, 1, -1, 1), 1);
    CHECK_EQ_INT(car_variant_step(LIST, 2, 1, -1, 1), 0);
    /* a car with only the second variant: its one choice is the second's */
    offered_b[2] = 1;
    CHECK_EQ_INT(car_variant_step(LIST, 2, 2, 1, 1), 1);
    CHECK(chosen_b[2] == 1 && chosen_a[2] == 0);
    CHECK_EQ_INT(car_variant_step(LIST, 2, 2, 1, 1), 0);
    /* no variant offered: nothing to cycle */
    CHECK_EQ_INT(car_variant_step(LIST, 2, 3, 1, 1), 0);
    CHECK(chosen_a[3] == 0 && chosen_b[3] == 0);
}

static void test_draw(void)
{
    const CarVariantCanvas canvas = { 0, 0, 0, 0, 1, 1 };
    CHECK_EQ_INT(car_variant_draw(LIST, 2, 3, &canvas), 0);
    CHECK_EQ_INT(car_variant_draw(LIST, 2, 1, &canvas), 1);      /* as it is: the first variant's preview, off */
    CHECK(drawn_a == 1 && last_on == 0);
    car_variant_step(LIST, 2, 1, 1, 1);
    car_variant_draw(LIST, 2, 1, &canvas);
    CHECK(drawn_a == 2 && last_on == 1);
    car_variant_step(LIST, 2, 1, 1, 1);
    car_variant_draw(LIST, 2, 1, &canvas);
    CHECK(drawn_b == 1 && last_on == 1);
}

/* the paging triangles and the two ways of stepping: Left and Right stop at the ends, Enter wraps */
static void test_paging(void)
{
    offered_a[0] = offered_b[0] = offered_c[0] = 1;   /* model 0: four choices, the car as it is and three */
    CHECK_EQ_INT(car_variant_choices(LIST3, 3, 0), 4);
    CHECK(!car_variant_can_step(LIST3, 3, 0, -1) && car_variant_can_step(LIST3, 3, 0, 1));    /* first: ► only */
    CHECK_EQ_INT(car_variant_step(LIST3, 3, 0, 1, 0), 1);
    CHECK(car_variant_can_step(LIST3, 3, 0, -1) && car_variant_can_step(LIST3, 3, 0, 1));     /* middle: both */
    CHECK_EQ_INT(car_variant_step(LIST3, 3, 0, 1, 0), 2);
    CHECK_EQ_INT(car_variant_step(LIST3, 3, 0, 1, 0), 3);
    CHECK(car_variant_can_step(LIST3, 3, 0, -1) && !car_variant_can_step(LIST3, 3, 0, 1));    /* last: ◄ only */
    CHECK_EQ_INT(car_variant_step(LIST3, 3, 0, 1, 0), 3);                                     /* no wrap on Right */
    CHECK(chosen_c[0] == 1 && chosen_a[0] == 0 && chosen_b[0] == 0);
    CHECK_EQ_INT(car_variant_step(LIST3, 3, 0, 1, 1), 0);                                     /* Enter wraps */
    CHECK(chosen_a[0] == 0 && chosen_b[0] == 0 && chosen_c[0] == 0);
    CHECK_EQ_INT(car_variant_step(LIST3, 3, 0, -1, 0), 0);                                    /* no wrap on Left */
    CHECK_EQ_INT(car_variant_step(LIST3, 3, 0, -1, 1), 3);
    /* the two-choice case of today's single variant: normal shows ► only, the variant ◄ only */
    car_variant_step(LIST3, 3, 0, 1, 1);
    offered_b[0] = offered_c[0] = 0;
    chosen_a[0] = chosen_b[0] = chosen_c[0] = 0;
    CHECK(!car_variant_can_step(LIST3, 3, 0, -1) && car_variant_can_step(LIST3, 3, 0, 1));
    car_variant_step(LIST3, 3, 0, 1, 0);
    CHECK(car_variant_can_step(LIST3, 3, 0, -1) && !car_variant_can_step(LIST3, 3, 0, 1));
    CHECK(!car_variant_can_step(LIST3, 3, 3, 1) && !car_variant_can_step(LIST3, 3, 3, -1));   /* none offered: no triangles */
}

int main(void)
{
    test_choices();
    test_cycle();
    test_draw();
    test_paging();
    return TEST_REPORT();
}
