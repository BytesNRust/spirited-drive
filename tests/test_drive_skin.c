/* dep59 step 3: an added car's skin and colour in the race (game/viewer/drive_skin.h). A skin's textures go over the entries of their
 * name and size in the car file's banks (an indexed PNG as it is, an RGB one by nearest colour among 0x00..0xf9); the masks of a skin
 * are read from its own folder alone (car_mask_load_dir); and which skin, masks and paint the race uses follows the player's pick, the
 * stock skin and own colour for a pick the piece no longer has, and Color only where the skin has masks. Synthetic files only. */
#include "formats/chunk.h"
#include "formats/fsh.h"
#include "formats/png.h"
#include "formats/shpi_build.h"
#include "game/race/car_mask.h"
#include "game/sys/shape_bank.h"
#include "game/viewer/drive_skin.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char base[256];

static void put_u32(uint8_t *p, uint32_t v)
{
    for (int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i));
}

static void make_dirs(const char *path)
{
    char buf[1024];
    snprintf(buf, sizeof buf, "%s", path);
    for (char *p = buf + 1; *p; ++p)
        if (*p == '/') { *p = '\0'; mkdir(buf, 0777); *p = '/'; }
    mkdir(buf, 0777);
}

static void spill(const char *dir, const char *name, const uint8_t *d, size_t n)
{
    char path[1200];
    make_dirs(dir);
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *f = fopen(path, "wb");
    if (f != NULL) fwrite(d, 1, n, f), fclose(f);
}

static uint8_t palette[768];

/* a bank: !PAL, then each texture (4 x 2 high, 2 x 2 low), all texels `fill` */
static uint8_t *bank_of(const char (*names)[4], int count, int w, int h, uint8_t fill, size_t *size)
{
    ShpiDescEntry e[4];
    static uint8_t pixels[4][64];
    memset(e, 0, sizeof e);
    e[0] = (ShpiDescEntry){ .name = { '!', 'P', 'A', 'L' }, .type = 0x24, .width = 256, .height = 1, .data = palette,
                            .block_size = SHPI_BLOCK_AUTO, .dir_index = 0 };
    for (int i = 0; i < count; i++) {
        memset(pixels[i], fill, sizeof pixels[i]);
        e[i + 1] = (ShpiDescEntry){ .type = 0x7b, .width = (uint16_t)w, .height = (uint16_t)h, .data = pixels[i],
                                    .block_size = SHPI_BLOCK_AUTO, .dir_index = (uint32_t)(i + 1) };
        memcpy(e[i + 1].name, names[i], 4);
    }
    uint8_t *out = NULL;
    ExchangeError err;
    if (!shpi_build(e, (size_t)count + 1, &out, size, &err)) return NULL;
    return out;
}

/* a CFM: wwww [ORIP, SHPI high (topv, side 4 x 2), ORIP, SHPI low (topv 2 x 2)], every texel 7 */
static uint8_t *cfm_of(size_t *size)
{
    static const char HIGH[2][4] = { { 't', 'o', 'p', 'v' }, { 's', 'i', 'd', 'e' } }, LOW[1][4] = { { 't', 'o', 'p', 'v' } };
    size_t hn = 0, ln = 0;
    uint8_t *high = bank_of(HIGH, 2, 4, 2, 7, &hn), *low = bank_of(LOW, 1, 2, 2, 7, &ln);
    const size_t table = 8 + 4 * 4, orip = 8;
    *size = table + orip + hn + orip + ln;
    uint8_t *f = calloc(*size, 1);
    memcpy(f, "wwww", 4), put_u32(f + 4, 4);
    size_t at = table;
    put_u32(f + 8, (uint32_t)at), memcpy(f + at, "ORIP", 4), put_u32(f + at + 4, orip), at += orip;
    put_u32(f + 12, (uint32_t)at), memcpy(f + at, high, hn), at += hn;
    put_u32(f + 16, (uint32_t)at), memcpy(f + at, "ORIP", 4), put_u32(f + at + 4, orip), at += orip;
    put_u32(f + 20, (uint32_t)at), memcpy(f + at, low, ln);
    png_free(high), png_free(low);
    return f;
}

