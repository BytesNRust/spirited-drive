/* EA's affine textured-quad mapper (NFS_WIN.EXE 00487180 and 00486950), used
 * by the race for every road, terrain and scenery polygon, plus the two
 * whole-texture wrappers the race calls for far segments and billboards.
 *
 * Screen corners are int32 pixel coordinates, (x, y) per corner. uv corners are
 * 16.16 texel coordinates, (u, v) per corner, in the order (u0,v0) (u1,v0)
 * (u1,v1) (u0,v1) the game passes, though any corner order is accepted.
 *
 * The clip rectangle is the game's: left and top inclusive, right and bottom
 * exclusive (004c78bc, 004c78c0, 004c78c4, 004c78c8). A pixel is drawn when
 * left <= x < right and top <= y < bottom. The mapper has no pixel-centre
 * offset: a polygon edge at integer coordinates covers both edge pixels, so a
 * quad from (0,0) to (3,3) fills 4 x 4 pixels.
 *
 * Only 8-bit textures (FSH type 0x7B, or 0x45 which the game flips to 0x7B
 * around the call) drawn to an 8-bit surface are ported.
 * port: 004a02d8's 16-bit and 32-bit destination surfaces (004c78cc > 8) are
 * not ported: the game always runs at 640 x 480 x 8 (00477980). Nor is 0049738c's conversion of a texture whose type
 * differs from the surface's into a temporary copy: every drawable shape in the data is 0x7B. */
#ifndef TNFS_QUAD_H
#define TNFS_QUAD_H

#include "formats/fsh.h"

#include <stdint.h>

/* How a texel reaches the surface. The object engine's third table column
 * (mode 2) is the shadow variant: colour 0xFF is skipped as in MAP_TRANSPARENT
 * and colour 0xFE is replaced by shade[destination pixel], the 256-entry
 * lookup the game keeps at 004c6604 (it darkens what is already there). */
typedef enum {
    MAP_OPAQUE = 0,
    MAP_TRANSPARENT = 1,
    MAP_SHADE = 2,
    /* port (the PS1 look's light, psx_light.h): every texel through the 256-entry table `shade` on its way to the surface
     * (a darker palette entry, say); with _TRANSPARENT colour 0xFF is skipped first, as MAP_TRANSPARENT. The exe has no
     * such mapper: the modes above are unchanged. */
    MAP_REMAP = 3,
    MAP_REMAP_TRANSPARENT = 4
} MapMode;

typedef struct {
    uint8_t *pixels;  /* 8-bit surface; row y starts at pixels + y * pitch */
    int pitch;        /* bytes per row */
    int clip_left, clip_top, clip_right, clip_bottom;
} QuadTarget;

/* The target must be valid: 0 <= left < right, 0 <= top < bottom <= 768 (the
 * game's per-scanline tables hold 768 rows), and the clip rectangle must lie
 * inside the surface. An invalid target or texture type draws nothing.
 *
 * Texels read outside the texture's pixel data read as 0; the original reads
 * whatever memory follows. */

/* Opaque (00487180) or, with transparent set, colour 0xFF skipped (00486950). */
void quad_draw(const QuadTarget *target, const FshEntry *texture, const int32_t uv[8],
               const int32_t screen[8], int transparent);

/* quad_draw storing only the rows [top, bottom): the quad's edges and its retrace are decided on the whole clip, so what
 * lands in those rows is what quad_draw paints there (a narrowed clip is not: a twisted quad retraces on a row it
 * would no longer visit). For the render bands. */
void quad_draw_rows(const QuadTarget *target, const FshEntry *texture, const int32_t uv[8], const int32_t screen[8],
                    int transparent, int32_t top, int32_t bottom);

/* The same as quad_draw, with any MapMode: 00487180, 00486950 or 004aa600.
 * shade is read only for MAP_SHADE, MAP_REMAP and MAP_REMAP_TRANSPARENT and must then point at 256 bytes. */
void quad_draw_mode(const QuadTarget *target, const FshEntry *texture, const int32_t uv[8],
                    const int32_t screen[8], MapMode mode, const uint8_t *shade);

/* The whole texture mapped onto the screen corners as one quad, uv from
 * (0,0) to ((width << 16) - 1, (height << 16) - 1): 00487a10 (opaque) and
 * 004879b0 (transparent), reached through 00425460. */
void quad_draw_full(const QuadTarget *target, const FshEntry *texture, const int32_t screen[8],
                    int transparent);

/* quad_draw_full with quad_draw_rows' window. */
void quad_draw_full_rows(const QuadTarget *target, const FshEntry *texture, const int32_t screen[8], int transparent,
                         int32_t top, int32_t bottom);

/* The whole texture inset by one texel and drawn as two triangles, with the
 * triangle mapper rather than the quad mapper: 00486720 (opaque) and
 * 004864f0 (transparent), reached through 004254a4. Corner coordinates are
 * pixel centres (see quad_tri.c). */
void quad_draw_full_tri(const QuadTarget *target, const FshEntry *texture,
                        const int32_t screen[8], int transparent);

/* One textured triangle (the sky ring's mapper, called by 004252ec once per
 * half of a quad): 00486400 (opaque) and 004858f0 (colour 0xFF transparent).
 * uv is 3 x (u, v) in whole texels, screen 3 x (x, y) in whole pixels; both
 * are taken as pixel centres (+0.5) and a pixel is drawn when its centre is
 * inside, so x runs from floor(left) up to but excluding floor(right).
 * Needs clip_right <= 32768 (see quad_tri.c). */
void tri_draw(const QuadTarget *target, const FshEntry *texture, const int32_t uv[6],
              const int32_t screen[6], int transparent);

/* The same as tri_draw, with any MapMode: 00486400, 004858f0 or 004aa510. */
void tri_draw_mode(const QuadTarget *target, const FshEntry *texture, const int32_t uv[6],
                   const int32_t screen[6], MapMode mode, const uint8_t *shade);

#endif
