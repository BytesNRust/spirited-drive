/* Temporary entry point: asset viewer for the game's FSH / QFS bitmaps, used to
 * check the format code and the platform layer before game code lands.
 *
 *   tnfs FILE                                 window, left/right to browse
 *   tnfs --screenshot OUT.ppm [--entry N] FILE   headless render to a PPM */
#include "platform/platform.h"
#include "viewer/asset_viewer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { SCREEN_W = 640, SCREEN_H = 480 };

typedef struct {
    const char *file;
    const char *screenshot;
    uint32_t entry;
} Options;

static int parse_args(int argc, char **argv, Options *opt)
{
    *opt = (Options){ "original/FRONTEND/ART/TITLE.QFS", NULL, 0 };
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
            opt->screenshot = argv[++i];
        } else if (strcmp(argv[i], "--entry") == 0 && i + 1 < argc) {
            opt->entry = (uint32_t)strtoul(argv[++i], NULL, 10);
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "usage: %s [--screenshot OUT.ppm] [--entry N] FILE\n", argv[0]);
            return 0;
        } else {
            opt->file = argv[i];
        }
    }
    return 1;
}

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
    const PlatformConfig config = { SCREEN_W, SCREEN_H, "The Need for Speed SE" };
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
        platform_sleep_ms(1);
    }
    platform_shutdown();
    return 1;
}

int main(int argc, char **argv)
{
    Options opt;
    if (!parse_args(argc, argv, &opt)) return EXIT_FAILURE;

    size_t size = 0;
    uint8_t *file = platform_read_file(opt.file, &size);
    AssetViewer viewer;
    const int loaded = file != NULL && asset_viewer_load(&viewer, file, size);
    platform_free_file(file);
    if (!loaded) return EXIT_FAILURE;
    printf("%s: %u entries, %u bitmaps\n", opt.file, viewer.fsh.count, viewer.bitmap_count);

    uint8_t *pixels = malloc(SCREEN_W * SCREEN_H);
    int ok = pixels != NULL;
    if (ok && opt.screenshot != NULL) {
        ok = asset_viewer_render(&viewer, opt.entry, pixels, SCREEN_W, SCREEN_H)
            && write_ppm(opt.screenshot, pixels, viewer.palette);
    } else if (ok) {
        ok = run_window(&viewer, pixels, opt.entry);
    }
    free(pixels);
    asset_viewer_free(&viewer);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
