/* `tnfs --view`: interactive or headless front for the asset viewer. */
#include "platform/platform.h"
#include "viewer/asset_viewer.h"

#include <stdio.h>
#include <stdlib.h>

enum { SCREEN_W = 640, SCREEN_H = 480 };

static int write_ppm(const char *path, const uint8_t *pixels, const uint8_t *rgb)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL) return 0;
    int ok = fprintf(f, "P6\n%d %d\n255\n", SCREEN_W, SCREEN_H) > 0;
    for (int i = 0; ok && i < SCREEN_W * SCREEN_H; i++) ok = fwrite(rgb + 3 * pixels[i], 1, 3, f) == 3;
    return fclose(f) == 0 && ok;
}

static void to_platform_palette(const uint8_t *rgb, PlatformColor palette[256])
{
    for (int i = 0; i < 256; i++) palette[i] = (PlatformColor){ rgb[3 * i], rgb[3 * i + 1], rgb[3 * i + 2] };
}

static int run_window(const AssetViewer *viewer, uint8_t *pixels, uint32_t entry)
{
    const PlatformConfig config = { SCREEN_W, SCREEN_H, "TNFS asset viewer" };
    if (!platform_init(&config)) return 0;
    PlatformColor palette[256];
    to_platform_palette(viewer->palette, palette);
    asset_viewer_render(viewer, entry, pixels, SCREEN_W, SCREEN_H);

    for (int running = 1; running && platform_poll_events();) {
        for (PlatformKey key; (key = platform_next_key()) != PLATFORM_KEY_NONE;) {
            if (key == PLATFORM_KEY_ESCAPE) running = 0;
            if (viewer->bitmap_count == 0) continue;
            if (key == PLATFORM_KEY_RIGHT) entry = (entry + 1) % viewer->bitmap_count;
            if (key == PLATFORM_KEY_LEFT) entry = (entry + viewer->bitmap_count - 1) % viewer->bitmap_count;
            asset_viewer_render(viewer, entry, pixels, SCREEN_W, SCREEN_H);
        }
        platform_present(pixels, palette);
        platform_wait_vblank();
    }
    platform_shutdown();
    return 1;
}

int asset_viewer_main(const char *path, uint32_t entry, const char *screenshot)
{
    size_t size = 0;
    uint8_t *file = platform_read_file(path, &size);
    AssetViewer viewer;
    const int loaded = file != NULL && asset_viewer_load(&viewer, file, size);
    platform_free_file(file);
    if (!loaded) return 0;
    printf("%s: %u entries, %u bitmaps\n", path, viewer.fsh.count, viewer.bitmap_count);

    uint8_t *pixels = malloc(SCREEN_W * SCREEN_H);
    int ok = pixels != NULL;
    if (ok && screenshot != NULL) {
        ok = asset_viewer_render(&viewer, entry, pixels, SCREEN_W, SCREEN_H) && write_ppm(screenshot, pixels, viewer.palette);
    } else if (ok) {
        ok = run_window(&viewer, pixels, entry);
    }
    free(pixels);
    asset_viewer_free(&viewer);
    return ok;
}
