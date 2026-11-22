/* A synthetic TRI for the tests that need a track's node count and no CD data: a circuit header, one model node and `segments` terrain strips (4 nodes each). Copied from
 * test_track_trace.c's template. Static inline: one test executable includes it. */
#ifndef TNFS_TESTS_FAKE_TRI_H
#define TNFS_TESTS_FAKE_TRI_H

#include "formats/tri.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline void fake_tri_u32(uint8_t *p, uint32_t v) { for (int i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8 * i)); }
static inline void fake_tri_i16(uint8_t *p, int v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }

/* writes the file `path`; 1 when it was written and opens as a TRI of `segments` segments */
static inline int fake_tri_write(const char *path, int segments)
{
    enum { DESCRIPTORS = 2, WIDTH = 40 };
    uint8_t *header = calloc(1, TRI_HEADER_SIZE), *scenery = calloc(1, (size_t)TRI_SCENERY_MAX * TRI_SCENERY_SIZE), *terrain = calloc((size_t)segments, TRI_SEGMENT_SIZE);
    uint8_t descriptors[DESCRIPTORS * TRI_DESCRIPTOR_SIZE];
    if (header == NULL || scenery == NULL || terrain == NULL) return free(header), free(scenery), free(terrain), 0;
    header[0] = TRI_VERSION;
    fake_tri_i16(header + 4, 16);
    fake_tri_u32(header + 0x16218, 1000);
    uint8_t *node = header + 0x98C + 36;
    node[0] = node[1] = WIDTH;
    node[2] = node[3] = WIDTH + 24;
    node[4] = 0x22, node[7] = 3;
    for (int i = 0; i < DESCRIPTORS * TRI_DESCRIPTOR_SIZE; ++i) descriptors[i] = (uint8_t)(i * 3);
    for (int i = 0; i < TRI_SCENERY_MAX; ++i) fake_tri_u32(scenery + 16 * i, 0xffffffffu);
    for (int s = 0; s < segments; ++s) {
        uint8_t *seg = terrain + (size_t)s * TRI_SEGMENT_SIZE;
        memcpy(seg, "TRKD", 4);
        fake_tri_u32(seg + 4, TRI_SEGMENT_SIZE - 12);
        seg[0x0d] = 0xff;
        memset(seg + 0x0e, 0x21, 10);
        for (int row = 0; row < 4; ++row) fake_tri_i16(seg + 0x18 + 66 * row + 6, WIDTH * 16);
    }
    const TriParts parts = { header, descriptors, DESCRIPTORS, scenery, terrain, (uint32_t)segments, NULL, 0 };
    size_t len = 0;
    uint8_t *file = malloc(tri_write_size(&parts));
    int ok = file != NULL && tri_write(&parts, file, tri_write_size(&parts), &len) == TRI_OK;
    FILE *f = ok ? fopen(path, "wb") : NULL;
    ok = f != NULL && fwrite(file, 1, len, f) == len;
    if (f != NULL) fclose(f);
    free(file), free(header), free(scenery), free(terrain);
    return ok;
}

/* the track's light file in the new format, a small one: the four times (night has two lamps), a tunnel at 10..20; the fake TRI's 30 segments are 120 nodes */
static inline void fake_light_write(const char *dir, const char *track)
{
    char path[1100], text[1024];
    snprintf(path, sizeof path, "%s/track_light/%s.txt", dir, track);
    snprintf(text, sizeof text, "# tnfs track light 2\ntrack %s\ntunnel 10 20\nsnow 0\n\ntime A\n  sun 150 20 55\ntime B\n  sun 150 50 55\ntime C\n  sun 210 15 55\ntime N\n  tunnel_look 90\n  lamp 30 150 ff2000 8\n  lamp 40 150\n", track);
    FILE *f = fopen(path, "wb");
    if (f == NULL) return;
    fputs(text, f);
    fclose(f);
}

#endif
