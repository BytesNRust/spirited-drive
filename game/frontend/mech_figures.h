/* port (owner 2026-11-19): the MECHANICAL tile's figures of an added car: its power= and weight= (free text: "231 hp", "1210 kg", "2668 lb", "170 PS",
 * "1500") read as numbers, shown in the player's units (kg and kg/hp with km/h, lb and lb/hp with mph; the power always in hp) and their ratio. Pure:
 * no drawing, no fonts, no files. */
#ifndef TNFS_MECH_FIGURES_H
#define TNFS_MECH_FIGURES_H

enum { MECH_FIGURE_TEXT = 48, MECH_LB_PER_KG_NUM = 220462, MECH_LB_PER_KG_DEN = 100000 };   /* 1 kg = 2.20462 lb */

/* `text` as a weight in kilograms: a number (digits, a "." decimal point, "," between thousands) and a unit, kg or lb (any case, "kgs", "lbs"), or no
 * unit and then kg; nothing else in it, and above 0. 1 and *kg when it reads, 0 when it does not */
int mech_weight_kg(const char *text, double *kg);
/* the same as a power in horsepower: hp or bhp, PS or CV (0.98632 hp) or kW (1.34102 hp), or no unit and then hp */
int mech_power_hp(const char *text, double *hp);

typedef struct {
    char power[MECH_FIGURE_TEXT];    /* "231 hp"; the author's own text when it does not read; "" when none was given */
    char weight[MECH_FIGURE_TEXT];   /* "1210 kg" or "2668 lb" (rounded to a whole number); the author's text when it does not read; "" when none */
    char ratio[MECH_FIGURE_TEXT];    /* "5.2 kg/hp" or "11.5 lb/hp" (one decimal); "" when either does not read */
} MechFigures;

/* the three lines for the power= and weight= as written, in km/h's units when `kmh` is not 0, else in mph's */
void mech_figures_make(const char *power, const char *weight, int kmh, MechFigures *out);

#endif
