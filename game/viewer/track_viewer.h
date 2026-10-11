/* Development track viewer (see track_viewer.c). */
#ifndef TNFS_TRACK_VIEWER_H
#define TNFS_TRACK_VIEWER_H

#include <stdint.h>

/* name: track file stem such as "TR1" or "CY1". */
int track_viewer_main(const char *data_root, const char *name, uint32_t node, const char *screenshot);

enum { TRACK_VIEWER_RACERS = 8 };  /* the most cars a drive races (RACE_PLAYERS) */

/* Drive car `model` (car_model_names) on the track against racers - 1
 * computer cars (the other models); race_type 0..3 instead sets the cars
 * as 0042702c (type 1 on CY, CL, AL: one rival, the police and traffic),
 * -1 keeps `racers`. With a screenshot, `ticks` race steps at full
 * throttle first. */
int track_viewer_drive(const char *data_root, const char *name, int model, int racers, int race_type, uint32_t ticks,
                       const char *screenshot);

#endif
