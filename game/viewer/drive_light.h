/* port: the PS1 look's light for a race (port.cfg psx_light, psx_dir): the track's Z<TRACK>B.LGT (the day), read from the
 * user's own PS1 files at the race's start (psx_light.h). Off, or with no psx_dir or no file, the race has no light and its
 * frames are the exe's. */
#ifndef TNFS_DRIVE_LIGHT_H
#define TNFS_DRIVE_LIGHT_H

#include <stddef.h>
#include <stdint.h>

/* `time` is a PSX_TIME_* (port_settings.h): the letter of the files, B for the day, A, C; the ambient colour of its .LGS tints
 * the light (a missing or unreadable LGS leaves it neutral). Loads the light of `track` ("TR1") into the renderer (ported_view_set_light), or clears it; `ring_nodes` is the TRI's node_count
 * (segment_count * 4; a stage's ring_nodes is 0x20000, a mask), which the file's must match. The previous race's light is freed. */
void drive_light_load(int enabled, int time, const char *psx_dir, const char *track, uint32_t ring_nodes);

/* The palette with every entry multiplied by the ambient colour of the light loaded (the palette itself with none), so a
 * texture keeps its variation under the tint. For what is all 3D (the track viewer); a race's frame has the HUD's pixels too, which
 * index the same entries: see below. */
void drive_light_palette(const uint8_t palette[768], uint8_t out[768]);

/* A race's frame: `frame` (count pixels) is the 3D when it is done and the 2D layer (HUD, dash, menus over it) not yet drawn. */
void drive_light_world_done(const uint8_t *frame, size_t count);
/* The pixels of `shown` (the frame now, count of them) that are the 3D's, 1 each (the 2D layer changed the others): the mask to
 * give platform_present_split with drive_light_palette's; NULL when the tint is neutral or no 3D was kept for this size. */
const uint8_t *drive_light_world_mask(const uint8_t *shown, size_t count);

#endif
