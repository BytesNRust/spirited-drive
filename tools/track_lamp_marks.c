/* The first yellow layer of the track masks: the lamps of the built-in table (game/race/psx_lamps.c: CY1, CY2, CY3, TR3, TR4) drawn as YELLOW texels (255, 255, 0) in the mask of each lamp's
 * texture, data/track_masks/<TRACK>/slot_<NN>.png, so that the owner corrects the marks in the PSD rather than a table in the code (track_mask.h: a lamp is yellow, red stays reflective).
 * usage: track_lamp_marks <data dir with SIMDATA/ETRACKFM> <output dir (data/track_masks)> [track ...]
 * A mask that is already there is kept (its red and white) and the yellow laid over it; the rest is transparent. The yellow texels are those of the table's rectangle whose colour the table lets
 * stay bright (psx_lamp_exempt): the marks hold no colour of the game and no pixel of it. */
#include "formats/chunk.h"
#include "formats/png.h"
#include "game/race/psx_lamps.h"
#include "game/race/track_art.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const char *const TRACKS[] = { "CY1", "CY2", "CY3", "TR3", "TR4", NULL };

static uint8_t *slurp(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    const long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *d = n > 0 ? malloc((size_t)n) : NULL;
    if (d != NULL && fread(d, 1, (size_t)n, f) != (size_t)n) free(d), d = NULL;
    fclose(f);
    *size = d != NULL ? (size_t)n : 0;
    return d;
}

/* the mask already at `path`, as RGBA at the texture's size (nearest neighbour); transparent where there is none */
static void existing_mask(const char *path, int width, int height, uint8_t *rgba)
{
    memset(rgba, 0, (size_t)width * (size_t)height * 4);
    size_t size = 0;
    uint8_t *file = slurp(path, &size);
    PngImage img;
    if (file == NULL) return;
    if (png_read(file, size, &img) == PNG_OK) {
        for (int y = 0; y < height; y++)
            for (int x = 0; x < width; x++) {
                const size_t at = ((size_t)y * img.height / (size_t)height) * img.width + (size_t)x * img.width / (size_t)width;
                uint8_t *out = rgba + 4 * ((size_t)y * (size_t)width + (size_t)x);
                if (img.indexed) memcpy(out, img.palette + 3 * img.pixels[at], 3), out[3] = 255;
                else memcpy(out, img.pixels + 4 * at, 4);
            }
        png_free(img.pixels);
    }
    free(file);
}

static int write_file(const char *path, const uint8_t *rgba, int width, int height)
{
    uint8_t *png = NULL;
    size_t len = 0;
    int ok = 0;
    if (png_write_rgba(rgba, (uint32_t)width, (uint32_t)height, &png, &len) == PNG_OK) {
        FILE *f = fopen(path, "wb");
        if (f != NULL) ok = fwrite(png, 1, len, f) == len, ok = fclose(f) == 0 && ok;
    }
    png_free(png);
    return ok;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s <data dir> <output dir> [track ...]\n", argv[0]);
        return 2;
    }
    int failed = 0;
    for (int i = 0; argc > 3 ? i < argc - 3 : TRACKS[i] != NULL; i++) {
        const char *track = argc > 3 ? argv[3 + i] : TRACKS[i];
        const PsxLamp *lamps = NULL;
        const int count = psx_lamps_for(track, &lamps);
        char path[700];
        snprintf(path, sizeof path, "%s/SIMDATA/ETRACKFM/%s_001.FAM", argv[1], track);
        size_t size = 0;
        uint8_t *fam = slurp(path, &size);
        static TrackArt art;   /* big: not on the stack */
        Chunk root;
        if (fam == NULL || chunk_open(&root, fam, size) != CHUNK_OK || !track_art_load(&art, &root)) {
            fprintf(stderr, "%s: cannot read the art\n", path);
            failed = 1;
            free(fam);
            continue;
        }
        char dir[700];
        snprintf(dir, sizeof dir, "%s/%s", argv[2], track);
        mkdir(argv[2], 0755);
        mkdir(dir, 0755);
        for (int l = 0; l < count; l++) {
            const TrackTexture *t = &art.slots[lamps[l].slot][0];
            const int w = t->bitmap.width, h = t->bitmap.height;
            if (!t->present || w <= 0 || h <= 0 || t->bitmap.payload == NULL) continue;
            uint8_t exempt[256];
            psx_lamp_exempt(&lamps[l], t->bitmap.payload, w, h, exempt);
            snprintf(path, sizeof path, "%s/slot_%02d.png", dir, lamps[l].slot);
            uint8_t *rgba = malloc((size_t)w * (size_t)h * 4);
            if (rgba == NULL) continue;
            existing_mask(path, w, h, rgba);
            int marked = 0;
            for (int y = lamps[l].rect[1]; y <= lamps[l].rect[3] && y < h; y++)
                for (int x = lamps[l].rect[0]; x <= lamps[l].rect[2] && x < w; x++) {
                    const uint8_t v = t->bitmap.payload[y * w + x];
                    if (v >= 0xFE || !exempt[v]) continue;
                    memcpy(rgba + 4 * ((size_t)y * (size_t)w + (size_t)x), (const uint8_t[]){ 255, 255, 0, 255 }, 4);
                    marked++;
                }
            printf("%s slot %d: %d yellow texels of %d x %d -> %s %s\n", track, lamps[l].slot, marked, w, h, path, write_file(path, rgba, w, h) ? "written" : "FAILED");
            free(rgba);
        }
        free(fam);
    }
    return failed;
}
