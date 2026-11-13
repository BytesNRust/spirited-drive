/* The wide screen's dash, shared by drive_wide.c and what was split from it (drive_wide_dump.c, drive_wide_wipers.c). */
#ifndef TNFS_DRIVE_WIDE_PRIVATE_H
#define TNFS_DRIVE_WIDE_PRIVATE_H

#include "viewer/drive_session.h"

#include <stdint.h>

extern const uint8_t *lit_palette;   /* drive_wide_set_palette's: the frame's palette, for what is lit */
int in_gauge(const Cockpit *k, int x, int y);
int in_dial(const Cockpit *k, int x, int y);
void dash_dump(const Cockpit *k, const uint8_t *art);
const uint8_t *blades_hidden(const uint8_t *frame);
const uint8_t *nose_classed(const uint8_t *shown);
const uint8_t *dash_art_layer(const Cockpit *k);
const uint8_t *dash_mask_now(void);

#endif
