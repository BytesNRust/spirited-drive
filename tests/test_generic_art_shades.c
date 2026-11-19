/* The bundled generic art (editor/art/GENERIC.FAM): under a car's shadow the road lines keep their hue. The race draws a shadow by turning each pixel into
 * the palette's nearest entry to its colour darkened by 0.6 (game/sys/palette.c palette_shade_table); a palette cut for the road's pictures alone had no
 * dark yellow and the centre line went green. The bar is the CD's own AL1, whose yellow's shade is 450 away (squared, 8-bit units).
 * usage: test_generic_art_shades GENERIC.FAM */
#include "formats/chunk.h"
#include "formats/fam_build.h"
#include "formats/fam_image.h"
#include "formats/fsh.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *read_file(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    const long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *data = n > 0 ? malloc((size_t)n) : NULL;
    if (data != NULL && fread(data, 1, (size_t)n, f) != (size_t)n) free(data), data = NULL;
    fclose(f);
    *len = data != NULL ? (size_t)n : 0;
    return data;
}

/* the squared distance, 8-bit units, from the palette's nearest entry to the shade of (r, g, b); that entry's colour into out[3] */
static long shade_gap(const uint8_t *pal6, int r, int g, int b, int out[3])
{
    const int c[3] = { r >> 2, g >> 2, b >> 2 };
    int want[3];
    for (int i = 0; i < 3; i++) {
        const long product = (long)c[i] * FAM_SHADE_SCALE;
        want[i] = (int)(product >> 16) + (int)((product >> 15) & 1);
    }
    long best = -1;
    for (int j = 0; j < FAM_PIXEL_INDICES; j++) {
        long d = 0;
        for (int i = 0; i < 3; i++) d += (long)((want[i] - pal6[3 * j + i]) * 4) * ((want[i] - pal6[3 * j + i]) * 4);
        if (best < 0 || d < best) best = d, out[0] = pal6[3 * j] << 2, out[1] = pal6[3 * j + 1] << 2, out[2] = pal6[3 * j + 2] << 2;
    }
    return best;
}

int main(int argc, char **argv)
{
    if (argc < 2) return fprintf(stderr, "usage: %s GENERIC.FAM\n", argv[0]), 2;
    size_t len = 0;
    uint8_t *file = read_file(argv[1], &len);
    if (file == NULL) return printf("skip: no %s\n", argv[1]), 0;
    Chunk root, terrain, bank;
    Fsh fsh;
    uint32_t at = 0;
    FshEntry pal;
    CHECK(chunk_open(&root, file, len) == CHUNK_OK && chunk_child(&root, FAM_CHILD_TERRAIN, &terrain) == CHUNK_OK);
    CHECK(chunk_child(&terrain, 0, &bank) == CHUNK_OK);   /* the road's bank: the stage's palette */
    CHECK(fsh_open(&fsh, bank.data, bank.size) == FSH_OK && fsh_find(&fsh, "!pal", &at) == FSH_OK && fsh_entry(&fsh, at, &pal) == FSH_OK);
    CHECK(pal.payload_size >= FAM_PALETTE_BYTES);
    if (pal.payload_size >= FAM_PALETTE_BYTES) {
        int rgb[3];
        /* the generic road's lines (editor/art/make_road.py: yellow 240,190,0 and white 235,235,235), as the palette holds them */
        const long yellow = shade_gap(pal.payload, 240, 190, 0, rgb);
        CHECK(yellow <= 450);
        CHECK(rgb[0] > rgb[1] && rgb[1] > rgb[2] + 40);   /* a dark yellow, not a green */
        const long white = shade_gap(pal.payload, 235, 235, 235, rgb);
        CHECK(white <= 450);
        CHECK(abs(rgb[0] - rgb[1]) < 24 && abs(rgb[1] - rgb[2]) < 24);   /* a grey */
    }
    free(file);
    return TEST_REPORT();
}
