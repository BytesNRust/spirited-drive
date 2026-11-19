/* See mech_figures.h. */
#include "game/frontend/mech_figures.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    const char *unit;
    double factor;
} Unit;

static const Unit WEIGHT_UNITS[] = { { "", 1.0 }, { "kg", 1.0 }, { "kgs", 1.0 }, { "lb", 100000.0 / 220462.0 }, { "lbs", 100000.0 / 220462.0 } };
static const Unit POWER_UNITS[] = { { "", 1.0 }, { "hp", 1.0 }, { "bhp", 1.0 }, { "ps", 0.98632 }, { "cv", 0.98632 }, { "kw", 1.34102 } };

/* the number at `*at` (digits; "," only before three digits and a non-digit; one "."), the cursor past it; 0 when there is none */
static int number(const char **at, double *value)
{
    const char *p = *at;
    double v = 0.0, scale = 0.0;
    int digits = 0, dot = 0;
    for (;; p++) {
        if (isdigit((unsigned char)*p)) {
            digits++;
            v = v * 10.0 + (*p - '0');
            if (dot) scale *= 10.0;
        } else if (*p == '.' && !dot) {
            dot = 1, scale = 1.0;
        } else if (*p == ',' && !dot && digits > 0 && isdigit((unsigned char)p[1]) && isdigit((unsigned char)p[2]) && isdigit((unsigned char)p[3]) &&
                   !isdigit((unsigned char)p[4])) {
            continue;
        } else {
            break;
        }
    }
    if (digits == 0) return 0;
    *value = dot ? v / scale : v;
    *at = p;
    return 1;
}

static int read_figure(const char *text, const Unit *units, int count, double *out)
{
    const char *p = text;
    double v;
    while (*p == ' ') p++;
    if (!number(&p, &v) || v <= 0.0) return 0;
    while (*p == ' ') p++;
    char unit[8];
    int n = 0;
    for (; isalpha((unsigned char)*p) && n < (int)sizeof unit - 1; p++) unit[n++] = (char)tolower((unsigned char)*p);
    unit[n] = 0;
    if (isalpha((unsigned char)*p)) return 0;   /* a longer word than any unit */
    while (*p == ' ') p++;
    if (*p != 0) return 0;
    for (int i = 0; i < count; i++)
        if (strcmp(unit, units[i].unit) == 0) {
            *out = v * units[i].factor;
            return 1;
        }
    return 0;
}

int mech_weight_kg(const char *text, double *kg) { return read_figure(text, WEIGHT_UNITS, (int)(sizeof WEIGHT_UNITS / sizeof *WEIGHT_UNITS), kg); }
int mech_power_hp(const char *text, double *hp) { return read_figure(text, POWER_UNITS, (int)(sizeof POWER_UNITS / sizeof *POWER_UNITS), hp); }

static int rounded(double v) { return (int)(v + 0.5); }

void mech_figures_make(const char *power, const char *weight, int kmh, MechFigures *out)
{
    double hp = 0.0, kg = 0.0;
    memset(out, 0, sizeof *out);
    const int has_power = mech_power_hp(power, &hp), has_weight = mech_weight_kg(weight, &kg);
    if (has_power) snprintf(out->power, sizeof out->power, "%d hp", rounded(hp));
    else snprintf(out->power, sizeof out->power, "%s", power);
    const double lb_per_kg = (double)MECH_LB_PER_KG_NUM / MECH_LB_PER_KG_DEN;
    if (has_weight) snprintf(out->weight, sizeof out->weight, kmh ? "%d kg" : "%d lb", rounded(kmh ? kg : kg * lb_per_kg));
    else snprintf(out->weight, sizeof out->weight, "%s", weight);
    if (has_power && has_weight) snprintf(out->ratio, sizeof out->ratio, kmh ? "%.1f kg/hp" : "%.1f lb/hp", (kmh ? kg : kg * lb_per_kg) / hp);
}
