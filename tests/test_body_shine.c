/* port (body shine): the per-car state the drawing sets around a car's body (body_shine.h): the tables made for the palette in use, the
 * world's up in the camera's space, and nothing at all while the flag is off. */
#include "viewer/body_shine.h"
#include "test.h"

#include <math.h>
#include <string.h>

static uint8_t palette_a[768], palette_b[768];
static const int32_t IDENTITY[9] = { 0x10000, 0, 0, 0, 0x10000, 0, 0, 0, 0x10000 };

static void test_off_sets_nothing(void)
{
    body_shine_enable(0);
    CHECK(!body_shine_enabled());
    body_shine_begin(palette_a, IDENTITY);
    CHECK(body_shine_tables() == NULL);
    body_shine_end();
}

static void test_on_makes_tables_for_the_palette_and_keeps_them(void)
{
    memset(palette_a, 0, sizeof palette_a), memset(palette_b, 0, sizeof palette_b);
    palette_a[3 * 10] = 170, palette_a[3 * 10 + 1] = 20, palette_a[3 * 10 + 2] = 20;
    palette_a[3 * 11] = 225, palette_a[3 * 11 + 1] = 70, palette_a[3 * 11 + 2] = 60;
    palette_b[3 * 10] = 20, palette_b[3 * 10 + 1] = 20, palette_b[3 * 10 + 2] = 170;
    body_shine_enable(2);
    CHECK(body_shine_enabled());
    body_shine_begin(palette_a, IDENTITY);
    ShineTables first = body_shine_tables();
    CHECK(first != NULL);
    CHECK_EQ_INT(first[CAR_SHINE_NEUTRAL][10], 10);
    CHECK_EQ_INT(first[CAR_SHINE_LEVELS - 1][10], 11);   /* the lighter red of palette_a */
    body_shine_end();
    CHECK(body_shine_tables() == NULL);
    body_shine_begin(palette_a, IDENTITY);
    CHECK(body_shine_tables() == first);                 /* the same palette: the same tables, not made again */
    body_shine_begin(palette_b, IDENTITY);
    ShineTables second = body_shine_tables();
    CHECK(second != first);                              /* another palette in the frame: its own, the first still valid */
    CHECK_EQ_INT(first[CAR_SHINE_LEVELS - 1][10], 11);
    body_shine_end();
}

/* the camera matrix takes the world's up to the camera's space (as the weather's sky does): the shine's horizon is the world's */
static void test_the_worlds_up_in_the_cameras_space(void)
{
    body_shine_enable(2);
    body_shine_begin(palette_a, IDENTITY);
    CHECK(fabsf(body_shine_up()[0]) < 0.001f && fabsf(body_shine_up()[1] - 1.0f) < 0.001f && fabsf(body_shine_up()[2]) < 0.001f);
    int32_t pitched[9];   /* a camera tipped 30 degrees: the world's up leans in its space */
    memcpy(pitched, IDENTITY, sizeof pitched);
    pitched[4] = 0xDDB4, pitched[5] = 0x8000, pitched[7] = -0x8000, pitched[8] = 0xDDB4;
    body_shine_begin(palette_a, pitched);
    const float *up = body_shine_up();
    CHECK(fabsf(up[0]) < 0.001f);
    CHECK(fabsf(up[1]) < 0.95f && fabsf(up[2]) > 0.3f);
    CHECK(fabsf(up[0] * up[0] + up[1] * up[1] + up[2] * up[2] - 1.0f) < 0.01f);
    body_shine_end();
}

/* strength 4 is the envmap: its tables (paint and glass) come with the image, and the classic tables are not made; 0..3 make no envmap */
static void test_strength_four_is_the_envmap(void)
{
    body_shine_enable(4);
    body_shine_begin(palette_a, IDENTITY);
    CHECK(body_shine_env() != NULL && body_shine_env()->image != NULL);
    CHECK(body_shine_env()->paint != NULL && body_shine_env()->glass != NULL);
    CHECK(body_shine_tables() == NULL);
    body_shine_end();
    CHECK(body_shine_env() == NULL);
    body_shine_enable(3);
    body_shine_begin(palette_a, IDENTITY);
    CHECK(body_shine_env() == NULL && body_shine_tables() != NULL);
    body_shine_end();
}

int main(void)
{
    test_off_sets_nothing();
    test_on_makes_tables_for_the_palette_and_keeps_them();
    test_the_worlds_up_in_the_cameras_space();
    test_strength_four_is_the_envmap();
    return TEST_REPORT();
}
