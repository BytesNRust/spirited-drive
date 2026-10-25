/* port: a rival's PS1 livery at load (extras.cfg psx_liveries, car_livery.h). The PS1 files are the user's own, read from
 * port.cfg's psx_dir (a folder of the CD's Z<CAR>.VSM and Z<CAR>.WSM); with the key unset or a file missing the car keeps
 * its normal colours. */
#ifndef TNFS_DRIVE_LIVERY_H
#define TNFS_DRIVE_LIVERY_H

#include <stddef.h>
#include <stdint.h>

/* Remaps the car's CFM in place (the bytes the loader reads, before it loads them) to its PS1 alternate livery: the map
 * is built from psx_dir's Z<STEM>.VSM and .WSM (STEM as the PC names the car, "P911", "TSupra") plus the car's table of
 * body palette entries (drive_livery.c), and applied to the palettes of the CFM's high and low detail banks and its
 * '!xxx' colours. The CFM buffer is the caller's, so the change is this car's alone. Returns the number of colours
 * mapped; 0 when a file is missing or there is nothing to map, and then the CFM is left as it was. */
int drive_livery_car(const char *psx_dir, const char *stem, uint8_t *cfm, size_t size);

/* The showcase box's colours: the main body colour (BGR555) of the normal livery (`colours`[0]) and of the alternate
 * (`colours`[1]), from psx_dir's Z<STEM>.VSM and .WSM, as drive_livery_car maps them. Returns 1 with both set; 0 when a
 * file is missing or the livery maps no colour. */
int drive_livery_swatch(const char *psx_dir, const char *stem, uint16_t colours[2]);

/* A catalogue paint (car_paint.h) for the car: its body textures' colours recoloured towards `rgb`, texel by texel in the body
 * textures only (car_repaint.h), in the CFM the caller owns, before the loader reads it. Returns the body texels changed. */
int drive_livery_paint(const char *stem, uint8_t *cfm, size_t size, const uint8_t rgb[3]);

/* The draft of a car's masks (car_mask.h) from the rules that repaint it today, for the hand-made ones to start from: for each body
 * texture of each bank, `fn(user, level, name, width, height, pixels, keep, rgb)` with `keep` (width x height of REPAINT_DRAFT_*: paint, or the rules leave the
 * texel as it is, matte or with a shine), the texture's palette indices and the car's own colours (`rgb`, 256 x 3, 8 bits). The CFM is not
 * changed. Returns the number of texels marked paint in all. */
typedef void (*LiveryDraftFn)(void *user, int level, const char name[4], int width, int height, const uint8_t *pixels, const uint8_t *keep,
                              const uint8_t rgb[768]);
int drive_livery_draft(const char *stem, uint8_t *cfm, size_t size, LiveryDraftFn fn, void *user);

#endif
