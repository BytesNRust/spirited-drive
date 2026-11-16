/* port (dep59 3b): an added car's brake lamps (car_lights.h): the standard red rows of race_palette_model_light (CAR_LAMP_STANDARD, row 0
 * off and row 13 lit), whatever original its sound_from names; an original keeps its own row. */
#include "game/race/car_lights.h"
#include "game/race/car_load.h"
#include "game/race/car_registry.h"
#include "game/race/race_palette.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

enum { ENTRY = 0x47, LIT_ROWS = 13, SUPRA = 0, VIPER = 5 };

typedef struct { int32_t first, count; uint8_t rgb[3]; int writes; } Written;

static void record(void *ctx, int32_t first, int32_t count, const uint8_t *colours)
{
    Written *w = ctx;
    w->first = first, w->count = count, w->writes++;
    memcpy(w->rgb, colours, 3);
}

static int add_piece(const char *id, const char *stem, int sound)
{
    CarPiece p;
    memset(&p, 0, sizeof p);
    snprintf(p.id, sizeof p.id, "%s", id);
    snprintf(p.name, sizeof p.name, "%s", id);
    snprintf(p.stem, sizeof p.stem, "%s", stem);
    snprintf(p.short_name, sizeof p.short_name, "%s", stem);
    p.sound_model = sound, p.ai = (CarAiTuning){ CAR_AI_PACE_MIN, CAR_AI_COAST_DEFAULT }, p.dash_model = -1;
    char why[160] = "";
    return car_registry_add(&p, why, sizeof why);
}

/* the colour car_lights writes for `model` with its brake at `brake` */
static Written lights_of(int model, int32_t brake)
{
    static Car car;
    memset(&car, 0, sizeof car);
    car.model = model, car.index = 0, car.brake = brake;
    const uint8_t slots[1] = { ENTRY };
    int32_t flash_tick = 0, flash = 0;
    const CarLightInputs in = { { 0, 0 }, slots, 1, 0, &flash_tick, &flash };
    Written w = { -1, -1, { 0, 0, 0 }, 0 };
    const CarLightOps ops = { &w, record };
    car_lights(&ops, &in, &car);
    return w;
}

static void test_rows(void)
{
    const uint8_t off[3] = { 15, 3, 3 }, lit[3] = { 62, 5, 5 };
    CHECK_MEM(race_palette_model_light + 3 * CAR_LAMP_STANDARD, off, 3);
    CHECK_MEM(race_palette_model_light + 3 * (CAR_LAMP_STANDARD + LIT_ROWS), lit, 3);
}

static void test_added_standard(void)
{
    car_registry_clear();
    const int viper = add_piece("v", "VCAR", VIPER), supra = add_piece("s", "SCAR", SUPRA), grey = add_piece("g", "GCAR", 4);
    CHECK(viper >= CAR_ADDED_FIRST && supra >= CAR_ADDED_FIRST && grey >= CAR_ADDED_FIRST);
    CHECK_EQ_INT(car_registry_lamp_model(viper), CAR_LAMP_STANDARD);
    CHECK_EQ_INT(car_registry_lamp_model(supra), CAR_LAMP_STANDARD);
    CHECK_EQ_INT(car_registry_lamp_model(grey), CAR_LAMP_STANDARD);   /* row 4 is the grey (6,6,6): not taken from sound_from */
    for (int b = 0; b < 2; b++) {
        const int32_t brake = b ? 11 : 0;
        const Written v = lights_of(viper, brake), s = lights_of(supra, brake), g = lights_of(grey, brake);
        CHECK_EQ_INT(v.writes, 1);
        CHECK_EQ_INT(v.first, ENTRY);
        CHECK_MEM(v.rgb, s.rgb, 3);
        CHECK_MEM(v.rgb, g.rgb, 3);
        CHECK_MEM(v.rgb, race_palette_model_light + 3 * (CAR_LAMP_STANDARD + (b ? LIT_ROWS : 0)), 3);
    }
    car_registry_clear();
}

static void test_originals_own(void)
{
    car_registry_clear();
    for (int car = 0; car < CAR_MODELS; car++) {
        CHECK_EQ_INT(car_registry_lamp_model(car), car);
        const Written w = lights_of(car, 11);
        CHECK_MEM(w.rgb, race_palette_model_light + 3 * (car + LIT_ROWS), 3);
    }
}

int main(void)
{
    test_rows();
    test_added_standard();
    test_originals_own();
    return TEST_REPORT();
}
