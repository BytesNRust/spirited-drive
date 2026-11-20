/* The car paint code's output, pinned (dep59 car editor, 1a): a synthetic car file and masks (no game data, so it runs in CI) put through every
 * entry point of the livery code (game/viewer/drive_livery.h): the paint by the family rules, by a mask folder, the fixed black, the lamp texels,
 * the draft of the masks, and the catalogue. Each result is hashed (FNV-1a, 64 bits) and compared with the value this file holds. The paint
 * code moves to the core with no change in behaviour, so none of these may change with it: a changed hash is a changed paint, never a "fix"
 * of the number. */
#include "formats/png.h"
#include "formats/shpi_build.h"
#include "game/race/car_mask.h"
#include "game/race/car_paint.h"
#include "game/race/lamp_emissive.h"
#include "game/viewer/drive_livery.h"
#include "test.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* the values of this file's results */
static const uint64_t GOLDEN_RULES = 0x25b9601d5fad4f62ULL, GOLDEN_MASKS = 0x183bc588d5bd6e89ULL, GOLDEN_BLACK = 0x11b659f841bff0abULL, GOLDEN_LAMPS = 0x93d94adea708889bULL, GOLDEN_DRAFT = 0x6a9eff9842881b23ULL,
                      GOLDEN_CATALOGUE = 0x455eced6ab0b51c4ULL;
/* the paths the added-car work added to the code (dep59 1a, on main 0e14fcc6) */
static const uint64_t GOLDEN_BLACK_DIR = 0xb48681832aafa24dULL, GOLDEN_LAMPS_DIR = 0x93d94adea708889bULL, GOLDEN_DRAFT_DIR = 0x40a88611b2908c72ULL,
                      GOLDEN_TRAFFIC = 0x3a6437c5513c5548ULL, GOLDEN_TRAFFIC_DIR = 0x3a6437c5513c5548ULL;

static const char STEM[] = "GOLDEN";
static char base[256];

static void put_u32(uint8_t *p, uint32_t v)
{
    for (int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i));
}

static uint64_t fnv(uint64_t h, const void *data, size_t n)
{
    const uint8_t *p = data;
    for (size_t i = 0; i < n; i++) h = (h ^ p[i]) * 0x100000001b3ULL;
    return h;
}
static const uint64_t FNV_START = 0xcbf29ce484222325ULL;

static uint32_t lcg_state;
static uint32_t lcg(void)
{
    lcg_state = lcg_state * 1664525u + 1013904223u;
    return lcg_state >> 8;
}

/* the palette: a red body ramp (32..47: the family rules need an entry shown at least 8 times), a grey one (glass and trim, 96..127), blues (128..159), saturated yellows (160..191); 192..255 stay unused (the spare entries) */
static uint8_t palette[768];
static void make_palette(void)
{
    memset(palette, 0, sizeof palette);
    for (int i = 0; i < 16; i++) {
        palette[3 * (32 + i) + 0] = (uint8_t)(60 + 11 * i), palette[3 * (32 + i) + 1] = (uint8_t)(8 + 3 * i), palette[3 * (32 + i) + 2] = (uint8_t)(10 + 3 * i);
    }
    for (int i = 0; i < 32; i++) palette[3 * (96 + i) + 0] = palette[3 * (96 + i) + 1] = palette[3 * (96 + i) + 2] = (uint8_t)(6 + 5 * i);
    for (int i = 0; i < 32; i++) palette[3 * (128 + i) + 0] = (uint8_t)(10 + i), palette[3 * (128 + i) + 1] = (uint8_t)(30 + 2 * i), palette[3 * (128 + i) + 2] = (uint8_t)(90 + 4 * i);
    for (int i = 0; i < 32; i++) palette[3 * (160 + i) + 0] = 250, palette[3 * (160 + i) + 1] = (uint8_t)(180 + i), palette[3 * (160 + i) + 2] = 20;
}

typedef struct {
    const char name[4];
    int w, h;
} Tex;