/* the texels of bank `level`'s texture `name` (NULL when there is none) */
static const uint8_t *texels(uint8_t *cfm, size_t size, int level, const char *name)
{
    Chunk file, shpi;
    Fsh bank;
    if (chunk_open(&file, cfm, size) != CHUNK_OK || chunk_child(&file, 2 * level + 1, &shpi) != CHUNK_OK) return NULL;
    if (fsh_open(&bank, shpi.data, shpi.size) != FSH_OK) return NULL;
    for (uint32_t i = 0; i < bank.count; i++) {
        FshEntry e;
        if (fsh_entry(&bank, i, &e) == FSH_OK && memcmp(e.name, name, 4) == 0) return e.payload;
    }
    return NULL;
}

static void put_indexed(const char *dir, const char *name, int w, int h, uint8_t first)
{
    uint8_t px[64], *png = NULL;
    size_t n = 0;
    for (int i = 0; i < w * h; i++) px[i] = (uint8_t)(first + i);
    if (png_write_indexed(px, (uint32_t)w, (uint32_t)h, palette, &png, &n) == PNG_OK) spill(dir, name, png, n), png_free(png);
}

static void test_skin_apply(void)
{
    char skin[400];
    snprintf(skin, sizeof skin, "%s/skins/stripes", base);
    put_indexed(skin, "topv.png", 4, 2, 1);   /* the high topv, as unpack writes it: indices taken as they are */
    put_indexed(skin, "side.png", 3, 2, 1);   /* not side's size: passed over */
    uint8_t rgba[16], *png = NULL;
    size_t n = 0;
    for (int i = 0; i < 4; i++) rgba[4 * i] = 255, rgba[4 * i + 1] = 0, rgba[4 * i + 2] = 0, rgba[4 * i + 3] = 255;   /* pure red */
    if (png_write_rgba(rgba, 2, 2, &png, &n) == PNG_OK) spill(skin, "topv_lo.png", png, n), png_free(png);

    size_t size = 0;
    uint8_t *cfm = cfm_of(&size);
    CHECK(cfm != NULL);
    CHECK_EQ_INT(drive_skin_apply(skin, cfm, size), 2);
    const uint8_t *topv = texels(cfm, size, 0, "topv"), *side = texels(cfm, size, 0, "side"), *lo = texels(cfm, size, 1, "topv");
    CHECK(topv != NULL && side != NULL && lo != NULL);
    for (int i = 0; i < 8; i++) CHECK_EQ_INT(topv[i], 1 + i);
    for (int i = 0; i < 8; i++) CHECK_EQ_INT(side[i], 7);
    for (int i = 0; i < 4; i++) CHECK_EQ_INT(lo[i], 5);   /* 0xfb is pure red too, but no texel of a skin takes 0xfa..0xff */
    CHECK_EQ_INT(drive_skin_apply("/nonexistent/skin", cfm, size), 0);
    CHECK_EQ_INT(drive_skin_apply(skin, cfm, 10), 0);   /* not a car file */
    free(cfm);
}

static void test_mask_from_its_folder(void)
{
    char dir[400], other[400];
    snprintf(dir, sizeof dir, "%s/skins/masked", base);
    snprintf(other, sizeof other, "%s/env_masks/TESTCAR", base);
    uint8_t mask[8] = { 0, 0, 255, 255, 0, 0, 255, 255 }, other_mask[8] = { 0 }, *png = NULL;
    size_t n = 0;
    if (car_mask_write(mask, 4, 2, &png, &n)) spill(dir, "topv_mask.png", png, n), png_free(png);
    if (car_mask_write(other_mask, 4, 2, &png, &n)) spill(other, "topv_mask.png", png, n), png_free(png);
    char env[400];
    snprintf(env, sizeof env, "%s/env_masks", base);
    setenv("TNFS_CAR_MASKS", env, 1);   /* the dev override car_mask_load reads first: car_mask_load_dir never looks there */
    uint8_t *m = car_mask_load_dir(dir, "topv", 0, 4, 2);
    CHECK(m != NULL && memcmp(m, mask, 8) == 0);
    free(m);
    CHECK(car_mask_load_dir(dir, "topv", 1, 2, 2) == NULL);   /* no topv_lo_mask.png: the high one is not taken */
    CHECK(car_mask_load_dir(dir, "side", 0, 4, 2) == NULL);
    unsetenv("TNFS_CAR_MASKS");
    CHECK(car_mask_load_dir(dir, "./to", 0, 4, 2) == NULL);   /* a texture name that is not letters and digits names no file */
    uint8_t *wide = calloc(3000, 1);   /* a mask past 2048 a side is not decoded */
    if (wide != NULL && car_mask_write(wide, 3000, 1, &png, &n)) spill(dir, "wide_mask.png", png, n), png_free(png);
    free(wide);
    CHECK(car_mask_load_dir(dir, "wide", 0, 4, 2) == NULL);
}

