/* The stem a loaded car file answers to (car_load_path's, the name of the CFM) is the name the eight cars' hand-made masks are filed under
 * (car_mask.h, data/car_masks/<stem>): the body shine's draft is made with it, so the masks drive the shine classes whether or not the
 * repaint (extras.cfg extra_liveries) runs. The session's own model table is only filled with the repaint on. */
#include "game/race/car_load.h"
#include "game/race/car_model.h"
#include "game/race/car_paint.h"
#include "game/race/race_state.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

enum { OTHER_SET = 100, HUMAN_SET = 200 };

static RaceState race;
static Car car;

static void test_a_racers_stem_is_its_masks_folder(void)
{
    for (int model = 0; model < CAR_PAINT_MODELS; model++) {
        const int sets[2] = { HUMAN_SET, OTHER_SET };
        for (int s = 0; s < 2; s++) {
            char path[CAR_LOAD_PATH], stem[16];
            memset(&race, 0, sizeof race);
            memset(&car, 0, sizeof car);
            race.models[0] = sets[s] + model;
            CHECK(car_load_path(&car, &race, "dir/", path, stem));
            if (strcmp(stem, car_model_names[model]) != 0) fprintf(stderr, "model %d set %d: stem %s, masks folder %s\n", model, sets[s], stem, car_model_names[model]);
            CHECK(strcmp(stem, car_model_names[model]) == 0);
        }
    }
}

int main(void)
{
    test_a_racers_stem_is_its_masks_folder();
    return TEST_REPORT();
}
