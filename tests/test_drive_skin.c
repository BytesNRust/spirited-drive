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
#include "game/race/car_paint.h"
#include "game/viewer/drive_livery.h"
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

#define CATALOGUE_SHAPED_DRAFT_HASH 756995945u   /* the hash of the draft the code made before the traffic side was a body texture (the bank of test_high_bank_side_and_back_by_their_masks): it must not move */
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

/* a CFM: wwww [ORIP, SHPI high (topv and `second`, 4 x 2), ORIP, SHPI low (topv 2 x 2)], every texel 7 */
static uint8_t *cfm_named(const char second[4], size_t *size)
{
    const char high_names[2][4] = { { 't', 'o', 'p', 'v' }, { second[0], second[1], second[2], second[3] } };
    static const char LOW[1][4] = { { 't', 'o', 'p', 'v' } };
    size_t hn = 0, ln = 0;
    uint8_t *high = bank_of(high_names, 2, 4, 2, 7, &hn), *low = bank_of(LOW, 1, 2, 2, 7, &ln);
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

static uint8_t *cfm_of(size_t *size) { return cfm_named("side", size); }

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

/* an added car's own texture (car texture add: a name the race does not paint by itself) is painted where its own mask says paint;
 * without a mask, or with a mask that paints nothing, or through the stock search (no mask folder: an original car), it is left */
static void test_added_texture_painted_by_its_mask(void)
{
    char dir[400];
    snprintf(dir, sizeof dir, "%s/skins/added", base);
    uint8_t paint_half[8] = { 0, 0, 0, 0, 255, 255, 255, 255 }, *png = NULL;
    size_t n = 0;
    if (car_mask_write(paint_half, 4, 2, &png, &n)) spill(dir, "tras_mask.png", png, n), png_free(png);
    const uint8_t red[3] = { 0xc8, 0x10, 0x2e };
    size_t size = 0;

    uint8_t *cfm = cfm_named("tras", &size);
    CHECK(drive_livery_paint_masks("TESTCAR", dir, cfm, size, red) > 0);
    const uint8_t *tras = texels(cfm, size, 0, "tras");
    CHECK(tras != NULL);
    for (int i = 0; i < 4; i++) CHECK(tras[i] != 7);    /* the mask's paint: the paint's colour */
    for (int i = 4; i < 8; i++) CHECK_EQ_INT(tras[i], 7);   /* its matte: as it was */
    free(cfm);

    cfm = cfm_named("tras", &size);   /* the stock search: no mask folder, as for an original car (the Jetta's BTM1 has a mask and stays) */
    drive_livery_paint_masks("TESTCAR", NULL, cfm, size, red);
    tras = texels(cfm, size, 0, "tras");
    for (int i = 0; i < 8; i++) CHECK_EQ_INT(tras[i], 7);
    free(cfm);

    char matte_dir[400];
    snprintf(matte_dir, sizeof matte_dir, "%s/skins/added_matte", base);
    uint8_t matte[8];
    memset(matte, 255, sizeof matte);
    if (car_mask_write(matte, 4, 2, &png, &n)) spill(matte_dir, "tras_mask.png", png, n), png_free(png);
    cfm = cfm_named("tras", &size);   /* a mask with no paint (a lamps' mask): the texture is not the body */
    drive_livery_paint_masks("TESTCAR", matte_dir, cfm, size, red);
    tras = texels(cfm, size, 0, "tras");
    for (int i = 0; i < 8; i++) CHECK_EQ_INT(tras[i], 7);
    free(cfm);
}

/* the colour (3 bytes, as the bank holds them) of palette entry `index` of bank `level` */
static const uint8_t *pal_colour(uint8_t *cfm, size_t size, int level, int index)
{
    Chunk file, shpi;
    if (chunk_open(&file, cfm, size) != CHUNK_OK || chunk_child(&file, 2 * level + 1, &shpi) != CHUNK_OK) return NULL;
    static const uint8_t PAL[4] = { '!', 'P', 'A', 'L' };
    const uint8_t *shape = shape_bank_find((uint8_t *)(uintptr_t)shpi.data, PAL);
    return shape == NULL ? NULL : shape + 0x10 + 3 * index;
}

/* an added car's fixed black (a mask's 224, magenta in the PSD): the texels it marks take the black `rgb`, read from the masks of the skin's folder, as the paint
 * is; the stock search (no folder: by the stem, an original car's masks) finds none for an added car and changes nothing */
static void test_added_texture_black_by_its_mask(void)
{
    char dir[400];
    snprintf(dir, sizeof dir, "%s/skins/blacked", base);
    uint8_t black_half[8] = { CAR_MASK_BLACK, CAR_MASK_BLACK, CAR_MASK_BLACK, CAR_MASK_BLACK, 0, 0, 0, 0 }, *png = NULL;
    size_t n = 0;
    if (car_mask_write(black_half, 4, 2, &png, &n)) spill(dir, "topv_mask.png", png, n), png_free(png);
    const uint8_t black[3] = CAR_PAINT_ADDED_BLACK;
    size_t size = 0;

    uint8_t *cfm = cfm_of(&size);
    CHECK(drive_livery_black("TESTCAR", dir, cfm, size, black) > 0);
    const uint8_t *topv = texels(cfm, size, 0, "topv");
    CHECK(topv != NULL);
    for (int i = 0; i < 4; i++) CHECK(topv[i] != 7);        /* the mask's black: another colour of the palette */
    for (int i = 4; i < 8; i++) CHECK_EQ_INT(topv[i], 7);   /* its paint: as it was (the paint is a later pass) */
    free(cfm);

    /* what the paint alone does with the black-class texels (the mechanism the measurement on an added car showed): nothing. A texel whose mask is not 0 is left where it
     * is (repaint_apply skips it) and counts as the entry's "other" users (survey_count), so the entry it shares with the paint texels is not recoloured in place: the paint
     * texels move to a spare entry of the paint's colour and the black-class texels keep their index and their colour. Without the black pass they stay the texture's own */
    cfm = cfm_of(&size);
    uint8_t before[3];
    memcpy(before, pal_colour(cfm, size, 0, 7), 3);
    const uint8_t red[3] = { 0xc8, 0x10, 0x2e };
    CHECK(drive_livery_paint_masks("TESTCAR", dir, cfm, size, red) > 0);
    topv = texels(cfm, size, 0, "topv");
    CHECK(topv != NULL);
    for (int i = 0; i < 4; i++) CHECK_EQ_INT(topv[i], 7);   /* the black class: the same entry */
    for (int i = 4; i < 8; i++) CHECK(topv[i] != 7);        /* the paint class: another one */
    CHECK(memcmp(pal_colour(cfm, size, 0, 7), before, 3) == 0);   /* and that entry's colour is as it was */
    free(cfm);

    cfm = cfm_of(&size);   /* the stock search: no folder, no masks for an added stem */
    CHECK_EQ_INT(drive_livery_black("TESTCAR", NULL, cfm, size, black), 0);
    topv = texels(cfm, size, 0, "topv");
    for (int i = 0; i < 8; i++) CHECK_EQ_INT(topv[i], 7);
    free(cfm);
}

/* an added car's skin's masks reach the lamps and the shine as they reach the paint: the lamp texels its own mask marks (a brake light in a texture of its own) and the draft
 * of the shine its masks make (the added texture is a body texture where its mask paints); the stock search by the stem finds neither for an added car */
typedef struct {
    int calls;
} DraftSeen;

static void draft_seen(void *user, int level, const char name[4], int width, int height, const uint8_t *pixels, const uint8_t *keep, const uint8_t rgb[768])
{
    (void)level, (void)width, (void)height, (void)pixels, (void)keep, (void)rgb;
    if (memcmp(name, "tras", 4) == 0) ((DraftSeen *)user)->calls++;
}

static void test_added_texture_lamps_and_shine_by_its_mask(void)
{
    char dir[400];
    snprintf(dir, sizeof dir, "%s/skins/lit", base);
    uint8_t brake_half[8] = { CAR_MASK_BRAKE, CAR_MASK_BRAKE, CAR_MASK_BRAKE, CAR_MASK_BRAKE, CAR_MASK_MATTE, CAR_MASK_MATTE, CAR_MASK_MATTE, CAR_MASK_MATTE }, *png = NULL;
    size_t n = 0;
    if (car_mask_write(brake_half, 4, 2, &png, &n)) spill(dir, "tras_mask.png", png, n), png_free(png);
    size_t size = 0;

    uint8_t *cfm = cfm_named("tras", &size);
    LampTexels lamps = { 0 };
    CHECK_EQ_INT(drive_livery_lamps_masks("TESTCAR", dir, cfm, size, &lamps), 4);   /* the mask's four brake texels, in any texture that has a mask */
    lamp_texels_free(&lamps);
    memset(&lamps, 0, sizeof lamps);
    CHECK_EQ_INT(drive_livery_lamps("TESTCAR", cfm, size, &lamps), 0);                /* the stock search: no masks for an added stem */
    lamp_texels_free(&lamps);
    free(cfm);

    char paint_dir[400];
    snprintf(paint_dir, sizeof paint_dir, "%s/skins/shiny", base);
    uint8_t paint_half[8] = { 0, 0, 0, 0, 255, 255, 255, 255 };
    if (car_mask_write(paint_half, 4, 2, &png, &n)) spill(paint_dir, "tras_mask.png", png, n), png_free(png);
    cfm = cfm_named("tras", &size);
    DraftSeen seen = { 0 };
    CHECK(drive_livery_draft_masks("TESTCAR", paint_dir, cfm, size, draft_seen, &seen) > 0);
    CHECK(seen.calls > 0);                                                         /* the added texture is drafted where its mask paints */
    seen.calls = 0;
    drive_livery_draft("TESTCAR", cfm, size, draft_seen, &seen);
    CHECK_EQ_INT(seen.calls, 0);                                                   /* the stock search: not a body texture, no mask */
    free(cfm);
}

/* what a draft reported: each texture's name and how many texels of each class (paint, matte, shine, body), and a hash of all its keep maps */
typedef struct {
    int calls;
    char name[8][4];
    int level[8];
    int klass[8][4];
    uint32_t hash;
} DraftLog;

static void draft_log(void *user, int level, const char name[4], int width, int height, const uint8_t *pixels, const uint8_t *keep, const uint8_t rgb[768])
{
    (void)pixels, (void)rgb;
    DraftLog *log = user;
    if (log->calls >= 8) return;
    memcpy(log->name[log->calls], name, 4);
    log->level[log->calls] = level;
    memset(log->klass[log->calls], 0, sizeof log->klass[0]);
    for (int i = 0; i < width * height; i++) {
        log->klass[log->calls][keep[i] & 3]++;
        log->hash = (log->hash ^ keep[i]) * 16777619u;
    }
    log->calls++;
}

static int drafted(const DraftLog *log, const char *name, int level)
{
    for (int i = 0; i < log->calls; i++)
        if (memcmp(log->name[i], name, 4) == 0 && log->level[i] == level) return i;
    return -1;
}

/* the traffic cars' high bank has a `side` (and some a `back`) texture that the catalogue cars' has not (they have rsid and rear): a mask in the stock
 * search makes it a body texture of the draft, none leaves it out as it was, and a bank like a catalogue car's drafts the same as it did */
static void test_high_bank_side_and_back_by_their_masks(void)
{
    char dir[400], env[400];
    snprintf(dir, sizeof dir, "%s/traffic_masks/TRAFTEST", base);
    snprintf(env, sizeof env, "%s/traffic_masks", base);
    uint8_t half[8] = { 0, 0, 0, 0, CAR_MASK_SHINE, CAR_MASK_SHINE, CAR_MASK_SHINE, CAR_MASK_SHINE }, *png = NULL;
    size_t n = 0;
    if (car_mask_write(half, 4, 2, &png, &n)) spill(dir, "side_mask.png", png, n), spill(dir, "back_mask.png", png, n), spill(dir, "rear_mask.png", png, n), png_free(png);
    setenv("TNFS_CAR_MASKS", env, 1);   /* the dev override car_mask_load reads first */
    size_t size = 0;

    uint8_t *cfm = cfm_named("side", &size);
    DraftLog log = { 0 };
    drive_livery_draft("TRAFTEST", cfm, size, draft_log, &log);
    const int side = drafted(&log, "side", 0);
    CHECK(side >= 0);                                            /* (a) a masked high-bank side is drafted ... */
    if (side >= 0) {   /* ... with the mask's classes: paint and shine */
        CHECK_EQ_INT(log.klass[side][0], 4);
        CHECK_EQ_INT(log.klass[side][2], 4);
    }
    free(cfm);

    cfm = cfm_named("back", &size);
    memset(&log, 0, sizeof log);
    drive_livery_draft("TRAFTEST", cfm, size, draft_log, &log);
    const int back = drafted(&log, "back", 0);
    CHECK(back >= 0);                                            /* (a) and a masked back too */
    if (back >= 0) CHECK_EQ_INT(log.klass[back][2], 4);
    free(cfm);

    cfm = cfm_named("side", &size);
    memset(&log, 0, sizeof log);
    drive_livery_draft("NOMASKS", cfm, size, draft_log, &log);
    CHECK_EQ_INT(drafted(&log, "side", 0), -1);                  /* (b) no mask file: not a body texture, as before */
    free(cfm);

    cfm = cfm_named("rear", &size);
    memset(&log, 0, sizeof log);
    log.hash = 2166136261u;
    drive_livery_draft("TRAFTEST", cfm, size, draft_log, &log);
    CHECK(drafted(&log, "rear", 0) >= 0);                        /* (c) a catalogue-shaped bank (no side or back): the draft the old code made */
    CHECK_EQ_INT(drafted(&log, "side", 0), -1);
    CHECK_EQ_INT((int)log.hash, (int)CATALOGUE_SHAPED_DRAFT_HASH);
    free(cfm);
    unsetenv("TNFS_CAR_MASKS");
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
    CHECK(strcmp(l.worn.skin, "stripes") == 0 && strcmp(l.worn.paint, "Blue") == 0);   /* what a replay keeps */
    drive_skin_choose(&c, &(CarLook){ "", "Blue" }, &l);   /* the stock skin has no masks here: Color is off, the colour kept for later */
    CHECK(l.skin_dir[0] == '\0' && strcmp(l.mask_dir, "/mods/test-car/car_masks/TESTCAR") == 0 && !l.paint_on);
    c.skin_masked[0] = 1;
    drive_skin_choose(&c, &(CarLook){ "", "Red" }, &l);
    CHECK(l.skin_dir[0] == '\0' && l.paint_on && l.rgb[0] == 0xc8);
    drive_skin_choose(&c, &(CarLook){ "gone", "Pink" }, &l);   /* a skin and a paint the piece no longer has: the stock skin, its own colour */
    CHECK(l.skin_dir[0] == '\0' && !l.paint_on);
    CHECK(l.worn.skin[0] == '\0' && l.worn.paint[0] == '\0');   /* the fallback is what was worn */
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
    test_added_texture_painted_by_its_mask();
    test_added_texture_black_by_its_mask();
    test_added_texture_lamps_and_shine_by_its_mask();
    test_high_bank_side_and_back_by_their_masks();
    test_the_look();
    char cmd[400];
    snprintf(cmd, sizeof cmd, "find '%s' -delete", base);
    if (system(cmd) != 0) fprintf(stderr, "could not remove %s\n", base);
    return TEST_REPORT();
}
