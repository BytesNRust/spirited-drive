/* owner 2026-11-19: the MECHANICAL tile's figures (game/frontend/mech_figures.h): power= and weight= read as free text in either system, shown in the
 * player's units, and their ratio. */
#include "game/frontend/mech_figures.h"
#include "test.h"

#include <math.h>
#include <string.h>

static int near(double a, double b) { return fabs(a - b) < 0.01; }

static void weights(void)
{
    double kg;
    CHECK(mech_weight_kg("1210 kg", &kg) && near(kg, 1210));
    CHECK(mech_weight_kg("1210KG", &kg) && near(kg, 1210));          /* any case, no space */
    CHECK(mech_weight_kg("  1210  kgs ", &kg) && near(kg, 1210));
    CHECK(mech_weight_kg("1210", &kg) && near(kg, 1210));            /* no unit: kg */
    CHECK(mech_weight_kg("1,210 kg", &kg) && near(kg, 1210));        /* a comma between thousands */
    CHECK(mech_weight_kg("1210.5 kg", &kg) && near(kg, 1210.5));
    CHECK(mech_weight_kg("2668 lb", &kg) && fabs(kg - 1210.1) < 0.2);
    CHECK(mech_weight_kg("2668 LBS", &kg) && fabs(kg - 1210.1) < 0.2);
    CHECK(!mech_weight_kg("", &kg));
    CHECK(!mech_weight_kg("heavy", &kg));
    CHECK(!mech_weight_kg("0 kg", &kg));
    CHECK(!mech_weight_kg("1210 tons", &kg));                         /* a unit it does not know */
    CHECK(!mech_weight_kg("1210 kg dry", &kg));                       /* something after the unit */
    CHECK(!mech_weight_kg("about 1210 kg", &kg));
    CHECK(!mech_weight_kg("1210 hp", &kg));
}

static void powers(void)
{
    double hp;
    CHECK(mech_power_hp("231 hp", &hp) && near(hp, 231));
    CHECK(mech_power_hp("231", &hp) && near(hp, 231));               /* no unit: hp */
    CHECK(mech_power_hp("231 BHP", &hp) && near(hp, 231));
    CHECK(mech_power_hp("170 PS", &hp) && near(hp, 170 * 0.98632));
    CHECK(mech_power_hp("170 cv", &hp) && near(hp, 170 * 0.98632));
    CHECK(mech_power_hp("100 kW", &hp) && near(hp, 134.102));
    CHECK(!mech_power_hp("231 hp @ 6000 rpm", &hp));
    CHECK(!mech_power_hp("lots", &hp));
    CHECK(!mech_power_hp("", &hp));
}

static void lines(void)
{
    MechFigures f;
    mech_figures_make("231 hp", "1210 kg", 1, &f);                    /* km/h: kg and kg/hp */
    CHECK(strcmp(f.power, "231 hp") == 0 && strcmp(f.weight, "1210 kg") == 0 && strcmp(f.ratio, "5.2 kg/hp") == 0);
    mech_figures_make("231 hp", "1210 kg", 0, &f);                    /* mph: lb and lb/hp, the power stays in hp */
    CHECK(strcmp(f.power, "231 hp") == 0 && strcmp(f.weight, "2668 lb") == 0 && strcmp(f.ratio, "11.5 lb/hp") == 0);
    mech_figures_make("170 PS", "2668 lb", 1, &f);                    /* written in the other systems: converted */
    CHECK(strcmp(f.power, "168 hp") == 0 && strcmp(f.weight, "1210 kg") == 0 && strcmp(f.ratio, "7.2 kg/hp") == 0);
    mech_figures_make("231", "1500", 0, &f);                          /* no units: hp and kg */
    CHECK(strcmp(f.weight, "3307 lb") == 0 && strcmp(f.ratio, "14.3 lb/hp") == 0);
    mech_figures_make("231 hp", "heavy", 1, &f);                      /* a weight that does not read: as written, no ratio */
    CHECK(strcmp(f.power, "231 hp") == 0 && strcmp(f.weight, "heavy") == 0 && f.ratio[0] == 0);
    mech_figures_make("lots", "1210 kg", 1, &f);
    CHECK(strcmp(f.power, "lots") == 0 && strcmp(f.weight, "1210 kg") == 0 && f.ratio[0] == 0);
    mech_figures_make("", "", 1, &f);                                 /* nothing given: nothing (the page draws a dash) */
    CHECK(f.power[0] == 0 && f.weight[0] == 0 && f.ratio[0] == 0);
}

int main(void)
{
    weights();
    powers();
    lines();
    return TEST_REPORT();
}