/* a bank: !PAL, then each texture with deterministic texels of the four families */
static uint8_t *bank_of(const Tex *tex, int count, uint32_t seed, size_t *size)
{
    enum { MAX = 8 };
    ShpiDescEntry e[MAX + 1];
    static uint8_t pixels[MAX][32 * 16];
    memset(e, 0, sizeof e);
    lcg_state = seed;
    e[0] = (ShpiDescEntry){ .name = { '!', 'P', 'A', 'L' }, .type = 0x24, .width = 256, .height = 1, .data = palette, .block_size = SHPI_BLOCK_AUTO, .dir_index = 0 };
    for (int t = 0; t < count; t++) {
        for (int i = 0; i < tex[t].w * tex[t].h; i++) {
            const uint32_t r = lcg();
            pixels[t][i] = (uint8_t)(r % 10 < 6 ? 32 + r / 16 % 16 : r % 10 < 8 ? 96 + r / 16 % 32 : r % 10 < 9 ? 128 + r / 16 % 32 : 160 + r / 16 % 32);
        }
        e[t + 1] = (ShpiDescEntry){ .type = 0x7b, .width = (uint16_t)tex[t].w, .height = (uint16_t)tex[t].h, .data = pixels[t], .block_size = SHPI_BLOCK_AUTO,
                                    .dir_index = (uint32_t)(t + 1) };
        memcpy(e[t + 1].name, tex[t].name, 4);
    }
    uint8_t *out = NULL;
    ExchangeError err;
    if (!shpi_build(e, (size_t)count + 1, &out, size, &err)) return NULL;
    return out;
}

/* a CFM: wwww [ORIP, high bank (topv, rsid, frnt, bott, wing), ORIP, low bank (side, back, topv)]; `traffic`: a traffic car's high bank, which has a side
 * and a back the catalogue cars have not (topv, side, back, wing) */
static uint8_t *make_cfm_of(size_t *size, int traffic)
{
    static const Tex HIGH[] = { { { 't', 'o', 'p', 'v' }, 16, 8 }, { { 'r', 's', 'i', 'd' }, 16, 8 }, { { 'f', 'r', 'n', 't' }, 8, 8 },
                                { { 'b', 'o', 't', 't' }, 8, 4 }, { { 'w', 'i', 'n', 'g' }, 8, 4 } };
    static const Tex TRAFFIC[] = { { { 't', 'o', 'p', 'v' }, 16, 8 }, { { 's', 'i', 'd', 'e' }, 16, 8 }, { { 'b', 'a', 'c', 'k' }, 8, 8 },
                                   { { 'w', 'i', 'n', 'g' }, 8, 4 } };
    static const Tex LOW[] = { { { 's', 'i', 'd', 'e' }, 8, 4 }, { { 'b', 'a', 'c', 'k' }, 8, 4 }, { { 't', 'o', 'p', 'v' }, 8, 4 } };
    size_t hn = 0, ln = 0;
    uint8_t *high = traffic ? bank_of(TRAFFIC, 4, 1, &hn) : bank_of(HIGH, 5, 1, &hn), *low = bank_of(LOW, 3, 2, &ln);
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

static uint8_t *make_cfm(size_t *size)
{
    return make_cfm_of(size, 0);
}

static void make_dirs(const char *path)
{
    char buf[1024];
    snprintf(buf, sizeof buf, "%s", path);
    for (char *p = buf + 1; *p; ++p)
        if (*p == '/') { *p = '\0'; mkdir(buf, 0777); *p = '/'; }
    mkdir(buf, 0777);
}

/* the mask folders: <base>/masks (a mask folder as drive_livery_paint_masks reads it) and <base>/root/GOLDEN (as TNFS_CAR_MASKS names it): the same files */
static void spill_mask(const char *name, int w, int h, int shift)
{
    static const uint8_t CLASS[] = { CAR_MASK_PAINT, CAR_MASK_PAINT, CAR_MASK_SHINE, CAR_MASK_BODY, CAR_MASK_BLACK, CAR_MASK_BRAKE, CAR_MASK_POSITION,
                                     CAR_MASK_HEADLIGHT, CAR_MASK_REVERSE, CAR_MASK_MATTE, CAR_MASK_PAINT };
    uint8_t mask[16 * 8], *png = NULL;
    size_t n = 0;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) mask[y * w + x] = CLASS[(x + 2 * y + shift) % (int)sizeof CLASS];
    if (!car_mask_write(mask, w, h, &png, &n)) return;
    const char *const dirs[2] = { "masks", "root/GOLDEN" };
    for (int d = 0; d < 2; d++) {
        char dir[400], path[500];
        snprintf(dir, sizeof dir, "%s/%s", base, dirs[d]);
        make_dirs(dir);
        snprintf(path, sizeof path, "%s/%s", dir, name);
        FILE *f = fopen(path, "wb");
        if (f != NULL) fwrite(png, 1, n, f), fclose(f);
    }
    png_free(png);
}

static void make_masks(void)
{
    spill_mask("topv_mask.png", 16, 8, 0);
    spill_mask("rsid_mask.png", 16, 8, 3);
    spill_mask("frnt_mask.png", 8, 8, 5);
    spill_mask("topv_lo_mask.png", 8, 4, 1);   /* the low bank's own files */
    spill_mask("side_lo_mask.png", 8, 4, 2);
    spill_mask("side_mask.png", 16, 8, 4);   /* a traffic car's high bank: a side and a back */
    spill_mask("back_mask.png", 8, 8, 6);
}

