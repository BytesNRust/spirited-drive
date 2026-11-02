#include "game/viewer/scenery_dump.h"

#include "game/sys/port_log.h"

#include <stdio.h>
#include <stdlib.h>

enum { CELL_W = 96, CELL_H = 144, COLUMNS = 10 };

void scenery_dump(const Track *t, const char *path)
{
    int uses[TRACK_SCENERY_TEXTURES] = { 0 }, first[TRACK_SCENERY_TEXTURES];
    for (int i = 0; i < TRACK_SCENERY_TEXTURES; i++) first[i] = -1;
    for (uint32_t i = 0; i < t->tri.scenery_count; i++) {
        TriScenery s;
        TriDescriptor d;
        if (tri_scenery(&t->tri, i, &s) != TRI_OK || tri_descriptor(&t->tri, s.descriptor, &d) != TRI_OK) continue;
        if (d.resource_id / 4 < TRACK_SCENERY_TEXTURES && uses[d.resource_id / 4]++ == 0) first[d.resource_id / 4] = s.node;
    }
    int rows = 0;
    for (int i = 0; i < TRACK_SCENERY_TEXTURES; i++) if (t->art.scenery[i].present) rows = i / COLUMNS + 1;
    const int w = COLUMNS * CELL_W, h = rows * CELL_H;
    uint8_t *img = rows > 0 ? calloc((size_t)w * h, 3) : NULL;
    if (img == NULL) return;
    for (int i = 0; i < TRACK_SCENERY_TEXTURES; i++) {
        const TrackTexture *tx = &t->art.scenery[i];
        if (!tx->present) continue;
        port_log("scenery_dump: board %2d: %3d x %3d, %4d placements (the first at node %d), type 0x%02x\n", i, tx->bitmap.width, tx->bitmap.height, uses[i], first[i], tx->bitmap.type);
        const int ox = i % COLUMNS * CELL_W, oy = i / COLUMNS * CELL_H;
        for (int y = 0; y < tx->bitmap.height && y < CELL_H; y++)
            for (int x = 0; x < tx->bitmap.width && x < CELL_W; x++) {
                const uint8_t v = tx->bitmap.payload[(size_t)y * tx->bitmap.width + x];
                uint8_t *p = img + 3 * ((size_t)(oy + y) * w + ox + x);
                if (v == 255) p[0] = 90, p[1] = 0, p[2] = 90;
                else p[0] = t->art.palette[3 * v], p[1] = t->art.palette[3 * v + 1], p[2] = t->art.palette[3 * v + 2];
            }
    }
    FILE *f = fopen(path, "wb");
    if (f != NULL) {
        fprintf(f, "P6\n%d %d\n255\n", w, h);
        fwrite(img, 3, (size_t)w * h, f);
        fclose(f);
    }
    free(img);
}

/* the track's texture slots (variant 0): cells 130 x 130, eight to a row, the id (slot) on the log lines */
void slot_dump(const Track *t, const char *path)
{
    enum { SCELL = 130, SCOLS = 8 };
    const int rows = (TRACK_TEXTURE_SLOTS + SCOLS - 1) / SCOLS, w = SCOLS * SCELL, h = rows * SCELL;
    uint8_t *img = calloc((size_t)w * h, 3);
    if (img == NULL) return;
    for (int i = 0; i < TRACK_TEXTURE_SLOTS; i++) {
        const TrackTexture *tx = &t->art.slots[i][0];
        if (!tx->present) continue;
        port_log("slot_dump: slot %2d: %3d x %3d\n", i, tx->bitmap.width, tx->bitmap.height);
        const int ox = i % SCOLS * SCELL, oy = i / SCOLS * SCELL;
        for (int y = 0; y < tx->bitmap.height && y < SCELL; y++)
            for (int x = 0; x < tx->bitmap.width && x < SCELL; x++) {
                const uint8_t v = tx->bitmap.payload[(size_t)y * tx->bitmap.width + x];
                uint8_t *p = img + 3 * ((size_t)(oy + y) * w + ox + x);
                p[0] = t->art.palette[3 * v], p[1] = t->art.palette[3 * v + 1], p[2] = t->art.palette[3 * v + 2];
            }
    }
    FILE *f = fopen(path, "wb");
    if (f != NULL) {
        fprintf(f, "P6\n%d %d\n255\n", w, h);
        fwrite(img, 3, (size_t)w * h, f);
        fclose(f);
    }
    free(img);
}
