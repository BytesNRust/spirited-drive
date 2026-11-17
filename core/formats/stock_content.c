/* port (dep59 step 4): see stock_content.h. */
#include "formats/stock_content.h"

#include <string.h>

static const char *const CARS[STOCK_CAR_WARRIOR + 1] = {
    "rx7", "nsx", "supra", "911", "zr1", "viper", "512tr", "diablo", NULL, NULL, NULL, "warrior",
};

static const char *const TRACKS[STOCK_TRACK_COUNT] = {
    "city", "coastal", "alpine", "rusty", "autumn", "burnt", "vertigo", "transtropolis", "vegas", "hidden", "lunar",
};

static const char *const CAR_NAMES[STOCK_CAR_WARRIOR + 1] = {
    "Mazda RX-7", "Acura NSX", "Toyota Supra Turbo", "Porsche 911 Carrera", "Chevrolet Corvette ZR-1", "Dodge Viper RT/10", "Ferrari 512TR",
    "Lamborghini Diablo VT", NULL, NULL, NULL, "Warrior PTO E/2",
};

static const char *const TRACK_NAMES[STOCK_TRACK_COUNT] = {
    "City", "Coastal", "Alpine", "Rusty Springs", "Autumn Valley", "Burnt Sienna", "Vertigo Ridge", "Transtropolis", "Lost Vegas", NULL,
    "Lunar Springs",
};

/* the place of `id` in `table` (`n` long, NULL holes), or -1 */
static int find(const char *const *table, int n, const char *id)
{
    if (id == NULL) return -1;
    for (int i = 0; i < n; i++)
        if (table[i] != NULL && strcmp(table[i], id) == 0) return i;
    return -1;
}

const char *stock_car_id(int car)
{
    return car >= 0 && car <= STOCK_CAR_WARRIOR ? CARS[car] : NULL;
}

int stock_car_of_id(const char *id)
{
    return find(CARS, STOCK_CAR_WARRIOR + 1, id);
}

const char *stock_track_id(int track)
{
    return track >= 0 && track < STOCK_TRACK_COUNT ? TRACKS[track] : NULL;
}

int stock_track_of_id(const char *id)
{
    return find(TRACKS, STOCK_TRACK_COUNT, id);
}

const char *stock_car_name(int car)
{
    return car >= 0 && car <= STOCK_CAR_WARRIOR ? CAR_NAMES[car] : NULL;
}

const char *stock_track_name(int track)
{
    return track >= 0 && track < STOCK_TRACK_COUNT ? TRACK_NAMES[track] : NULL;
}
