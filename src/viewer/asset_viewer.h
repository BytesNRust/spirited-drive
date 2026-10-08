/* Development viewer for FSH / QFS bitmaps: decodes a container and renders one
 * bitmap at its UI screen position with the container's palette. Independent
 * of the platform layer so it can render headless (screenshots, tests). */
#ifndef TNFS_ASSET_VIEWER_H
#define TNFS_ASSET_VIEWER_H

#include "formats/fsh.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t *container;   /* decompressed SHPI data, owned */
    Fsh fsh;
    uint8_t palette[768]; /* RGB, from "!pal" or the first palette entry */
    uint32_t bitmap_count;
} AssetViewer;

/* Takes a raw .FSH or .QFS file. Returns 0 and prints the reason on failure. */
int asset_viewer_load(AssetViewer *viewer, const uint8_t *file, size_t size);
void asset_viewer_free(AssetViewer *viewer);

/* Draws the n-th bitmap (0 .. bitmap_count-1) into an 8-bit framebuffer. */
int asset_viewer_render(const AssetViewer *viewer, uint32_t bitmap, uint8_t *pixels, int width, int height);

/* viewer_main.c: window (left/right to browse) or, with a screenshot path, a
 * headless PPM render of one bitmap. */
int asset_viewer_main(const char *path, uint32_t entry, const char *screenshot);

#endif
