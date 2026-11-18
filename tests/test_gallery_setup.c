/* dep59 showcase (PR C): the SETUP panel's stone from the CD's option/loadanim.qfs (game/frontend/added_gallery_setup.h). */
#include "game/frontend/added_gallery_setup.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int holes(const uint8_t *stone)
{
    int n = 0;
    for (int i = 0; i < GL_SETUP_W * GL_SETUP_H; i++) n += stone[i] == 0;
    return n;
}

static void test_the_stone_from_the_cd(void)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/original/FRONTEND/ART/OPTION/LOADANIM.QFS", TNFS_SOURCE_DIR);
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        printf("skip: no CD at %s\n", path);
        return;
    }
    static uint8_t file[1 << 20];
    const size_t size = fread(file, 1, sizeof file, f);
    fclose(f);
    uint8_t pal[768], usable[256];   /* a grey ramp: each entry its own level, 200 of them usable */
    for (int i = 0; i < 256; i++) pal[3 * i] = pal[3 * i + 1] = pal[3 * i + 2] = (uint8_t)i, usable[i] = i < 200;
    static SetupArt art;
    CHECK_EQ_INT(added_gallery_setup_art(&art, file, size, pal, usable), 1);
    CHECK_EQ_INT(art.ok, 1);
    for (int i = 0; i < GL_SETUP_W * GL_SETUP_H; i++) CHECK(art.slab[i] < 200 && art.body[i] < 200);
    /* the hole grows from frame to frame, and the first leaves most of the stone */
    for (int k = 1; k < SETUP_FRAMES; k++) CHECK(holes(art.stone[k]) >= holes(art.stone[k - 1]));
    CHECK(holes(art.stone[0]) < GL_SETUP_W * GL_SETUP_H / 2);
    CHECK(holes(art.stone[SETUP_FRAMES - 1]) > holes(art.stone[0]));
    if (getenv("GALLERY_SETUP_DUMP") != NULL) {
        for (int k = 0; k < SETUP_FRAMES; k++) {
            char name[1100];
            snprintf(name, sizeof name, "%s/stone%d.pgm", getenv("GALLERY_SETUP_DUMP"), k);
            FILE *o = fopen(name, "wb");
            if (o == NULL) continue;
            fprintf(o, "P5 %d %d 255\n", GL_SETUP_W, GL_SETUP_H);
            for (int i = 0; i < GL_SETUP_W * GL_SETUP_H; i++) fputc(art.stone[k][i] ? art.slab[i] : art.body[i], o);
            fclose(o);
        }
    }
}

/* the closed tile's picture from the Supra's bank, in a grey ramp: it is not blank and not one colour */
static void test_the_tile_from_the_cd(void)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/original/FRONTEND/SHOW/SUPRA.QFS", TNFS_SOURCE_DIR);
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        printf("skip: no CD at %s\n", path);
        return;
    }
    static uint8_t file[1 << 21];
    const size_t size = fread(file, 1, sizeof file, f);
    fclose(f);
    uint8_t pal[768], usable[256];
    for (int i = 0; i < 256; i++) pal[3 * i] = pal[3 * i + 1] = pal[3 * i + 2] = (uint8_t)i, usable[i] = i < 200;
    static uint8_t tile[GL_COL_W * GL_COL_H];
    CHECK_EQ_INT(added_gallery_setup_tile(tile, file, size, pal, usable), 1);
    int lo = 255, hi = 0;
    for (int i = 0; i < GL_COL_W * GL_COL_H; i++) {
        CHECK(tile[i] < 200);
        lo = tile[i] < lo ? tile[i] : lo, hi = tile[i] > hi ? tile[i] : hi;
    }
    CHECK(hi - lo > 60);
    if (getenv("GALLERY_SETUP_DUMP") != NULL) {
        char name[1100];
        snprintf(name, sizeof name, "%s/tile.pgm", getenv("GALLERY_SETUP_DUMP"));
        FILE *o = fopen(name, "wb");
        if (o != NULL) {
            fprintf(o, "P5 %d %d 255\n", GL_COL_W, GL_COL_H);
            fwrite(tile, 1, sizeof tile, o);
            fclose(o);
        }
    }
}

int main(void)
{
    test_the_tile_from_the_cd();
    test_the_stone_from_the_cd();
    return TEST_REPORT();
}
