/* The textures a track's reflective mask is painted over: for each of a track's road texture slots and scenery boards, the bitmap in the track's own colours as an RGBA PNG
 * (a transparent texel, colour 0xFF, is transparent in it).
 * usage: track_mask_draft <data dir with SIMDATA/ETRACKFM> <output dir> [track ...]
 * writes <output>/<TRACK>/slot_<NN>.rgb.png (the road and shoulder textures, NN the slot, variant 0) and scenery_<NN>.rgb.png (the boards: signs, posts, panels) for the
 * tracks given (all sixteen by default), from the midday FAM (<TRACK>_001.FAM). The files hold the game's pixels: keep them out of the repository. tools/track_mask_psd.py turns them into PSDs. */
#include "formats/chunk.h"
#include "formats/png.h"
#include "game/race/track_art.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const char *const TRACKS[] = { "AL1", "AL2", "AL3", "CL1", "CL2", "CL3", "CY1", "CY2", "CY3", "TR1", "TR2", "TR3", "TR4", "TR5", "TR6", "TR7", NULL };

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

static int write_png(const char *path, const TrackArt *art, const TrackTexture *tx)
{
    const int width = tx->bitmap.width, height = tx->bitmap.height;
    if (width <= 0 || height <= 0 || tx->bitmap.payload == NULL) return 0;
    const size_t count = (size_t)width * (size_t)height;
    uint8_t *rgba = malloc(count * 4);
    if (rgba == NULL) return 0;
    for (size_t i = 0; i < count; i++) {
        const uint8_t v = tx->bitmap.payload[i];
        memcpy(rgba + 4 * i, art->palette + 3 * v, 3);
        rgba[4 * i + 3] = v == 255 ? 0 : 255;
    }
    uint8_t *png = NULL;
    size_t len = 0;
    int ok = 0;
    if (png_write_rgba(rgba, (uint32_t)width, (uint32_t)height, &png, &len) == PNG_OK) {
        FILE *f = fopen(path, "wb");
        if (f != NULL) ok = fwrite(png, 1, len, f) == len, ok = fclose(f) == 0 && ok;
    }
    png_free(png);
    free(rgba);
    return ok;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s <data dir> <output dir> [track ...]\n", argv[0]);
        return 2;
    }
    int failed = 0;
    mkdir(argv[2], 0755);
    for (int i = 0; argc > 3 ? i < argc - 3 : TRACKS[i] != NULL; i++) {
        const char *track = argc > 3 ? argv[3 + i] : TRACKS[i];
        char path[700];
        snprintf(path, sizeof path, "%s/SIMDATA/ETRACKFM/%s_001.FAM", argv[1], track);
        size_t size = 0;
        uint8_t *fam = slurp(path, &size);
        static TrackArt art;   /* big: not on the stack */
        Chunk root;
        if (fam == NULL || chunk_open(&root, fam, size) != CHUNK_OK || !track_art_load(&art, &root) || !art.has_palette) {
            fprintf(stderr, "%s: cannot read the art\n", path);
            failed = 1;
            free(fam);
            continue;
        }
        char dir[700];
        snprintf(dir, sizeof dir, "%s/%s", argv[2], track);
        mkdir(dir, 0755);
        int slots = 0, boards = 0;
        for (int s = 0; s < TRACK_TEXTURE_SLOTS; s++) {
            if (!art.slots[s][0].present) continue;
            snprintf(path, sizeof path, "%s/slot_%02d.rgb.png", dir, s);
            slots += write_png(path, &art, &art.slots[s][0]);
        }
        for (int s = 0; s < TRACK_SCENERY_TEXTURES; s++) {
            if (!art.scenery[s].present) continue;
            snprintf(path, sizeof path, "%s/scenery_%02d.rgb.png", dir, s);
            boards += write_png(path, &art, &art.scenery[s]);
        }
        printf("%s: %d slots, %d boards in %s\n", track, slots, boards, dir);
        free(fam);
    }
    return failed;
}
