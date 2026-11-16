/* port (dep59 step 4): see stock_content.h. */
#include "formats/stock_content.h"

#include <string.h>

static const char *const CARS[STOCK_CAR_WARRIOR + 1] = {
    "rx7", "nsx", "supra", "911", "zr1", "viper", "512tr", "diablo", NULL, NULL, NULL, "warrior",
};

static const char *const TRACKS[STOCK_TRACK_COUNT] = {
    "city", "coastal", "alpine", "rusty", "autumn", "burnt", "vertigo", "transtropolis", "vegas", "hidden",
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