static void report(const char *what, uint64_t got, uint64_t pinned)
{
    printf("golden %-10s 0x%016" PRIx64 "%s\n", what, got, got == pinned ? "" : "   <- NOT the pinned value");
    CHECK(got == pinned);
}

static const uint8_t RED[3] = { 0xc8, 0x10, 0x2e }, BLACK[3] = { 0x14, 0x14, 0x16 }, ADDED_BLACK[3] = CAR_PAINT_ADDED_BLACK;

static void test_rules(void)
{
    size_t size = 0;
    uint8_t *cfm = make_cfm(&size);
    CHECK(cfm != NULL);
    unsetenv("TNFS_CAR_MASKS");
    const int changed = drive_livery_paint(STEM, cfm, size, RED);
    CHECK(changed > 0);
    report("rules", fnv(fnv(FNV_START, cfm, size), &changed, sizeof changed), GOLDEN_RULES);
    free(cfm);
}

static void test_masks(void)
{
    size_t size = 0;
    uint8_t *cfm = make_cfm(&size);
    char dir[400];
    snprintf(dir, sizeof dir, "%s/masks", base);
    const int changed = drive_livery_paint_masks(STEM, dir, cfm, size, RED);
    CHECK(changed > 0);
    report("masks", fnv(fnv(FNV_START, cfm, size), &changed, sizeof changed), GOLDEN_MASKS);
    free(cfm);
}

static void test_black(void)
{
    size_t size = 0;
    uint8_t *cfm = make_cfm(&size);
    char root[400];
    snprintf(root, sizeof root, "%s/root", base);
    setenv("TNFS_CAR_MASKS", root, 1);   /* car_mask_load's dev override: <root>/<STEM>/<tex>_mask.png */
    const int changed = drive_livery_black(STEM, NULL, cfm, size, BLACK);
    unsetenv("TNFS_CAR_MASKS");
    CHECK(changed > 0);
    report("black", fnv(fnv(FNV_START, cfm, size), &changed, sizeof changed), GOLDEN_BLACK);
    free(cfm);
}

static void test_lamps(void)
{
    size_t size = 0;
    uint8_t *cfm = make_cfm(&size);
    char root[400];
    snprintf(root, sizeof root, "%s/root", base);
    setenv("TNFS_CAR_MASKS", root, 1);
    LampTexels lamps = { NULL, 0, 0 };
    const int found = drive_livery_lamps(STEM, cfm, size, &lamps);
    unsetenv("TNFS_CAR_MASKS");
    CHECK(found > 0);
    uint64_t h = fnv(FNV_START, &found, sizeof found);
    for (int i = 0; i < lamps.count; i++) h = fnv(fnv(h, &lamps.texel[i].offset, sizeof lamps.texel[i].offset), &lamps.texel[i].kind, 1);
    report("lamps", h, GOLDEN_LAMPS);
    lamp_texels_free(&lamps);
    free(cfm);
}

typedef struct {
    uint64_t hash;
    int banks;
} Draft;

static void draft_fn(void *user, int level, const char name[4], int width, int height, const uint8_t *pixels, const uint8_t *keep, const uint8_t rgb[768])
{
    Draft *d = user;
    d->banks++;
    d->hash = fnv(fnv(fnv(fnv(fnv(d->hash, &level, sizeof level), name, 4), &width, sizeof width), &height, sizeof height), pixels, (size_t)width * (size_t)height);
    d->hash = fnv(fnv(d->hash, keep, (size_t)width * (size_t)height), rgb, 768);
}

static void test_draft(void)
{
    size_t size = 0;
    uint8_t *cfm = make_cfm(&size);
    Draft d = { FNV_START, 0 };
    const int marked = drive_livery_draft(STEM, cfm, size, draft_fn, &d);
    CHECK(marked > 0 && d.banks > 0);
    report("draft", fnv(d.hash, &marked, sizeof marked), GOLDEN_DRAFT);
    free(cfm);
}

/* the paths the added-car work added (a mask folder through the black, the lamps and the draft; the added car's fixed black; a traffic car's high side and back) */
static void test_black_dir(void)
{
    size_t size = 0;
    uint8_t *cfm = make_cfm(&size);
    char dir[400];
    snprintf(dir, sizeof dir, "%s/masks", base);
    const int changed = drive_livery_black("ADDED", dir, cfm, size, ADDED_BLACK);   /* a stem the catalogue has not: the added car's black */
    CHECK(changed > 0);
    report("black_dir", fnv(fnv(FNV_START, cfm, size), &changed, sizeof changed), GOLDEN_BLACK_DIR);
    free(cfm);
}

