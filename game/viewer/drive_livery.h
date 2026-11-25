/* port: a rival's console livery at load (the mode's file extra_liveries, car_livery.h). The PS1 files are the user's own, read from
 * port.cfg's psx_dir (a folder of the CD's Z<CAR>.VSM and Z<CAR>.WSM); with the key unset or a file missing the car keeps
 * its normal colours. */
#ifndef TNFS_DRIVE_LIVERY_H
#define TNFS_DRIVE_LIVERY_H

#include "formats/car_livery_pass.h"
#include "game/race/lamp_emissive.h"

#include <stddef.h>
#include <stdint.h>

/* Remaps the car's CFM in place (the bytes the loader reads, before it loads them) to its console alternate livery: the map
 * is built from psx_dir's Z<STEM>.VSM and .WSM (STEM as the PC names the car, "P911", "TSupra") plus the car's table of
 * body palette entries (drive_livery.c), and applied to the palettes of the CFM's high and low detail banks and its
 * '!xxx' colours. The CFM buffer is the caller's, so the change is this car's alone. Returns the number of colours
 * mapped; 0 when a file is missing or there is nothing to map, and then the CFM is left as it was. */
int drive_livery_car(const char *psx_dir, const char *stem, uint8_t *cfm, size_t size, uint8_t main_rgb[3]);
/* `main_rgb` (NULL: none) gets the alternate livery's main body colour (8 bits a channel), for the dash art (drive_livery_dash). */

/* The showcase box's colours: the main body colour (BGR555) of the normal livery (`colours`[0]) and of the alternate
 * (`colours`[1]), from psx_dir's Z<STEM>.VSM and .WSM, as drive_livery_car maps them. Returns 1 with both set; 0 when a
 * file is missing or the livery maps no colour. */
int drive_livery_swatch(const char *psx_dir, const char *stem, uint16_t colours[2]);

/* A catalogue paint (car_paint.h) for the car: its body textures' colours recoloured towards `rgb`, texel by texel in the body
 * textures only (car_repaint.h), in the CFM the caller owns, before the loader reads it. Returns the body texels changed. */
int drive_livery_paint(const char *stem, uint8_t *cfm, size_t size, const uint8_t rgb[3]);
/* port (dep59 step 3): drive_livery_paint with the masks of `mask_dir` alone (car_mask_load_dir: an added car's skin's folder); NULL
 * searches as drive_livery_paint does */
int drive_livery_paint_masks(const char *stem, const char *mask_dir, uint8_t *cfm, size_t size, const uint8_t rgb[3]);
/* TEMPORARY, per car (car_livery_flat_paint): the flat polygons of a car's body that carry no texture take the paint's colour, in the CFM the caller owns, after drive_livery_paint. Returns the references changed. */
int drive_livery_flat_paint(const char *stem, uint8_t *cfm, size_t size, const uint8_t rgb[3]);

/* The texels the car's masks mark as the fixed black (car_mask.h CAR_MASK_BLACK) repainted with the car's black `rgb` (car_paint_black_of, or CAR_PAINT_ADDED_BLACK for an added
 * car), in the CFM the caller owns, before the loader reads it and before any paint or livery of the car (car_repaint_fixed_black). `mask_dir` as in drive_livery_paint_masks:
 * the masks of an added car's skin's folder alone, NULL the stock search by the stem. Returns the texels changed. */
int drive_livery_black(const char *stem, const char *mask_dir, uint8_t *cfm, size_t size, const uint8_t rgb[3]);

/* port (emissive lamps): the texels the car's masks mark as a brake, position or head light (car_mask.h), appended to `out` as offsets into the CFM, over
 * both banks and in any texture that has a mask file, not only the body's. The CFM is not changed. Returns the number of texels found. */
/* port (Restomod): a car's brake lights by its masks alone (car_livery_unlamp): the texels of palette entry 0xfe that a texture's mask does not mark as a brake light stop being lit by the exe's lamp. `mask_dir` as in
 * drive_livery_paint_masks. Returns the texels changed (0 for a car with no mask). */
int drive_livery_unlamp(const char *stem, const char *mask_dir, uint8_t *cfm, size_t size);
int drive_livery_lamps(const char *stem, uint8_t *cfm, size_t size, LampTexels *out);
/* drive_livery_lamps with the masks of `mask_dir` alone (an added car's skin's folder, as drive_livery_paint_masks); NULL searches as drive_livery_lamps does */
int drive_livery_lamps_masks(const char *stem, const char *mask_dir, uint8_t *cfm, size_t size, LampTexels *out);

/* The dash art's body (the bonnet, wings and pillars the cockpit picture shows) in a paint: `bank` is the dash's SHPI (SIMDATA/DASH/<stem>DH.FSH) as
 * read, `rgb` a catalogue paint (car_paint.h) or the console version livery's main colour (drive_livery_car's `main_rgb`; the dash shows tones the car's textures
 * do not, so the livery's map by colour would leave them grey). Texel by texel, in the entries DASH_BODY_FIRST..LAST of the 'dash' picture alone; the
 * gauges, plastics and the HUD keep theirs. Run on the bank before the palette is built. Returns the number of body texels changed. */
int drive_livery_dash(const char *stem, uint8_t *bank, size_t size, const uint8_t rgb[3]);

/* The draft of a car's masks (car_mask.h) from the rules that repaint it today, for the hand-made ones to start from: for each body
 * texture of each bank, `fn(user, level, name, width, height, pixels, keep, rgb)` with `keep` (width x height of REPAINT_DRAFT_*: paint, or the rules leave the
 * texel as it is, matte or with a shine), the texture's palette indices and the car's own colours (`rgb`, 256 x 3, 8 bits). The CFM is not
 * changed. Returns the number of texels marked paint in all. */
typedef CarLiveryDraftFn LiveryDraftFn;   /* core/formats/car_livery_pass.h */
int drive_livery_draft(const char *stem, uint8_t *cfm, size_t size, LiveryDraftFn fn, void *user);
/* drive_livery_draft with the masks of `mask_dir` alone (an added car's skin's folder: its own texture is a body texture where its mask paints); NULL as drive_livery_draft */
int drive_livery_draft_masks(const char *stem, const char *mask_dir, uint8_t *cfm, size_t size, LiveryDraftFn fn, void *user);

#endif
