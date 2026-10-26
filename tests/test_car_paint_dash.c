/* The catalogue paint on the eight real dash banks (drive_livery_dash): only the body texels of the 'dash' picture (entries 128..143) change
 * their colour, every other texel of it and of the bank's other pictures shows the colour it showed, and the body takes the paint. Needs the CD's
 * SIMDATA/DASH: from $TNFS_DATA, or original/ of the source tree; skipped when it is not there. */
#include "formats/fsh.h"
#include "game/race/car_load.h"
#include "game/race/car_paint.h"
#include "game/sys/shape_bank.h"
#include "game/viewer/drive_livery.h"
#include "test.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { BODY_FIRST = 128, BODY_LAST = 143, MAX_TEX = 64 };

/* drive_livery.c reads the PS1 files through the platform; a paint does not, so the test has none to give */
uint8_t *platform_read_file(const char *path, size_t *size)
{
    (void)path;
    *size = 0;
    return NULL;
}
void platform_free_file(uint8_t *data) { free(data); }

static uint8_t *slurp(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    const long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *d = malloc((size_t)n);
    if (d != NULL && fread(d, 1, (size_t)n, f) != (size_t)n) free(d), d = NULL;
    fclose(f);
    *size = d != NULL ? (size_t)n : 0;
    return d;
}

static const uint8_t *palette_of(uint8_t *bank)
{
    uint8_t *shape = shape_bank_find(bank, (const uint8_t *)"!pal");
    if (shape == NULL) {
        uint8_t *dash = shape_bank_find(bank, (const uint8_t *)"dash");
        if (dash == NULL) return NULL;
        shape = dash + ((uint32_t)dash[0] | (uint32_t)dash[1] << 8 | (uint32_t)dash[2] << 16 | (uint32_t)dash[3] << 24) / 256;
    }
    return shape;
}

/* the colour every texel of every picture shows, 3 bytes each, in the directory's order; returns the number of texels */
static size_t colours_of(uint8_t *bank, size_t size, uint8_t **out, const uint8_t **body)
{
    Fsh fsh;
    const uint8_t *pal = palette_of(bank);
    if (pal == NULL || fsh_open(&fsh, bank, size) != FSH_OK) return 0;
    const int shift = pal[0] == 0x22 ? 2 : 0;
    size_t total = 0, cap = 4 << 20;
    uint8_t *rgb = malloc(cap * 3), *is_body = malloc(cap);
    for (uint32_t k = 0; k < fsh.count; k++) {
        FshEntry e;
        if (fsh_entry(&fsh, k, &e) != FSH_OK || e.type != FSH_TYPE_BITMAP || e.name[0] == '!') continue;
        size_t n = (size_t)e.width * e.height;
        if (n > e.payload_size) n = e.payload_size;
        const int dash = memcmp(e.name, "dash", 4) == 0;
        for (size_t i = 0; i < n && total < cap; i++, total++) {
            const uint8_t v = e.payload[i];
            for (int c = 0; c < 3; c++) rgb[3 * total + c] = (uint8_t)(pal[16 + 3 * v + c] << shift);
            is_body[total] = dash && v >= BODY_FIRST && v <= BODY_LAST;
        }
    }
    *out = rgb, *body = is_body;
    return total;
}

static void check_car(const char *root, int model, const uint8_t paint[3])
{
    char path[512], stem[16];
    size_t i = 0;
    for (; car_model_names[model][i] != '\0' && i < sizeof stem - 1; i++) stem[i] = (char)toupper((unsigned char)car_model_names[model][i]);
    stem[i] = '\0';
    snprintf(path, sizeof path, "%s/SIMDATA/DASH/%sDH.FSH", root, stem);
    size_t size = 0;
    uint8_t *file = slurp(path, &size);
    if (file == NULL) return;
    uint8_t *before, *after;
    const uint8_t *body_before, *body_after;
    const size_t n = colours_of(file, size, &before, &body_before);
    CHECK(n > 0);
    const int changed = drive_livery_dash(car_model_names[model], file, size, NULL, 0, paint);
    CHECK(colours_of(file, size, &after, &body_after) == n);
    size_t body = 0, repainted = 0, others_moved = 0;
    for (size_t t = 0; t < n; t++) {
        const int same = memcmp(before + 3 * t, after + 3 * t, 3) == 0;
        if (body_before[t]) body++, repainted += !same;
        else others_moved += !same;
    }
    CHECK(others_moved == 0);   /* the gauges, plastics and the HUD's pictures show what they showed */
    if (body > 0) CHECK(repainted * 10 >= body * 9);   /* and the body takes the paint */
    else CHECK_EQ_INT(changed, 0);
    fprintf(stderr, "  %s %d,%d,%d: body texels %zu repainted %zu\n", stem, paint[0], paint[1], paint[2], body, repainted);
    free(before), free(after), free((void *)(uintptr_t)body_before), free((void *)(uintptr_t)body_after), free(file);
}

int main(void)
{
    const char *root = getenv("TNFS_DATA");
    char fallback[512];
#ifdef TNFS_SOURCE_DIR
    snprintf(fallback, sizeof fallback, "%s/original", TNFS_SOURCE_DIR);
#else
    fallback[0] = '\0';
#endif
    if (root == NULL) root = fallback;
    char probe[600];
    snprintf(probe, sizeof probe, "%s/SIMDATA/DASH/ANSXDH.FSH", root);
    FILE *f = fopen(probe, "rb");
    if (f == NULL) {
        fprintf(stderr, "test_car_paint_dash: no SIMDATA/DASH under %s, skipped\n", root);
        return 0;
    }
    fclose(f);
    for (int model = 0; model < CAR_PAINT_MODELS; model++)
        for (int k = 0; k < car_paint_count(model); k++) check_car(root, model, car_paint(model, k)->rgb);
    return TEST_REPORT();
}
