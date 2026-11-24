/* Create PSD (editor/track_mask_make_psd.c): the PSD is written in C and the reader of the Studio's own PSDs (core/formats/psd_read.c) reads it back: two layers, the texture (its pixels,
 * locked) and an empty `mask` layer over it; it goes to art-source/masks, never over a PSD that is there, with nothing left behind when it cannot be made. */
#include "track_mask_make_psd.h"
#include "formats/psd_read.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

enum { W = 6, H = 5 };

static uint8_t pic[W * H * 4];

static void texture(void)
{
    for (int i = 0; i < W * H; ++i) pic[4 * i] = (uint8_t)(i * 7), pic[4 * i + 1] = (uint8_t)(i * 3 + 1), pic[4 * i + 2] = (uint8_t)(255 - i), pic[4 * i + 3] = (uint8_t)(i % 2 ? 0 : 255);
}

static uint8_t *slurp(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    *len = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc(*len + 1);
    if (fread(b, 1, *len, f) != *len) *len = 0;
    fclose(f);
    return b;
}

static void reads_back(void)
{
    size_t len = 0;
    uint8_t *psd = track_mask_psd_bytes(pic, W, H, &len);
    CHECK(psd != NULL && len > 0);
    static const char *const want[] = { "texture", "mask" };
    PsdFile f;
    char why[160] = "";
    CHECK(psd_parse(psd, len, want, 2, &f, why, sizeof why) || fprintf(stderr, "%s\n", why) == 0);
    CHECK_EQ_INT((int)f.width, W);
    CHECK_EQ_INT((int)f.height, H);
    CHECK_EQ_INT(f.count, 2);
    const PsdLayer *tex = psd_find_layer(&f, (const char *const[]){ "texture" }, 1), *mask = psd_find_layer(&f, (const char *const[]){ "mask" }, 1);
    CHECK(tex != NULL && mask != NULL);
    if (tex != NULL && tex->rgba != NULL) {
        int same = 1;
        for (int i = 0; i < W * H; ++i) same &= tex->rgba[4 * i] == pic[4 * i] && tex->rgba[4 * i + 1] == pic[4 * i + 1] && tex->rgba[4 * i + 2] == pic[4 * i + 2];
        CHECK(same);   // the texture's own colours, a transparent texel too
    } else CHECK(0);
    int painted = 0;
    if (mask != NULL && mask->rgba != NULL)
        for (int i = 0; i < W * H; ++i) painted += mask->rgba[4 * i + 3] != 0;
    CHECK_EQ_INT(painted, 0);   // an empty mask: nothing painted
    psd_free(&f);
    free(psd);
    CHECK(track_mask_psd_bytes(pic, 0, H, &len) == NULL && track_mask_psd_bytes(NULL, W, H, &len) == NULL && track_mask_psd_bytes(pic, 9000, 1, &len) == NULL);
}

static void writes_once(const char *root)
{
    char why[200] = "x", path[600];
    snprintf(path, sizeof path, "%s/art-source/masks/slot_04.psd", root);
    CHECK(track_mask_make_psd(root, "slot_04", pic, W, H, why, sizeof why) && why[0] == '\0');
    size_t a = 0;
    uint8_t *first = slurp(path, &a);
    CHECK(first != NULL && a > 100 && memcmp(first, "8BPS", 4) == 0);
    // a second time, with another picture: refused in words, the PSD as it was (it may be the owner's painted one)
    uint8_t other[W * H * 4];
    memset(other, 200, sizeof other);
    CHECK(!track_mask_make_psd(root, "slot_04", other, W, H, why, sizeof why) && strstr(why, "already") != NULL);
    size_t b = 0;
    uint8_t *again = slurp(path, &b);
    CHECK(again != NULL && a == b && memcmp(first, again, a) == 0);
    free(first), free(again);
    // nothing left behind but the PSD
    char cmd[700];
    snprintf(cmd, sizeof cmd, "test \"$(ls -A '%s/art-source/masks' | wc -l)\" -eq 1", root);
    CHECK(system(cmd) == 0);
    CHECK(!track_mask_make_psd(root, "bad/name", pic, W, H, why, sizeof why) && why[0] != '\0');
    CHECK(!track_mask_make_psd(root, "slot_05", NULL, W, H, why, sizeof why) && why[0] != '\0');
}

int main(void)
{
    char root[300];
    snprintf(root, sizeof root, "%s/tnfs_make_psd_XXXXXX", getenv("TMPDIR") != NULL ? getenv("TMPDIR") : "/tmp");
    if (mkdtemp(root) == NULL) return 1;
    texture();
    reads_back();
    writes_once(root);
    char cmd[400];
    snprintf(cmd, sizeof cmd, "find '%s' -mindepth 1 -delete && rmdir '%s'", root, root);
    if (system(cmd) != 0) return 1;
    return TEST_REPORT();
}
