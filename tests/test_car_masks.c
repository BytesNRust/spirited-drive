/* tnfs-studio car masks (studio/car_masks.h): a synthetic kit (no game data) made into grey masks: the classes, the opacity the picture's alpha carries, the stem
 * from mod.ini, the files left alone, and nothing overwritten without force. */
#include "formats/car_mask_codec.h"
#include "formats/png.h"
#include "studio/car_masks.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char kit[256];

static void put(const char *rel, const void *data, size_t n)
{
    char path[400];
    snprintf(path, sizeof path, "%s/%s", kit, rel);
    FILE *f = fopen(path, "wb");
    if (f != NULL) fwrite(data, 1, n, f), fclose(f);
}

static uint8_t *get(const char *rel, size_t *n)
{
    char path[400];
    snprintf(path, sizeof path, "%s/%s", kit, rel);
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    *n = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *d = malloc(*n);
    if (d != NULL && fread(d, 1, *n, f) != *n) free(d), d = NULL;
    fclose(f);
    return d;
}

/* 4 x 2: the texels of a layer saved with its opacity (117) in the alpha, and an edge texel of alpha 10 */
static void make_picture(void)
{
    static const uint8_t px[8][4] = { { 0, 0, 0, 0 }, { 0, 0, 255, 117 }, { 0, 255, 0, 117 }, { 255, 0, 255, 117 },
                                      { 255, 0, 0, 117 }, { 255, 255, 255, 117 }, { 255, 255, 255, 10 }, { 20, 20, 20, 117 } };
    uint8_t *png = NULL;
    size_t n = 0;
    CHECK(png_write_rgba(&px[0][0], 4, 2, &png, &n) == PNG_OK);
    put("mascaras/topv.png", png, n);
    put("mascaras/topv_lo.png", png, n);
    png_free(png);
}

static void check_classes(const char *rel)
{
    size_t n = 0;
    uint8_t *png = get(rel, &n);
    CHECK(png != NULL);
    if (png == NULL) return;
    uint8_t mask[8];
    CHECK(car_mask_parse(png, n, 4, 2, mask));
    static const uint8_t WANT[8] = { CAR_MASK_PAINT, CAR_MASK_SHINE, CAR_MASK_BODY, CAR_MASK_BLACK, CAR_MASK_BRAKE, CAR_MASK_MATTE, CAR_MASK_PAINT, CAR_MASK_PAINT };
    CHECK(memcmp(mask, WANT, 8) == 0);
    uint32_t w = 0, h = 0;
    CHECK(png_size(png, n, &w, &h) == PNG_OK && w == 4 && h == 2);
    CHECK(png[25] == 0);   /* IHDR colour type 0: grey */
    free(png);
}

int main(void)
{
    snprintf(kit, sizeof kit, "%s/tnfs_car_masks_%d", getenv("TMPDIR") != NULL ? getenv("TMPDIR") : "/tmp", (int)getpid());
    char dir[300];
    mkdir(kit, 0777);
    snprintf(dir, sizeof dir, "%s/mascaras", kit), mkdir(dir, 0777);
    snprintf(dir, sizeof dir, "%s/coche", kit), mkdir(dir, 0777);
    make_picture();
    ExchangeError err;
    CHECK(car_masks_make(kit, NULL, 0, NULL, &err) < 0);   /* no mod.ini: no stem */
    static const char INI[] = "id=maskkit\nname=Mask kit\nversion=0.1\ntype=car\nstem=MKIT\n";
    put("coche/mod.ini", INI, strlen(INI));
    CHECK(car_masks_make(kit, "TWO", 0, NULL, &err) == 2);   /* --stem wins over the card */
    check_classes("coche/car_masks/TWO/topv_mask.png");
    check_classes("coche/car_masks/TWO/topv_lo_mask.png");
    CHECK(car_masks_make(kit, NULL, 0, NULL, &err) == 2);   /* the card's stem */
    check_classes("coche/car_masks/MKIT/topv_mask.png");
    CHECK(car_masks_make(kit, NULL, 0, NULL, &err) < 0);   /* it exists: not overwritten */
    CHECK(strstr(err.message, "--force") != NULL);
    CHECK(car_masks_make(kit, NULL, 1, NULL, &err) == 2);
    put("mascaras/bad name.png", "x", 1);   /* a name that is not a texture's */
    CHECK(car_masks_make(kit, NULL, 1, NULL, &err) < 0);
    return TEST_REPORT();
}
