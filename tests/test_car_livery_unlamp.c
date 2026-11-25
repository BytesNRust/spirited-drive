/* A car with masks, lamps by its masks (car_livery_unlamp): on the Viper's rsid texture, whose brake lights are texels of palette entry 0xfe (the oval y42-49 and a block y5-7), the texels the
 * mask does not mark as a brake light stop being entry 0xfe (the exe lights that entry by index, wherever it is), the marked ones keep it; a texture with no mask, and a pass with no masks, are
 * left as they are. Needs the CD's CARFAMS ($TNFS_DATA or original/ of the source tree); skipped when it is not there. */
#include "formats/car_livery_pass.h"
#include "formats/car_mask_codec.h"
#include "formats/chunk.h"
#include "formats/fsh.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { W = 226, H = 75, LAMP = 0xfe };

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

/* the high bank's rsid texels (a pointer into the file) */
static uint8_t *rsid(uint8_t *cfm, size_t size)
{
    Chunk file, shpi;
    Fsh bank;
    uint32_t index;
    FshEntry e;
    if (chunk_open(&file, cfm, size) != CHUNK_OK || chunk_child(&file, 1, &shpi) != CHUNK_OK || fsh_open(&bank, shpi.data, shpi.size) != FSH_OK) return NULL;
    if (fsh_find(&bank, "rsid", &index) != FSH_OK || fsh_entry(&bank, index, &e) != FSH_OK || e.width != W || e.height != H) return NULL;
    return (uint8_t *)(uintptr_t)e.payload;
}

static int g_marks;   /* 0: the brake class on a block far from the lamp texels; 1: on the oval's own place */

static uint8_t *mask_fn(void *user, const char name[4], int level, int width, int height)
{
    (void)user;
    if (level != 0 || memcmp(name, "rsid", 4) != 0 || width != W || height != H) return NULL;
    uint8_t *m = malloc((size_t)W * H);
    if (m == NULL) return NULL;
    memset(m, CAR_MASK_MATTE, (size_t)W * H);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            if (g_marks == 0 ? (x >= 100 && x < 120 && y >= 10 && y < 20) : (x >= 26 && x < 58 && y >= 41 && y < 51)) m[y * W + x] = CAR_MASK_BRAKE;
    return m;
}

static int count(const uint8_t *px, int entry)
{
    int n = 0;
    for (int i = 0; i < W * H; i++) n += px[i] == entry;
    return n;
}

int main(void)
{
    const char *root = getenv("TNFS_DATA");
    char path[600], fallback[512];
    snprintf(fallback, sizeof fallback, "%s/original", TNFS_SOURCE_DIR);
    if (root == NULL) root = fallback;
    snprintf(path, sizeof path, "%s/SIMDATA/CARFAMS/DVIPER.CFM", root);
    size_t size = 0;
    uint8_t *cfm = slurp(path, &size);
    if (cfm == NULL) {
        fprintf(stderr, "test_car_livery_unlamp: no DVIPER.CFM under %s, skipped\n", root);
        return 0;
    }
    uint8_t *orig = malloc(size);
    memcpy(orig, cfm, size);
    const int lamps = count(rsid(cfm, size), LAMP);
    CHECK_EQ_INT(lamps, 185);   /* the oval and the block: the texture's own brake lights */

    const CarLiveryPass none = { "DVIPER", 0, 0, NULL, NULL };   /* no masks: the exe's lamps */
    CHECK_EQ_INT(car_livery_unlamp(cfm, size, &none), 0);
    CHECK_MEM(cfm, orig, size);

    const CarLiveryPass with = { "DVIPER", 0, 0, mask_fn, NULL };
    g_marks = 0;   /* the oval moved away: none of the lamp texels is marked */
    CHECK_EQ_INT(car_livery_unlamp(cfm, size, &with), lamps);
    uint8_t *px = rsid(cfm, size);
    CHECK_EQ_INT(count(px, LAMP), 0);
    const uint8_t *was = rsid(orig, size);
    int others_kept = 1, replaced = -1;
    for (int i = 0; i < W * H; i++) {
        if (was[i] != LAMP) others_kept = others_kept && px[i] == was[i];
        else if (replaced < 0) replaced = px[i];
        else CHECK_EQ_INT(px[i], replaced);   /* all take the one entry */
    }
    CHECK(others_kept);   /* every other texel is as it was */
    CHECK(replaced >= 0 && replaced < 0xfa);
    /* the rest of the file: only rsid's texels changed */
    int diff = 0;
    for (size_t i = 0; i < size; i++) diff += cfm[i] != orig[i];
    CHECK(diff > 0 && diff <= lamps);

    memcpy(cfm, orig, size);
    g_marks = 1;   /* the mask marks the oval's place: the oval stays, the block (y5-7) goes */
    const int changed = car_livery_unlamp(cfm, size, &with);
    px = rsid(cfm, size);
    CHECK(changed > 0 && changed < lamps);
    int oval_kept = 1, block_gone = 1;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            if (was[y * W + x] == LAMP) {
                if (y >= 41) oval_kept = oval_kept && px[y * W + x] == LAMP;
                else block_gone = block_gone && px[y * W + x] != LAMP;
            }
    CHECK(oval_kept && block_gone);
    free(orig);
    free(cfm);
    return TEST_REPORT();
}
