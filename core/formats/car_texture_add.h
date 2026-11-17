/* dep71 follow-up 2 (notes/dep71-texture-add-plan.md): a bitmap of the car's own added to a level of a CFM (0 the high model and its
 * bank, 1 the far model's): the entry after the bank's last, its pixels as given (already fitted to the bank's palette:
 * car_texture_from_png), and a texture reference to it at the end of that level's mesh, with no face on it yet (a face takes it in
 * Blender by the material t<index>_<label>). The other entries, references and chunks are kept byte for byte. */
#ifndef TNFS_CAR_TEXTURE_ADD_H
#define TNFS_CAR_TEXTURE_ADD_H

#include "formats/exchange.h"

#include <stddef.h>
#include <stdint.h>

enum {
    CAR_TEXTURE_REFS_MAX = 256,      /* a polygon's texture is one byte */
    CAR_TEXTURE_ENTRIES_MAX = 256,   /* a bank's entries the importer reads */
    CAR_TEXTURE_PNG_MAX_SIDE = 2048  /* a picture's side read (PNG_MAX_PIXELS) */
};

/* why `name` cannot be a bitmap of the car's own, or NULL when it can: 4 characters a-z 0-9, not one the race looks up (tyr1..tyr4,
 * shad, circ, the repaint classes, J000 J001, anything starting with '!'). Taken names are the caller's to check. */
const char *car_texture_name_refused(const char *name);
/* why `label` cannot be a reference's label, or NULL: 1 to 8 of a-z 0-9 _, not one the race looks up (frntires, rertires,
 * tire1..tire4, pal) */
const char *car_texture_label_refused(const char *label);

/* `cfm` with bitmap `name` (w x h `pixels`) and a reference labelled `label` added to `level` (a new CFM, malloc'd), the
 * reference's index in `*ref`; 0 with the reason in `err`: a refused name or label, a name the bank has, a full bank or mesh, a side
 * of 0 or past CAR_TEXTURE_PNG_MAX_SIDE, no such level. */
int car_texture_add(const uint8_t *cfm, size_t len, int level, const char *name, const char *label, const uint8_t *pixels, uint16_t w,
                    uint16_t h, uint8_t **out, size_t *out_len, uint32_t *ref, ExchangeError *err);

#endif
