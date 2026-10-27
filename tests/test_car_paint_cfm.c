/* The catalogue paint on the eight real CFMs (drive_livery_paint): only the body textures change, the other textures and the colours
 * they show are byte for byte as they were, the lamps are left, and the body's texels take the paint. Needs the CD's CARFAMS: from
 * $TNFS_DATA, or original/ of the source tree; skipped when it is not there. */
#include "formats/chunk.h"
#include "formats/fsh.h"
#include "game/race/car_paint.h"
#include "game/race/car_load.h"
#include "game/race/car_repaint.h"
#include "game/sys/shape_bank.h"
#include "game/viewer/drive_livery.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MAX_TEX = 32 };

/* drive_livery.c reads the PS1 files through the platform; a paint does not, so the test has none to give */
uint8_t *platform_read_file(const char *path, size_t *size)
{
    (void)path;
    *size = 0;
    return NULL;
}
void platform_free_file(uint8_t *data) { free(data); }

typedef struct {
    int level, count;
    char name[MAX_TEX][4];
    int body[MAX_TEX];     /* 0 other, 1 main, 2 end */
    uint8_t *pixels[MAX_TEX];
    size_t size[MAX_TEX];
    uint8_t *palette;      /* 768 bytes, at the car's type */
    int type;
} Snapshot;

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

/* 0 other, 1 main, 2 end: the repaint's own classes (the low detail bank's side and back are body textures there) */
static int class_of(const char *n, int level)
{
    const int c = car_repaint_body_class(n, level);
    return c == REPAINT_MAIN ? 1 : c == REPAINT_END ? 2 : 0;
}

static int snap(uint8_t *cfm, size_t size, int level, Snapshot *s)
{
    Chunk file, shpi;
    Fsh bank;
    memset(s, 0, sizeof *s);
    if (chunk_open(&file, cfm, size) != CHUNK_OK || chunk_child(&file, 2 * level + 1, &shpi) != CHUNK_OK) return 0;
    if (shpi.kind != CHUNK_KIND_BITMAP || fsh_open(&bank, shpi.data, shpi.size) != FSH_OK) return 0;
    static const uint8_t PAL[4] = { '!', 'P', 'A', 'L' };
    uint8_t *pal = shape_bank_find((uint8_t *)(uintptr_t)bank.data, PAL);
    if (pal == NULL) return 0;
    s->type = pal[0];
    s->palette = malloc(768);
    memcpy(s->palette, pal + 16, 768);
    for (uint32_t i = 0; i < bank.count && s->count < MAX_TEX; i++) {
        FshEntry e;
        if (fsh_entry(&bank, i, &e) != FSH_OK || e.type != FSH_TYPE_BITMAP || e.name[0] == '!') continue;
        size_t n = (size_t)e.width * e.height;
        if (n > e.payload_size) n = e.payload_size;
        memcpy(s->name[s->count], e.name, 4);
        s->body[s->count] = class_of(e.name, level);
        s->pixels[s->count] = malloc(n);
        memcpy(s->pixels[s->count], e.payload, n);
        s->size[s->count++] = n;
    }
    return 1;
}

static void rgb(const Snapshot *s, int entry, int out[3])
{
    const int sh = s->type == 0x22 ? 2 : 0;
    for (int c = 0; c < 3; c++) out[c] = s->palette[3 * entry + c] << sh;
}

static int spread(const int c[3])
{
    const int hi = c[0] > c[1] ? (c[0] > c[2] ? c[0] : c[2]) : (c[1] > c[2] ? c[1] : c[2]);
    const int lo = c[0] < c[1] ? (c[0] < c[2] ? c[0] : c[2]) : (c[1] < c[2] ? c[1] : c[2]);
    return hi - lo;
}

static void free_snapshot(Snapshot *s)
{
    for (int i = 0; i < s->count; i++) free(s->pixels[i]);
    free(s->palette);
}

static void check_car(const char *root, int model, const uint8_t paint[3])
{
    char path[512];
    snprintf(path, sizeof path, "%s/SIMDATA/CARFAMS/%s.CFM", root, car_model_names[model]);
    size_t size = 0;
    uint8_t *cfm = slurp(path, &size);
    if (cfm == NULL) return;
    for (int level = 0; level < 2; level++) {
        uint8_t *work = malloc(size);
        memcpy(work, cfm, size);
        Snapshot before, after;
        CHECK(snap(work, size, level, &before));
        CHECK(drive_livery_paint(car_model_names[model], work, size, paint) > 0);
        CHECK(snap(work, size, level, &after));
        CHECK_EQ_INT(before.count, after.count);
        int unused[256];   /* the entries a non-body texture shows */
        memset(unused, 0, sizeof unused);
        for (int t = 0; t < before.count; t++)
            if (before.body[t] == 0) {
                CHECK_MEM(before.pixels[t], after.pixels[t], before.size[t]);   /* the other textures are as they were */
                for (size_t i = 0; i < before.size[t]; i++) unused[before.pixels[t][i]] = 1;
            }
        for (int e = 0; e < 256; e++)
            if (unused[e]) CHECK_MEM(before.palette + 3 * e, after.palette + 3 * e, 3);   /* and the colours they show */
        uint32_t lamps = 0, lamps_kept = 0, grey_mid = 0, grey_painted = 0, reds = 0, reds_kept = 0;
        for (int t = 0; t < before.count; t++) {
            if (before.body[t] == 0) continue;
            for (size_t i = 0; i < before.size[t]; i++) {
                int a[3], b[3];
                rgb(&before, before.pixels[t][i], a);
                rgb(&after, after.pixels[t][i], b);
                const int same = a[0] == b[0] && a[1] == b[1] && a[2] == b[2];
                if (spread(a) >= 180 && before.body[t] == 2) lamps++, lamps_kept += same;   /* a bright saturated lamp colour */
                if (model == 3 && a[0] > a[1] + 24 && a[0] > a[2] + 24) reds++, reds_kept += same;   /* the ZR-1's red lamps, on a teal body */
                if (before.body[t] == 1 && spread(a) <= 24 && 30 * a[0] + 59 * a[1] + 11 * a[2] > 6400) grey_mid++, grey_painted += !same;
            }
        }
        if (lamps > 0 && model != 0 && model != 4 && model != 7 && model != 8 && model != 9) CHECK_EQ_INT(lamps_kept, lamps);   /* (the red cars' body and the MRX7's lime one are as saturated as a lamp) */
        if (model == 3) CHECK_EQ_INT(reds_kept, reds);
        if (model == 6 || model == 1) CHECK(grey_mid == 0 || grey_painted * 100 >= grey_mid * 90);   /* a grey body is all paint */
        if (level == 0) fprintf(stderr, "  %s %d,%d,%d: lamps %u kept %u, grey body texels %u painted %u\n", car_model_names[model],
                                paint[0], paint[1], paint[2], lamps, lamps_kept, grey_mid, grey_painted);
        free_snapshot(&before);
        free_snapshot(&after);
        free(work);
    }
    free(cfm);
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
    snprintf(probe, sizeof probe, "%s/SIMDATA/CARFAMS/ANSX.CFM", root);
    FILE *f = fopen(probe, "rb");
    if (f == NULL) {
        fprintf(stderr, "test_car_paint_cfm: no CARFAMS under %s, skipped\n", root);
        return 0;
    }
    fclose(f);
    for (int model = 0; model < CAR_PAINT_MODELS; model++) {
        for (int k = 0; k < car_paint_count(model); k++) check_car(root, model, car_paint(model, k)->rgb);
    }
    return TEST_REPORT();
}