static void test_lamps_dir(void)
{
    size_t size = 0;
    uint8_t *cfm = make_cfm(&size);
    char dir[400];
    snprintf(dir, sizeof dir, "%s/masks", base);
    LampTexels lamps = { NULL, 0, 0 };
    const int found = drive_livery_lamps_masks("ADDED", dir, cfm, size, &lamps);
    CHECK(found > 0);
    uint64_t h = fnv(FNV_START, &found, sizeof found);
    for (int i = 0; i < lamps.count; i++) h = fnv(fnv(h, &lamps.texel[i].offset, sizeof lamps.texel[i].offset), &lamps.texel[i].kind, 1);
    report("lamps_dir", h, GOLDEN_LAMPS_DIR);
    lamp_texels_free(&lamps);
    free(cfm);
}

static void test_draft_dir(void)
{
    size_t size = 0;
    uint8_t *cfm = make_cfm(&size);
    char dir[400];
    snprintf(dir, sizeof dir, "%s/masks", base);
    Draft d = { FNV_START, 0 };
    const int marked = drive_livery_draft_masks("ADDED", dir, cfm, size, draft_fn, &d);
    CHECK(marked > 0 && d.banks > 0);
    report("draft_dir", fnv(d.hash, &marked, sizeof marked), GOLDEN_DRAFT_DIR);
    free(cfm);
}

static void test_traffic(void)
{
    size_t size = 0;
    uint8_t *cfm = make_cfm_of(&size, 1);
    char root[400];
    snprintf(root, sizeof root, "%s/root", base);
    setenv("TNFS_CAR_MASKS", root, 1);   /* the traffic stem's masks: the side and the back of the high bank take the mask's paint */
    const int changed = drive_livery_paint(STEM, cfm, size, RED);
    unsetenv("TNFS_CAR_MASKS");
    CHECK(changed > 0);
    size_t plain_size = 0;   /* without masks the high side and back stay as they are: the rule is what changes them */
    uint8_t *plain = make_cfm_of(&plain_size, 1);
    const int plain_changed = drive_livery_paint(STEM, plain, plain_size, RED);
    CHECK(plain_size == size && (changed != plain_changed || memcmp(plain, cfm, size) != 0));
    free(plain);
    report("traffic", fnv(fnv(FNV_START, cfm, size), &changed, sizeof changed), GOLDEN_TRAFFIC);
    free(cfm);
}

static void test_traffic_dir(void)
{
    size_t size = 0;
    uint8_t *cfm = make_cfm_of(&size, 1);
    char dir[400];
    snprintf(dir, sizeof dir, "%s/masks", base);
    const int changed = drive_livery_paint_masks(STEM, dir, cfm, size, RED);
    CHECK(changed > 0);
    report("traffic_dir", fnv(fnv(FNV_START, cfm, size), &changed, sizeof changed), GOLDEN_TRAFFIC_DIR);
    free(cfm);
}

static void test_catalogue(void)
{
    uint64_t h = FNV_START;
    for (int model = 0; model < CAR_PAINT_MODELS; model++) {
        const int n = car_paint_count(model);
        h = fnv(h, &n, sizeof n);
        for (int k = 0; k < n; k++) {
            const CarPaint *p = car_paint(model, k);
            CHECK(p != NULL);
            if (p != NULL) h = fnv(fnv(h, p->name, strlen(p->name)), p->rgb, 3);
        }
        const uint8_t *own = car_paint_default(model);
        if (own != NULL) h = fnv(h, own, 3);
    }
    static const char *const STEMS[] = { "ANSX", "p911", "TSupra", "LDiabl", "CZR1", "nope" };
    for (size_t i = 0; i < sizeof STEMS / sizeof *STEMS; i++) {
        const CarPaint *b = car_paint_black_of(STEMS[i]);
        if (b != NULL) h = fnv(fnv(h, b->name, strlen(b->name)), b->rgb, 3);
    }
    report("catalogue", h, GOLDEN_CATALOGUE);
}

int main(void)
{
    snprintf(base, sizeof base, "%s/tnfs_paint_golden_%d", getenv("TMPDIR") != NULL ? getenv("TMPDIR") : "/tmp", (int)getpid());
    make_dirs(base);
    make_palette();
    make_masks();
    test_rules();
    test_masks();
    test_black();
    test_lamps();
    test_draft();
    test_black_dir();
    test_lamps_dir();
    test_draft_dir();
    test_traffic();
    test_traffic_dir();
    test_catalogue();
    return TEST_REPORT();
}
