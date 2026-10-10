#include "viewer/asset_viewer.h"

#include "formats/qfs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *decode_container(const uint8_t *file, size_t size, size_t *out_size)
{
    if (!qfs_is_compressed(file, size)) {
        uint8_t *copy = malloc(size ? size : 1);
        if (copy != NULL) memcpy(copy, file, size);
        *out_size = size;
        return copy;
    }
    QfsResult r = qfs_decompressed_size(file, size, out_size);
    uint8_t *out = r == QFS_OK ? malloc(*out_size ? *out_size : 1) : NULL;
    if (out != NULL && (r = qfs_decompress(file, size, out, *out_size)) != QFS_OK) {
        free(out);
        out = NULL;
    }
    if (out == NULL) fprintf(stderr, "viewer: %s\n", qfs_result_str(r));
    return out;
}

static void load_palette(AssetViewer *viewer)
{
    uint32_t index;
    FshEntry e;
    if (fsh_find(&viewer->fsh, "!pal", &index) == FSH_OK && fsh_entry(&viewer->fsh, index, &e) == FSH_OK
        && fsh_palette_rgb(&e, viewer->palette) == FSH_OK) {
        return;
    }
    for (uint32_t i = 0; i < viewer->fsh.count; i++) {
        if (fsh_entry(&viewer->fsh, i, &e) == FSH_OK && fsh_palette_rgb(&e, viewer->palette) == FSH_OK) return;
    }
    for (int i = 0; i < 256; i++) memset(viewer->palette + 3 * i, i, 3);  /* no palette: grayscale */
}

int asset_viewer_load(AssetViewer *viewer, const uint8_t *file, size_t size)
{
    memset(viewer, 0, sizeof *viewer);
    size_t container_size;
    viewer->container = decode_container(file, size, &container_size);
    if (viewer->container == NULL) return 0;

    FshResult r = fsh_open(&viewer->fsh, viewer->container, container_size);
    if (r != FSH_OK) {
        fprintf(stderr, "viewer: %s\n", fsh_result_str(r));
        asset_viewer_free(viewer);
        return 0;
    }
    load_palette(viewer);
    for (uint32_t i = 0; i < viewer->fsh.count; i++) {
        FshEntry e;
        if (fsh_entry(&viewer->fsh, i, &e) == FSH_OK && e.type == FSH_TYPE_BITMAP) viewer->bitmap_count++;
    }
    return 1;
}

void asset_viewer_free(AssetViewer *viewer)
{
    free(viewer->container);
    viewer->container = NULL;
}

static int find_bitmap(const AssetViewer *viewer, uint32_t n, FshEntry *out)
{
    for (uint32_t i = 0; i < viewer->fsh.count; i++) {
        if (fsh_entry(&viewer->fsh, i, out) == FSH_OK && out->type == FSH_TYPE_BITMAP && n-- == 0) return 1;
    }
    return 0;
}

int asset_viewer_render(const AssetViewer *viewer, uint32_t bitmap, uint8_t *pixels, int width, int height)
{
    FshEntry e;
    memset(pixels, 0, (size_t)width * (size_t)height);
    if (!find_bitmap(viewer, bitmap, &e)) return 0;

    /* misc[2..3] is the UI screen position; images without one are centred. */
    int x0 = e.misc[2], y0 = e.misc[3];
    if (x0 < 0 || y0 < 0 || x0 + e.width > width || y0 + e.height > height) {
        x0 = (width - e.width) / 2;
        y0 = (height - e.height) / 2;
    }
    for (int y = 0; y < e.height; y++) {
        const int dy = y0 + y;
        if (dy < 0 || dy >= height) continue;
        for (int x = 0; x < e.width; x++) {
            const int dx = x0 + x;
            if (dx >= 0 && dx < width) pixels[dy * width + dx] = e.payload[(size_t)y * e.width + x];
        }
    }
    printf("%.4s %ux%u at %d,%d\n", e.name, e.width, e.height, x0, y0);
    return 1;
}