static CarPieceCard card_of(void)
{
    CarPieceCard c;
    memset(&c, 0, sizeof c);
    snprintf(c.dir, sizeof c.dir, "/mods/test-car");
    snprintf(c.info.stem, sizeof c.info.stem, "TESTCAR");
    snprintf(c.info.id, sizeof c.info.id, "test-car");
    c.skins = 2;
    snprintf(c.skin[1], sizeof c.skin[1], "stripes");
    c.skin_masked[1] = 1;
    c.info.paint_count = 2;
    c.info.paints[0] = (ModPaint){ "Red", { 0xc8, 0x10, 0x2e } };
    c.info.paints[1] = (ModPaint){ "Blue", { 0x10, 0x40, 0xc0 } };
    return c;
}

static void test_the_look(void)
{
    CarPieceCard c = card_of();
    DriveSkinLook l;
    drive_skin_choose(&c, &(CarLook){ "stripes", "Blue" }, &l);
    CHECK(strcmp(l.skin_dir, "/mods/test-car/skins/stripes") == 0 && strcmp(l.mask_dir, "/mods/test-car/skins/stripes") == 0);
    CHECK(l.paint_on && l.rgb[0] == 0x10 && l.rgb[1] == 0x40 && l.rgb[2] == 0xc0);
    drive_skin_choose(&c, &(CarLook){ "", "Blue" }, &l);   /* the stock skin has no masks here: Color is off, the colour kept for later */
    CHECK(l.skin_dir[0] == '\0' && strcmp(l.mask_dir, "/mods/test-car/car_masks/TESTCAR") == 0 && !l.paint_on);
    c.skin_masked[0] = 1;
    drive_skin_choose(&c, &(CarLook){ "", "Red" }, &l);
    CHECK(l.skin_dir[0] == '\0' && l.paint_on && l.rgb[0] == 0xc8);
    drive_skin_choose(&c, &(CarLook){ "gone", "Pink" }, &l);   /* a skin and a paint the piece no longer has: the stock skin, its own colour */
    CHECK(l.skin_dir[0] == '\0' && !l.paint_on);
    drive_skin_choose(&c, &(CarLook){ "stripes", "" }, &l);
    CHECK(strcmp(l.skin_dir, "/mods/test-car/skins/stripes") == 0 && !l.paint_on);
    drive_skin_choose(&c, NULL, &l);   /* a rival: the stock skin and its own colour */
    CHECK(l.skin_dir[0] == '\0' && !l.paint_on && strcmp(l.mask_dir, "/mods/test-car/car_masks/TESTCAR") == 0);
}

int main(void)
{
    for (int i = 0; i < 256; i++) palette[3 * i] = (uint8_t)i, palette[3 * i + 1] = (uint8_t)(i / 2), palette[3 * i + 2] = (uint8_t)(i / 3);
    palette[15] = 250, palette[16] = 0, palette[17] = 0;          /* 5: nearly red */
    palette[3 * 0xfb] = 255, palette[3 * 0xfb + 1] = 0, palette[3 * 0xfb + 2] = 0;   /* 0xfb: red itself, a colour with a meaning */
    snprintf(base, sizeof base, "%s/tnfs_drive_skin_test_%d", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp", (int)getpid());
    test_skin_apply();
    test_mask_from_its_folder();
    test_the_look();
    char cmd[400];
    snprintf(cmd, sizeof cmd, "find '%s' -delete", base);
    if (system(cmd) != 0) fprintf(stderr, "could not remove %s\n", base);
    return TEST_REPORT();
}
