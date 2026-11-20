/* See car_brake_lamp.h. */
#include "formats/car_brake_lamp.h"

#include <ctype.h>
#include <stddef.h>
#include <string.h>

enum { ORIGINALS = 12 };

const uint8_t car_brake_lamp_rows[CAR_BRAKE_LAMP_ROWS * 3] = {
    15, 3, 3, 15, 3, 3, 15, 3, 3, 15, 3, 3, 6, 6, 6, 15, 3, 3, 15, 3, 3, 15, 3, 3, 15, 3, 3, 15, 3, 3,
    15, 3, 3, 30, 5, 5, 15, 3, 3, 62, 5, 5, 47, 5, 5, 62, 5, 5, 62, 5, 5, 62, 5, 5, 62, 5, 5, 62, 5, 5,
    62, 5, 5, 62, 5, 5, 62, 5, 5, 62, 5, 5, 62, 20, 20, 62, 20, 20
};

/* game/race/car_load.c car_model_names: the model numbers the rows follow */
static const char *const MODELS[ORIGINALS] = { "TSUPRA", "LDIABL", "P911", "CZR1", "F512TR", "DVIPER", "ANSX", "MRX7", "MRX7", "MRX7", "P911T", "TRAFFC" };

static int same_stem(const char *a, const char *b)
{
    for (; *a != '\0' && *b != '\0'; ++a, ++b)
        if (toupper((unsigned char)*a) != *b) return 0;
    return *a == *b;
}

int car_brake_lamp_row(const char *stem)
{
    for (int i = 0; stem != NULL && i < ORIGINALS; i++)
        if (same_stem(stem, MODELS[i])) return i;
    return CAR_BRAKE_LAMP_STANDARD;
}

void car_brake_lamp_colour(int row, int lit, uint8_t six[3])
{
    if (row < 0 || row >= CAR_BRAKE_LAMP_MODELS) row = CAR_BRAKE_LAMP_STANDARD;
    memcpy(six, car_brake_lamp_rows + 3 * (row + (lit ? CAR_BRAKE_LAMP_MODELS : 0)), 3);
}
