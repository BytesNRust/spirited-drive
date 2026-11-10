/* port: the track renderer's fixed topology tables read from NFS_WIN.EXE (the 1:1 port of tools/gen_track_tables.py): the LOD
 * by road property and distance, the LODs (vertex selection), the texture-cell rows, the polygon lists, the texture variant
 * by row distance and the subdivision depth limits. */
#include "game/exe/exe_tables.h"

#include <stdio.h>
#include <string.h>

static int32_t le32(const uint8_t *p)
{
    return (int32_t)((uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24);
}

static int fail(char *err, size_t errsz, uint32_t va, const char *what)
{
    if (err != NULL && errsz > 0) snprintf(err, errsz, "the exe has no data at %08x (%s)", va, what);
    return -1;
}

int exe_tables_track(const ExeImage *img, ExeTables *t, char *err, size_t errsz)
{
    enum { BY_DISTANCE = 0x4BBA64, LOD_BASE = 0x4C49D4, LOD_SIZE = 0x4C, UV_BASE = 0x4C3034, UV_SIZE = 0x14,
           POLY_BASE = 0x4C3098, POLY_SIZE = 11, VARIANT_NEAR = 0x4C2EA8, VARIANT_FAR = 0x4C2F20, SUBDIVIDE = 0x4B9594 };
    uint8_t b[LOD_SIZE];
    int lods = 0;
    for (int p = 0; p < TRACK_PROPERTIES; p++) {   /* a row is 32 words; the LOD it names is a byte here */
        uint8_t words[4 * TRACK_LOD_DISTANCES];
        const uint32_t va = (uint32_t)(BY_DISTANCE + p * 128);
        if (exe_va_read(img, va, words, sizeof words) != 0) return fail(err, errsz, va, "the LOD by distance");
        for (int d = 0; d < TRACK_LOD_DISTANCES; d++) {
            const int32_t v = le32(words + 4 * d);
            if (v < 0 || v > 255) {
                if (err != NULL && errsz > 0) snprintf(err, errsz, "the LOD by distance at %08x holds %d, not a byte", va + 4u * (uint32_t)d, (int)v);
                return -1;
            }
            t->track_lod_by_distance[p][d] = (uint8_t)v;
            if (v + 1 > lods) lods = v + 1;
        }
    }
    if (lods > TRACK_LOD_COUNT) {
        if (err != NULL && errsz > 0) snprintf(err, errsz, "the exe names %d LODs, the port has room for %d", lods, TRACK_LOD_COUNT);
        return -1;
    }
    int polys_end = 0;
    for (int lod = 0; lod < lods; lod++) {
        const uint32_t va = (uint32_t)(LOD_BASE + lod * LOD_SIZE);
        if (exe_va_read(img, va, b, LOD_SIZE) != 0) return fail(err, errsz, va, "the LODs");
        TrackLod *l = &t->track_lods[lod];
        l->vertex_count = le32(b), l->polygon_count = le32(b + 4);
        memcpy(l->fence, b + 8, sizeof l->fence);
        memcpy(l->select, b + 0x18, sizeof l->select);
        l->first_polygon = le32(b + 0x48);
        if (l->first_polygon + l->polygon_count > polys_end) polys_end = l->first_polygon + l->polygon_count;
    }
    if (polys_end > TRACK_POLYGON_COUNT) {
        if (err != NULL && errsz > 0) snprintf(err, errsz, "the LODs end at polygon %d, the port has room for %d", polys_end, TRACK_POLYGON_COUNT);
        return -1;
    }
    for (int i = 0; i < TRACK_UV_COUNT; i++) {
        uint8_t u[12];
        const uint32_t va = (uint32_t)(UV_BASE + UV_SIZE * i);
        if (exe_va_read(img, va, u, sizeof u) != 0) return fail(err, errsz, va, "the texture cells");
        t->track_uvs[i].u_level = le32(u), t->track_uvs[i].u_cell = le32(u + 4), t->track_uvs[i].variant = le32(u + 8);
    }
    for (int i = 0; i < polys_end; i++) {
        uint8_t e[POLY_SIZE];
        const uint32_t va = (uint32_t)(POLY_BASE + POLY_SIZE * i);
        if (exe_va_read(img, va, e, POLY_SIZE) != 0) return fail(err, errsz, va, "the polygons");
        const uint32_t ptr = (uint32_t)le32(e + 6);
        int uv = -1;
        for (int k = 0; k < TRACK_UV_COUNT; k++)
            if (ptr == (uint32_t)(UV_BASE + UV_SIZE * k)) uv = k;
        if (uv < 0) {
            if (err != NULL && errsz > 0) snprintf(err, errsz, "the polygon at %08x points at %08x, not at a texture cell", va, ptr);
            return -1;
        }
        TrackPolygon *p = &t->track_polygons[i];
        memcpy(p->corner, e, 4);
        p->strip = e[4], p->flags = e[5], p->uv = (uint8_t)uv, p->rows_done = e[10];
    }
    for (int i = polys_end; i < TRACK_POLYGON_COUNT; i++) memset(&t->track_polygons[i], 0, sizeof t->track_polygons[i]);
    if (exe_va_read(img, VARIANT_NEAR, t->track_variant_near, TRACK_VARIANT_ROWS) != 0) return fail(err, errsz, VARIANT_NEAR, "the near variants");
    if (exe_va_read(img, VARIANT_FAR, t->track_variant_far, TRACK_VARIANT_ROWS) != 0) return fail(err, errsz, VARIANT_FAR, "the far variants");
    uint8_t z[4 * TRACK_SUBDIVIDE_LEVELS];
    if (exe_va_read(img, SUBDIVIDE, z, sizeof z) != 0) return fail(err, errsz, SUBDIVIDE, "the subdivision limits");
    for (int i = 0; i < TRACK_SUBDIVIDE_LEVELS; i++) t->track_subdivide_z[i] = le32(z + 4 * i);
    return 0;
}
