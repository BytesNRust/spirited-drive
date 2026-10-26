/* port (body shine): the per-car state the drawing sets around a car's body (body_shine.h): the tables made for the palette in use, the
 * scroll that follows the car's travel, and nothing at all while the flag is off. */
#include "game/race/car_struct.h"
#include "viewer/body_shine.h"
#include "viewer/object_sink.h"
#include "viewer/shine_class.h"
#include "test.h"

#include <string.h>

enum { W = 64, H = 48, TEX = 8, TEXEL = 50, UNIT = 65536 };

static Car car;
static uint8_t palette_a[768], palette_b[768];

static void move_to(Car *c, int x_metres, int z_metres)
{
    c->x = x_metres * UNIT, c->y = 0, c->z = z_metres * UNIT;
}

static void test_off_sets_nothing(void)
{
    body_shine_enable(0);
    CHECK(!body_shine_enabled());
    body_shine_begin(0, &car, palette_a);
    CHECK(body_shine_tables() == NULL);
    body_shine_end();
}

static void test_on_makes_tables_for_the_palette_and_keeps_them(void)
{
    memset(palette_a, 0, sizeof palette_a), memset(palette_b, 0, sizeof palette_b);
    palette_a[3 * 10] = 170, palette_a[3 * 10 + 1] = 20, palette_a[3 * 10 + 2] = 20;
    palette_a[3 * 11] = 225, palette_a[3 * 11 + 1] = 70, palette_a[3 * 11 + 2] = 60;
    palette_b[3 * 10] = 20, palette_b[3 * 10 + 1] = 20, palette_b[3 * 10 + 2] = 170;
    body_shine_enable(1);
    move_to(&car, 0, 0);
    body_shine_begin(0, &car, palette_a);
    ShineTables first = body_shine_tables();
    CHECK(first != NULL);
    CHECK_EQ_INT(first[CAR_SHINE_NEUTRAL][10], 10);
    CHECK_EQ_INT(first[CAR_SHINE_LEVELS - 1][10], 11);   /* the lighter red of palette_a */
    body_shine_end();
    CHECK(body_shine_tables() == NULL);
    body_shine_begin(1, &car, palette_a);
    CHECK(body_shine_tables() == first);                 /* the same palette: the same tables, not made again */
    body_shine_begin(1, &car, palette_b);
    ShineTables second = body_shine_tables();
    CHECK(second != first);                              /* another palette in the frame: its own, the first still valid */
    CHECK_EQ_INT(first[CAR_SHINE_LEVELS - 1][10], 11);
    body_shine_end();
}

static void test_the_scroll_follows_the_travel_and_not_the_calls(void)
{
    body_shine_enable(1);
    move_to(&car, 100, 100);
    body_shine_begin(2, &car, palette_a);
    const float start = body_shine_phase(2);
    body_shine_begin(2, &car, palette_a);   /* drawn twice in a frame (the mirror): the car did not move */
    CHECK(body_shine_phase(2) == start);
    move_to(&car, 100, 110);
    body_shine_begin(2, &car, palette_a);
    const float after_ten = body_shine_phase(2);
    CHECK(after_ten > start);
    move_to(&car, 100, 100);   /* back the same way: the odometer counts distance, it does not unwind */
    body_shine_begin(2, &car, palette_a);
    CHECK(body_shine_phase(2) > after_ten);
    move_to(&car, 5000, 5000);   /* a teleport (a replay's jump, a reset): no whirl of the streaks */
    const float before = body_shine_phase(2);
    body_shine_begin(2, &car, palette_a);
    CHECK(body_shine_phase(2) - before < 1.0f);
    body_shine_end();
}

static void test_each_slot_has_its_own_scroll(void)
{
    body_shine_enable(1);
    move_to(&car, 0, 0);
    body_shine_begin(3, &car, palette_a);
    move_to(&car, 0, 20);
    body_shine_begin(3, &car, palette_a);
    const float moved = body_shine_phase(3);
    body_shine_begin(4, &car, palette_a);
    CHECK(body_shine_phase(4) < moved);
    body_shine_end();
}

int main(void)
{
    test_off_sets_nothing();
    test_on_makes_tables_for_the_palette_and_keeps_them();
    test_the_scroll_follows_the_travel_and_not_the_calls();
    test_each_slot_has_its_own_scroll();
    return TEST_REPORT();
}
