/* port (spike): the near road without the PS1-era artefacts (modern_road.c). */
#ifndef TNFS_MODERN_ROAD_H
#define TNFS_MODERN_ROAD_H

#include "formats/fsh.h"
#include "game/gfx/quad.h"
#include "game/race/view.h"

#include <stdint.h>

/* A cell of `texture` (uv 16.16, corners in cam's order) from its four camera-space corners into q's clip box. */
void modern_road_cell(const QuadTarget *q, const View *view, const FshEntry *texture, const int32_t uv[8],
                      const int32_t *const cam[4], int transparent);

/* port (PS1 light): the same cell with a darkness at each corner (0 light .. MODERN_FOG_LEVELS - 1, in cam's order), a
 * pixel's, interpolated perspective-correct, picking between two of `tables` (MODERN_FOG_LEVELS of them, level 0 the
 * colours themselves, kept by pointer) by a 4x4 ordered dither, after the fog */
void modern_road_cell_lit(const QuadTarget *q, const View *view, const FshEntry *texture, const int32_t uv[8],
                          const int32_t *const cam[4], int transparent, const uint8_t (*tables)[256], const float dark[4]);

/* port (lamp_pool_tint): the same with a warmth (0..1) at each corner and `warm`, the tables of a lamp's full warmth (MODERN_FOG_LEVELS of them, kept by pointer): a pixel takes
 * the warm tables where its interpolated warmth and the ordered dither say, the lit ones elsewhere. warm NULL, or no warmth at any corner: modern_road_cell_lit's. */
void modern_road_cell_lit_warm(const QuadTarget *q, const View *view, const FshEntry *texture, const int32_t uv[8],
                               const int32_t *const cam[4], int transparent, const uint8_t (*tables)[256], const float dark[4],
                               const uint8_t (*warm)[256], const float heat[4]);

/* port: the 3D object engine's textured polygon (3 or 4 corners) the same way: uv 16.16 per corner, the mapper's
 * mode (a MAP_SHADE texel 0xfe takes shade[] of the pixel under it, `shade` 256 bytes then) */
void modern_polygon(const QuadTarget *q, const View *view, const FshEntry *texture, const int32_t uv[8],
                    const int32_t *const cam[4], int corners, MapMode mode, const uint8_t *shade);

/* port: a flat polygon (3 or 4 corners) of one palette colour, clipped the same way */
void modern_flat(const QuadTarget *q, const View *view, uint8_t colour, const int32_t *const cam[4], int corners);

/* port: EXTRAS' Texture Filter and Fog on the polygons above. */
enum { MODERN_FOG_LEVELS = 16 };
typedef struct {
    int filter;                     /* 1: where a texel spans more than a pixel, an ordered 2x2 sub-texel offset per
                                     * pixel (a dithered stand-in for bilinear filtering in 256 colours); 0 nearest */
    const uint8_t (*fog)[256];      /* MODERN_FOG_LEVELS tables (modern_fog_tables), level 0 each colour itself; NULL
                                     * no fog */
    float fog_start, fog_end;       /* camera depths where the fog begins and is whole (between levels a 4x4 ordered
                                     * dither of both) */
    float fog_exp, fog_cap;         /* fog_exp > 0: past fog_start the fog is fog_cap of the way (0..1) times 1 - e^-(z -
                                     * start) / fog_exp, which only approaches fog_cap, instead of the straight line to
                                     * fog_end that is whole there (0 each: the line) */
} ModernStyle;
/* The style for the draws that follow (copied; the fog tables are kept by pointer); NULL none, as Modern 3D was. */
void modern_road_set_style(const ModernStyle *style);
/* The fog's tables for a palette (768 bytes, as the art's): level l the colour nearest each colour moved l /
 * (MODERN_FOG_LEVELS - 1) of the way to rgb (the palette's scale); 0xfe and 0xff, the mappers' markers, never chosen. */
void modern_fog_tables(const uint8_t palette[768], const uint8_t rgb[3], uint8_t out[MODERN_FOG_LEVELS][256]);

/* port (PS1 light): MODERN_FOG_LEVELS tables of the palette's colours seen by light: level l the colour nearest each colour
 * times tint (per channel, 1 neutral) times (1 - l / (MODERN_FOG_LEVELS - 1)); with a neutral tint level 0 is each colour
 * itself. 0xfe and 0xff stay themselves. */
void modern_light_tables(const uint8_t palette[768], const float tint[3], uint8_t out[MODERN_FOG_LEVELS][256]);

/* The same with `cast` (per channel, 1 none) on the darkened levels 1 and up: level 0, the full light, is not cast (a tunnel's lit
 * parts stay as they are while its dim road and walls turn olive). A neutral cast is modern_light_tables. */
void modern_light_tables_cast(const uint8_t palette[768], const float tint[3], const float cast[3], uint8_t out[MODERN_FOG_LEVELS][256]);

/* The tunnel's tables: the same cast, but the tint only as its brightness (the mean of its channels, no hue; at least min_light, a
 * tunnel's own lamps keep it lit when the night's tint is darker), the cast on every level the tint darkens: a tunnel is as warm at
 * dusk or night as by day, not the red the dusk tint times the cast makes, nor the black the night's ceiling leaves. */
void modern_tunnel_tables(const uint8_t palette[768], const float tint[3], const float cast[3], float min_light, uint8_t out[MODERN_FOG_LEVELS][256]);

#endif
