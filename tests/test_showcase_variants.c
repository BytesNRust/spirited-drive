/* The car showcase's Color box with the real entry table (showcase_variants.h): with the car colours switch on, every car offers its
 * catalogue paints (no PS1 files needed), stepping keeps the choice in extras.cfg's car_paint, and the box draws each paint's colour. */
#include "game/frontend/showcase_variants.h"
#include "game/race/car_paint.h"
#include "game/sys/port_extras.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

static char extras[512], port[512];
static size_t extras_size, port_size;

uint8_t *platform_read_user_file(const char *name, size_t *size)
{
    const char *src = strcmp(name, "restomod.cfg") == 0 ? extras : port;
    const size_t n = strcmp(name, "restomod.cfg") == 0 ? extras_size : port_size;
    if (n == 0) return NULL;
    uint8_t *copy = malloc(n);
    memcpy(copy, src, n);
    *size = n;
    return copy;
}

bool platform_write_user_file(const char *name, const uint8_t *data, size_t size)
{
    if (strcmp(name, "restomod.cfg") == 0) memcpy(extras, data, size), extras_size = size;
    else memcpy(port, data, size), port_size = size;
    return true;
}

uint8_t *platform_read_file(const char *path, size_t *size)
{
    (void)path;
    *size = 0;
    return NULL;
}
void platform_free_file(uint8_t *data) { free(data); }

static int32_t last_fill;
static void fill(void *ctx, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t rgb)
{
    (void)ctx, (void)x0, (void)y0, (void)x1, (void)y1;
    last_fill = rgb;
}

static void set_extras(const char *text)
{
    strcpy(extras, text);
    extras_size = strlen(text);
}

int main(void)
{
    strcpy(port, "profile=restomod\n");   /* the mode with the car colours */
    port_size = strlen(port);
    const CarVariantCanvas canvas = { NULL, fill, 0, 0, 1, 1 };
    set_extras("psx_liveries=0\n");
    CHECK_EQ_INT(car_variant_choices(showcase_variants, showcase_variant_count, 3), 0);   /* the switch off: no box */

    set_extras("psx_liveries=1\n");
    showcase_variants_reset();
    const int corvette = 3, choices = 1 + car_paint_count(corvette);
    CHECK_EQ_INT(car_variant_choices(showcase_variants, showcase_variant_count, corvette), choices);   /* its own colour + the paints */
    CHECK_EQ_INT(car_variant_current(showcase_variants, showcase_variant_count, corvette), 0);
    CHECK(car_variant_draw(showcase_variants, showcase_variant_count, corvette, &canvas));
    CHECK_EQ_INT(last_fill, car_paint_default(corvette)[0] << 16 | car_paint_default(corvette)[1] << 8 | car_paint_default(corvette)[2]);

    for (int k = 0; k < car_paint_count(corvette); k++) {
        CHECK_EQ_INT(car_variant_step(showcase_variants, showcase_variant_count, corvette, 1, 0), k + 1);
        CHECK_EQ_INT(port_extras_car_paint(corvette), PAINT_CATALOGUE + k);   /* kept as car_paint's id */
        CHECK(car_variant_draw(showcase_variants, showcase_variant_count, corvette, &canvas));
        const uint8_t *rgb = car_paint(corvette, k)->rgb;
        CHECK_EQ_INT(last_fill, rgb[0] << 16 | rgb[1] << 8 | rgb[2]);
    }
    CHECK(!car_variant_can_step(showcase_variants, showcase_variant_count, corvette, 1));   /* the last: no right triangle */
    CHECK_EQ_INT(car_variant_step(showcase_variants, showcase_variant_count, corvette, 1, 1), 0);   /* Enter wraps to its own colour */
    CHECK_EQ_INT(port_extras_car_paint(corvette), PAINT_OWN);
    CHECK_EQ_INT(car_variant_step(showcase_variants, showcase_variant_count, corvette, -1, 1), choices - 1);   /* and back round */
    for (int c = 0; c < CAR_PAINT_SLOTS; c++)
        if (c != corvette) CHECK_EQ_INT(port_extras_car_paint(c), PAINT_OWN);   /* the other cars keep theirs */

    set_extras("psx_liveries=1\ncar_paint=000000000000\n");   /* an old mask in the file: read as the PS1 choice, and no PS1 files */
    CHECK_EQ_INT(car_variant_choices(showcase_variants, showcase_variant_count, 1), 1 + car_paint_count(1));
    CHECK_EQ_INT(car_variant_choices(showcase_variants, showcase_variant_count, 11), 0);   /* a car with no table (the Warrior) */
    return TEST_REPORT();
}
