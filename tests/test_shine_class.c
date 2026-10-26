/* port (body shine): the class maps of the cars' body textures, found by the texels' address (shine_class.h). */
#include "game/viewer/shine_class.h"
#include "test.h"

#include <string.h>

static uint8_t pixels_a[16], pixels_b[16], pixels_c[16];

static void test_a_registered_texture_is_found_by_its_pixels(void)
{
    shine_class_clear();
    const uint8_t keep[16] = { 0, 255, 128, 0, 0, 0, 0, 0, 255, 255, 255, 255, 0, 0, 0, 0 };
    shine_class_add(2, pixels_a, 4, 4, keep);
    const ShineClass *c = shine_class_find(pixels_a);
    CHECK(c != NULL);
    CHECK_EQ_INT(c->width, 4);
    CHECK_EQ_INT(c->height, 4);
    CHECK_MEM(c->klass, keep, 16);
    CHECK(shine_class_find(pixels_b) == NULL);
    CHECK(shine_class_find(pixels_a + 1) == NULL);   /* the texture's own first texel, nothing inside it */
}

static void test_the_class_map_is_a_copy(void)
{
    shine_class_clear();
    uint8_t keep[16] = { 0 };
    shine_class_add(0, pixels_a, 4, 4, keep);
    keep[0] = 255;
    CHECK_EQ_INT(shine_class_find(pixels_a)->klass[0], 0);
}

static void test_a_slot_can_be_dropped_and_reloaded(void)
{
    shine_class_clear();
    const uint8_t keep[16] = { 0 };
    shine_class_add(0, pixels_a, 4, 4, keep);
    shine_class_add(1, pixels_b, 4, 4, keep);
    shine_class_drop_slot(0);
    CHECK(shine_class_find(pixels_a) == NULL);
    CHECK(shine_class_find(pixels_b) != NULL);
    shine_class_add(0, pixels_c, 4, 4, keep);
    CHECK(shine_class_find(pixels_c) != NULL);
    shine_class_clear();
    CHECK(shine_class_find(pixels_b) == NULL && shine_class_find(pixels_c) == NULL);
}

static void test_bad_arguments_register_nothing(void)
{
    shine_class_clear();
    const uint8_t keep[16] = { 0 };
    shine_class_add(0, NULL, 4, 4, keep);
    shine_class_add(0, pixels_a, 0, 4, keep);
    shine_class_add(0, pixels_a, 4, 4, NULL);
    CHECK(shine_class_find(pixels_a) == NULL);
    CHECK(shine_class_find(NULL) == NULL);
}

static void test_many_textures(void)
{
    shine_class_clear();
    static uint8_t big[4096];
    const uint8_t keep[4] = { 0 };
    for (int i = 0; i < 2000; i++) shine_class_add(i % 8, big + i, 2, 2, keep);
    CHECK(shine_class_find(big + 1500) != NULL);   /* a car's textures are a few dozen; the registry does not stop at the first hundred */
    shine_class_clear();
}

int main(void)
{
    test_a_registered_texture_is_found_by_its_pixels();
    test_the_class_map_is_a_copy();
    test_a_slot_can_be_dropped_and_reloaded();
    test_bad_arguments_register_nothing();
    test_many_textures();
    return TEST_REPORT();
}
