/* dep71 follow-up (notes/dep71-texture-resize-plan.md): a car's bitmap given another size and the uvs that address it following.
 * In a CFM's level (0 the high mesh and its bank, 1 the far model's), the bank entry `name` takes w x h pixels (the given ones, or
 * its own scaled nearest-neighbour) and every uv of every face whose texture reference shows it (mesh_edit_shown: a nameless
 * tyre reference shows tyr1) is scaled by w / was_w, h / was_h, rounded to the nearest texel. A uv record a face of another bitmap
 * also uses is split: those faces keep the old record, this bitmap's faces get a scaled copy. Faces with no uvs (they map the whole
 * bitmap) need nothing. The other chunks are kept byte for byte. */
#ifndef TNFS_CAR_RESIZE_H
#define TNFS_CAR_RESIZE_H

#include "formats/exchange.h"
#include "formats/mesh_build.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint16_t was_w, was_h;
    uint32_t faces;        /* faces with uvs on the bitmap */
    uint32_t uvs_scaled;   /* uv records scaled (copies included) */
    uint32_t uvs_split;    /* of which copies made for a record shared with another bitmap's faces */
} CarResizeStats;

/* `cfm`'s level resized (a new CFM, malloc'd); 0 with the reason in `err`: no such level or bank entry, an entry that is not a
 * bitmap, a side of 0 or past SHPI_MAX_SIDE. `pixels` (w x h palette indexes) or NULL to scale the bitmap's own. */
int car_resize(const uint8_t *cfm, size_t len, int level, const char name[4], uint16_t w, uint16_t h, const uint8_t *pixels,
               uint8_t **out, size_t *out_len, CarResizeStats *stats, ExchangeError *err);

/* the mesh part alone, on a description the caller owns (its uvs may grow); 0 when memory runs out */
int car_resize_uvs(MeshDesc *mesh, const char name[4], uint16_t was_w, uint16_t was_h, uint16_t w, uint16_t h, CarResizeStats *stats,
                   ExchangeError *err);
/* `src` (sw x sh) scaled nearest-neighbour into `dst` (w x h) */
void car_resize_pixels(const uint8_t *src, uint16_t sw, uint16_t sh, uint8_t *dst, uint16_t w, uint16_t h);
/* a texel coordinate `v` of a side `was` on a side `now`, rounded to the nearest (halves away from zero) */
int32_t car_resize_scale(int32_t v, uint16_t was, uint16_t now);

#endif
