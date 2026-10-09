/* Development track viewer (see track_viewer.c). */
#ifndef TNFS_TRACK_VIEWER_H
#define TNFS_TRACK_VIEWER_H

#include <stdint.h>

/* name: track file stem such as "TR1" or "CY1". */
int track_viewer_main(const char *data_root, const char *name, uint32_t node, const char *screenshot);

/* Drive car `model` (car_model_names) on the track; with a screenshot,
 * `ticks` race steps at full throttle first. */
int track_viewer_drive(const char *data_root, const char *name, int model, uint32_t ticks, const char *screenshot);

#endif
