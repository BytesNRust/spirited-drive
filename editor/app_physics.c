/* See app.h: Physics from (dep102). */
#include "app.h"

#include "car_physics_from.h"
#include "formats/car_piece.h"

#include <stdio.h>

int app_physics_from(App *a, const char *word)
{
    char why[APP_MSG];
    int specs_rows = 0, pdn_rows = 0;
    if (!app_car_is_kit(a)) return snprintf(a->status, sizeof a->status, "Physics from: open a car kit first (a car in the current layout)"), 0;
    if (car_piece_stem_is_original(a->car.info.stem)) return snprintf(a->status, sizeof a->status, "Physics from: an original car's physics are the game's own and stay"), 0;
    if (!car_physics_from(a->export_src.cd, a->car.root, word, &specs_rows, &pdn_rows, why, sizeof why)) return snprintf(a->status, sizeof a->status, "Physics from: %s", why), 0;
    snprintf(a->status, sizeof a->status, "Physics from %s: %d rows of specs.tsv and %d of pdn.tsv copied; Build makes the car with them", word, specs_rows, pdn_rows);
    return 1;
}
