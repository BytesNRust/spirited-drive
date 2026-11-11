/* `tnfs --drive`'s cars (track_viewer_cars.c): what track_viewer.c takes from it. */
#ifndef TNFS_TRACK_VIEWER_CARS_H
#define TNFS_TRACK_VIEWER_CARS_H

#include "viewer/drive_session.h"
#include "game/race/race_replay_image.h"
#include "game/sys/port_extras.h"

#include <stddef.h>
#include <stdint.h>

extern const ReplayRace *replay_of;   /* the replay played back, its file replay_file; NULL a race */

void drive_slide(void *user, int32_t picture, int32_t car);
const CarPdn *drive_load(void *user, Car *car, RaceState *race);
int load_specs(const char *data_root, int model, CarSpecs *specs);
PortExtras race_extras(void);
void race_stand_in(void *user, int32_t player);
void car_stem(int model, char stem[16]);
int32_t le32_at(const uint8_t *p);
void slot_colours(int slot, int car);
void rival_liveries(const RaceField *field, int player, const char *track, int replay);
int paint_dash(void *user, uint8_t *bank, size_t size);

#endif
