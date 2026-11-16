/* dep59 section 8: a car's power and weight from its physics record (game/frontend/car_figures.h) */
#include "game/frontend/car_figures.h"
#include "test.h"

#include <string.h>

static void put(uint8_t *r, int at, int32_t v)
{
    for (int k = 0; k < 4; k++) r[at + k] = (uint8_t)((uint32_t)v >> (8 * k));
}

/* a record: weight kg, the curve from 1000 rpm with `n` labelled points, torque t[i]; four forward gears that change up at 9000 rpm */
static void record(uint8_t *r, int kg, const int *t, int n, int points)
{
    memset(r, 0, PBS_SIZE);
    put(r, 0x008, kg << 16);
    put(r, 0x058, 4 + 3);
    for (int g = 3; g < 4 + 3; g++) put(r, 0x288 + 4 * g, 9000);
    put(r, 0x088, points);
    put(r, 0x0b4, 1000);
    for (int i = 0; i < n; i++) put(r, 0x0b8 + 8 * i, t[i]), put(r, 0x0b8 + 8 * i + 4, 1000 + 200 * (i + 1));
}

int main(void)
{
    static uint8_t r[PBS_SIZE];
    int hp = -1, kg = -1;
    static const int T[5] = { 100, 200, 300, 290, 250 };   /* 1100, 1300, 1500, 1700, 1900 rpm */
    record(r, 1269, T, 5, 51);
    CHECK(car_figures_pbs(r, &hp, &kg));
    CHECK_EQ_INT(kg, 1269);
    CHECK_EQ_INT(hp, (290 * 1700 + 7121 / 2) / 7121);   /* the peak of torque x rpm: 290 at 1700 (69), not the torque's peak */
    /* past the labelled points the record holds other fields: a huge word there is not the peak */
    put(r, 0x0b8 + 8 * 5, 100000), put(r, 0x0b8 + 8 * 5 + 4, 77);
    CHECK(car_figures_pbs(r, &hp, &kg));
    CHECK_EQ_INT(hp, 69);
    /* no more points than torque_points says */
    record(r, 1500, T, 5, 2);
    CHECK(car_figures_pbs(r, &hp, &kg));
    CHECK_EQ_INT(hp, (200 * 1300 + 3560) / 7121);
    /* half a kilogram rounds */
    record(r, 1000, T, 5, 51);
    put(r, 0x008, (1000 << 16) + 0x8000);
    CHECK(car_figures_pbs(r, &hp, &kg));
    CHECK_EQ_INT(kg, 1001);
    /* only up to the revs it changes up at: past them the curve is never reached (the Viper's runs to 11000, it changes up at 5900) */
    record(r, 1269, T, 5, 51);
    for (int g = 3; g < 7; g++) put(r, 0x288 + 4 * g, 1500);
    CHECK(car_figures_pbs(r, &hp, &kg));
    CHECK_EQ_INT(hp, (300 * 1500 + 3560) / 7121);   /* 290 at 1700 is past it */
    put(r, 0x288 + 4 * 5, 1700);                       /* the highest of the gears' */
    CHECK(car_figures_pbs(r, &hp, &kg));
    CHECK_EQ_INT(hp, 69);
    put(r, 0x058, 99);                                  /* a gear count out of range: the curve's own end */
    CHECK(car_figures_pbs(r, &hp, &kg));
    CHECK_EQ_INT(hp, 69);
    /* nothing to read */
    record(r, 1269, T, 0, 51);
    CHECK(!car_figures_pbs(r, &hp, &kg));
    record(r, 0, T, 5, 51);
    CHECK(!car_figures_pbs(r, &hp, &kg));
    record(r, 1269, T, 5, 0);
    CHECK(!car_figures_pbs(r, &hp, &kg));
    return TEST_REPORT();
}
