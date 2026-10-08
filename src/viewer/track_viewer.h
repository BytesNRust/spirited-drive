/* Development track viewer (see track_viewer.c). */
#ifndef TNFS_TRACK_VIEWER_H
#define TNFS_TRACK_VIEWER_H

#include <stdint.h>

/* name: track file stem such as "TR1" or "CY1". */
int track_viewer_main(const char *data_root, const char *name, uint32_t node, const char *screenshot);

#endif
