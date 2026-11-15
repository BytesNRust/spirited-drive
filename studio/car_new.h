/* tnfs-studio car new: a car piece (a mod of type=car, notes/dep59-content-model-design.md) made from one of the game's cars.
 *
 *   tnfs-studio car new --template CAR --id ID --stem STEM --out DIR [--name NAME] [--data DIR] [--port-data DIR] [--force]
 *
 * CAR is one of the eight race cars, by its word (viper) or its stem on the CD (DVIPER). DIR gets mod.ini (type=car, the stem, and
 * sound_from, ai_from and dash_from the template's word), the template's CFM, PDN and PBS from the data root (default "original")
 * under the new stem, and the port's masks of the template from the port's data (default "data"). Every argument is checked before
 * anything is written, and nothing is overwritten without --force. */
#ifndef TNFS_STUDIO_CAR_NEW_H
#define TNFS_STUDIO_CAR_NEW_H

#include "formats/exchange.h"

/* the race model 0..7 of `template`, a race car's word (viper) or its stem on the CD (DVIPER), any case; -1 otherwise */
int car_template_model(const char *template);

/* argv[0] is "car"; argv[1] "new", "import" (studio/car_import.h) or "card" (studio/car_card.h). 0 with `err` filled on failure. */
int car_command(int argc, char **argv, ExchangeError *err);

#endif
