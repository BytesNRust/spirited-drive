/* The painted preview (editor/car_preview.c): the texels it shows are the game's own. For a synthetic car file and masks (the builders of test_paint_golden.c, copied: no game data, so
 * it runs in CI) the preview equals the colours of the file after the core's livery pass (formats/car_livery_pass.h, the pass the game's drive_livery_* run: its output is pinned by
 * the 1a golden, whose three hashes for it are checked here too) runs on it directly with the same masks; a mask edit not yet saved is
 * used in place of its file; the body's colours are counted and what is said about them. */
#include "car_preview.h"
#include "formats/car_bank_walk.h"
#include "formats/png.h"
#include "formats/shpi_build.h"
#include "formats/car_livery_pass.h"
#include "formats/car_mask_codec.h"
#include "formats/car_paint.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* the 1a golden's values for the pass (tests/test_paint_golden.c): a mask folder through the paint, the added car's fixed black, an original car's fixed black */
static const uint64_t GOLDEN_MASKS = 0x183bc588d5bd6e89ULL, GOLDEN_BLACK_DIR = 0xb48681832aafa24dULL, GOLDEN_BLACK = 0x11b659f841bff0abULL;

static uint64_t fnv(uint64_t h, const void *data, size_t n)
{
    const uint8_t *p = data;
    for (size_t i = 0; i < n; i++) h = (h ^ p[i]) * 0x100000001b3ULL;
    return h;
}
static const uint64_t FNV_START = 0xcbf29ce484222325ULL;

/* a PreviewIn by name of its fields (the struct has others the test leaves empty) */
#define IN(c_, s_, m_, e_, p_, a_) ((PreviewIn){ .cfm = (c_), .stem = (s_), .masks_dir = (m_), .edit = (e_), .paint = (p_), .added = (a_) })

static const char STEM[] = "GOLDEN";
static char base[256];

static void put_u32(uint8_t *p, uint32_t v)
{
    for (int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i));
}

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


/* the reference: the colour of every texel of every texture, read straight from a CFM after the game's own calls */
typedef struct {
    uint8_t *rgb[2][CAR_BANK_MAX_TEXTURES];
    int w[2][CAR_BANK_MAX_TEXTURES], h[2][CAR_BANK_MAX_TEXTURES], n[2];
} Ref;

static void ref_texture(void *user, int level, const char name[4], int width, int height, RepaintTexture *tex)
{
    (void)name, (void)tex;
    Ref *r = user;
    const int k = r->n[level]++;
    r->w[level][k] = width, r->h[level][k] = height;
}

static int ref_bank(void *user, const RepaintBank *bank, const CarBankMeta *meta)
{
    Ref *r = user;
    uint8_t six[768], rgb[768];
    for (int i = 0; i < 768; i++) six[i] = (uint8_t)(bank->palette[0][i] >> 2);   /* the fixture's palette is 8 bits (type 0x24) */
    for (int i = 0; i < 768; i++) rgb[i] = (uint8_t)((six[i] * 0xff + 0x20) / 0x3f);
    for (int k = 0; k < bank->texture_count; k++) {
        const size_t px = (size_t)r->w[meta->level][k] * r->h[meta->level][k];
        r->rgb[meta->level][k] = malloc(px * 3);
        for (size_t i = 0; i < px; i++) memcpy(r->rgb[meta->level][k] + i * 3, rgb + bank->textures[k].pixels[i] * 3, 3);
    }
    return 0;
}

static void ref_free(Ref *r)
{
    for (int l = 0; l < 2; l++)
        for (int k = 0; k < CAR_BANK_MAX_TEXTURES; k++) free(r->rgb[l][k]);
}

static const uint8_t RED_PAINT[3] = { 0xc8, 0x10, 0x2e };

/* 1 when every texel of the preview is the reference's colour (the textures in the walk's order: the high bank, then the low) */
static int same_as(const CarPreview *p, const Ref *r)
{
    int i = 0;
    for (int l = 0; l < 2; l++)
        for (int k = 0; k < r->n[l]; k++, i++) {
            if (i >= p->count || p->tex[i].info.level != l || p->tex[i].info.width != r->w[l][k]) return 0;
            const size_t px = (size_t)r->w[l][k] * r->h[l][k];
            for (size_t t = 0; t < px; t++)
                if (memcmp(p->tex[i].rgba + t * 4, r->rgb[l][k] + t * 3, 3) != 0) return 0;
        }
    return i == p->count;
}

/* the test's own reader of a mask folder: <dir>/<name>[_lo]_mask.png, scaled to the texture by car_mask_parse */
static uint8_t *ref_mask(void *user, const char name[4], int level, int width, int height)
{
    char path[600];
    snprintf(path, sizeof path, "%s/%.4s%s_mask.png", (const char *)user, name, level != 0 ? "_lo" : "");
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    static uint8_t png[1 << 16];
    const size_t n = fread(png, 1, sizeof png, f);
    fclose(f);
    uint8_t *mask = malloc((size_t)width * (size_t)height);
    if (mask != NULL && !car_mask_parse(png, n, width, height, mask)) free(mask), mask = NULL;
    return mask;
}

/* the reference: the file after the core's pass with the folder `dir`, as the preview runs it (the fixed black, then the paint) */
static int make_ref(const char *dir, const uint8_t *paint, const char *stem, int added, Ref *r)
{
    size_t size = 0;
    uint8_t *cfm = make_cfm(&size);
    const CarLiveryPass pass = { "GOLDEN", added, 0, ref_mask, (void *)dir };
    const CarPaint *black = car_paint_black_of(stem);
    const uint8_t added_black[3] = CAR_PAINT_ADDED_BLACK;
    if (black != NULL || added) car_livery_black(cfm, size, &pass, black != NULL ? black->rgb : added_black);
    if (paint != NULL) car_livery_paint(cfm, size, &pass, paint);
    const CarBankHooks hooks = { ref_texture, ref_bank };
    memset(r, 0, sizeof *r);
    car_bank_walk(cfm, size, &hooks, r);
    free(cfm);
    return 1;
}

static void write_cfm(const char *path)
{
    size_t size = 0;
    uint8_t *cfm = make_cfm(&size);
    FILE *f = fopen(path, "wb");
    fwrite(cfm, 1, size, f);
    fclose(f);
    free(cfm);
}

int main(void)
{
    snprintf(base, sizeof base, "/tmp/tnfs-preview-XXXXXX");
    if (mkdtemp(base) == NULL) return 2;
    make_palette();
    make_masks();
    char cfm_path[400], masks[400], root[400], why[256];
    snprintf(cfm_path, sizeof cfm_path, "%s/car.CFM", base);
    snprintf(masks, sizeof masks, "%s/masks", base);
    snprintf(root, sizeof root, "%s/root", base);
    write_cfm(cfm_path);
    Ref ref;

    /* no paint: the file's colours (the fixture's stem has no catalogue black) */
    PreviewIn in = IN(cfm_path, STEM, masks, NULL, NULL, 0);
    CarPreview p;
    CHECK(car_preview_make(&in, &p, why, sizeof why));
    make_ref(masks, NULL, STEM, 0, &ref);
    CHECK(same_as(&p, &ref));
    ref_free(&ref);
    CHECK_EQ_INT(p.painted, 0);
    CHECK(p.count == 8);   /* five textures in the high bank, three in the low */
    CHECK(p.body_colours > 0);
    CarPreview unpainted = p;
    memset(&p, 0, sizeof p);

    /* a paint: the game's calls on a copy give the same colours, and they are not the file's */
    in.paint = RED_PAINT;
    CHECK(car_preview_make(&in, &p, why, sizeof why));
    CHECK(p.painted > 0);
    make_ref(masks, RED_PAINT, STEM, 0, &ref);
    CHECK(same_as(&p, &ref));
    ref_free(&ref);
    int differs = 0;
    for (int i = 0; i < p.count; i++)
        if (memcmp(p.tex[i].rgba, unpainted.tex[i].rgba, (size_t)p.tex[i].info.width * p.tex[i].info.height * 4) != 0) differs = 1;
    CHECK(differs);
    CHECK(p.tex[0].info.level == 0 && strcmp(p.tex[0].info.name, "topv") == 0 && strcmp(p.tex[0].info.file, "topv_mask.png") == 0);
    CHECK(p.tex[5].info.level == 1 && strcmp(p.tex[5].info.file, "side_lo_mask.png") == 0);
    CHECK(p.tex[0].body);   /* topv is a body texture */
    CHECK(strstr(p.note, "colours") != NULL);
    CHECK(p.tex[0].mask != NULL && p.tex[3].mask == NULL);   /* topv has a mask in the fixture, bott has none */
    {   /* the classes are the file's, read by the game's own loader */
        uint8_t *want = ref_mask(masks, "topv", 0, 16, 8);
        CHECK(want != NULL && memcmp(want, p.tex[0].mask, 16 * 8) == 0);
        free(want);
    }
    CHECK(p.tex[0].index != NULL && p.palette[0][3 * p.tex[0].index[0]] == p.tex[0].rgba[0]);   /* the indices and the palette give the same colours as the picture */
    car_preview_free(&unpainted);
    car_preview_free(&p);

    /* the painted file as bytes: the same layout, and the game's calls on a copy give the same bytes */
    {
        uint8_t *got = NULL, *want = NULL;
        size_t glen = 0, wlen = 0;
        int blacked = -1, painted = -1;
        CHECK(car_preview_painted_cfm(&IN(cfm_path, STEM, masks, NULL, RED_PAINT, 0), &got, &glen, &blacked, &painted, why, sizeof why));
        want = make_cfm(&wlen);
        const CarLiveryPass pass = { "GOLDEN", 0, 0, ref_mask, (void *)masks };
        const int np = car_livery_paint(want, wlen, &pass, RED_PAINT);
        CHECK(glen == wlen && memcmp(got, want, wlen) == 0);
        CHECK(painted == np && painted > 0 && blacked == 0);
        free(got);
        free(want);
        size_t orig_len = 0;
        uint8_t *orig = make_cfm(&orig_len);
        CHECK(car_preview_painted_cfm(&IN(cfm_path, STEM, masks, NULL, NULL, 0), &got, &glen, NULL, NULL, why, sizeof why));
        CHECK(glen == orig_len && memcmp(got, orig, orig_len) == 0);   /* no paint, no black: the file as it is */
        free(got);
        free(orig);
        CHECK(!car_preview_painted_cfm(&IN("/nonexistent/x.CFM", STEM, NULL, NULL, NULL, 0), &got, &glen, NULL, NULL, why, sizeof why));
    }

    /* the pass is the game's: its hashes are the 1a golden's (test_paint_golden.c), whatever the folder is read by */
    {
        size_t size = 0;
        uint8_t *cfm = make_cfm(&size);
        const CarLiveryPass skin = { "GOLDEN", 1, 0, ref_mask, (void *)masks };
        int changed = car_livery_paint(cfm, size, &skin, RED_PAINT);
        CHECK(fnv(fnv(FNV_START, cfm, size), &changed, sizeof changed) == GOLDEN_MASKS);
        free(cfm);
        cfm = make_cfm(&size);
        const CarLiveryPass added = { "ADDED", 1, 0, ref_mask, (void *)masks };
        const uint8_t added_black[3] = CAR_PAINT_ADDED_BLACK;
        changed = car_livery_black(cfm, size, &added, added_black);
        CHECK(fnv(fnv(FNV_START, cfm, size), &changed, sizeof changed) == GOLDEN_BLACK_DIR);
        free(cfm);
        cfm = make_cfm(&size);
        const CarLiveryPass original = { "GOLDEN", 0, 0, ref_mask, (void *)root };   /* root is <base>/root: the folder above GOLDEN */
        char golden_dir[500];
        snprintf(golden_dir, sizeof golden_dir, "%s/GOLDEN", root);
        const CarLiveryPass original_dir = { "GOLDEN", 0, 0, ref_mask, golden_dir };
        (void)original;
        const uint8_t black[3] = { 0x14, 0x14, 0x16 };
        changed = car_livery_black(cfm, size, &original_dir, black);
        CHECK(fnv(fnv(FNV_START, cfm, size), &changed, sizeof changed) == GOLDEN_BLACK);
        free(cfm);
    }

    /* an edit not yet saved is used in place of its file: the whole topv mask matte keeps the body as it was, whatever the paint says */
    char topv[400];
    snprintf(topv, sizeof topv, "%s/topv_mask.png", masks);
    MaskEdit e;
    CHECK(mask_edit_open(&e, topv, 0, 0, NULL, why, sizeof why));
    for (int y = 0; y < e.height; y++)
        for (int x = 0; x < e.width; x++) mask_edit_pencil(&e, x, y, 1, 255);   /* every texel matte */
    in.edit = &e;
    CHECK(car_preview_make(&in, &p, why, sizeof why));
    CarPreview edited = p;
    memset(&p, 0, sizeof p);
    in.edit = NULL;
    CHECK(car_preview_make(&in, &p, why, sizeof why));
    CHECK(memcmp(p.tex[0].rgba, edited.tex[0].rgba, (size_t)p.tex[0].info.width * p.tex[0].info.height * 4) != 0);   /* the edit shows */
    {   /* the unsaved edit is the mask the preview carries: all matte; the file's own is not */
        int edited_matte = 1, file_matte = 1;
        for (int i = 0; i < 16 * 8; i++) edited_matte &= edited.tex[0].mask[i] == CAR_MASK_MATTE, file_matte &= p.tex[0].mask[i] == CAR_MASK_MATTE;
        CHECK(edited_matte && !file_matte);
    }
    CHECK(memcmp(p.tex[1].rgba, edited.tex[1].rgba, (size_t)p.tex[1].info.width * p.tex[1].info.height * 4) == 0);   /* another texture is as it was */
    CHECK(mask_edit_modified(&e));   /* nothing was saved: the preview used the edit itself */
    /* the same edit saved to its file gives the same preview as the unsaved one */
    CHECK(mask_edit_save(&e, why, sizeof why));
    CarPreview saved;
    CHECK(car_preview_make(&IN(cfm_path, STEM, masks, NULL, RED_PAINT, 0), &saved, why, sizeof why));
    CHECK(memcmp(saved.tex[0].rgba, edited.tex[0].rgba, (size_t)saved.tex[0].info.width * saved.tex[0].info.height * 4) == 0);
    car_preview_free(&saved);
    car_preview_free(&edited);
    car_preview_free(&p);
    mask_edit_free(&e);

    /* a car with a catalogue black (the fixed black of the masks: a stem the catalogue has) is blacked as the race does; no masks folder: the game's own are used */
    CHECK(car_paint_black_of("P911") != NULL);
    CarPreview b;
    CHECK(car_preview_make(&IN(cfm_path, "P911", NULL, NULL, NULL, 0), &b, why, sizeof why));
    CHECK(b.count == 8);
    car_preview_free(&b);

    /* a count of body colours: what is said when they are more than the car's entries */
    CHECK(PREVIEW_OWN_ENTRIES == 23 && PREVIEW_SHARED_ENTRIES == 72);
    {   /* the line about the palette: none when all fit, grey for the game's own cars, amber for a person's */
        CarPreview q;
        char line[160], help[PREVIEW_NOTE + 160];
        memset(&q, 0, sizeof q);
        snprintf(q.note, sizeof q.note, "the note");
        q.body_colours = PREVIEW_OWN_ENTRIES;
        CHECK_EQ_INT(car_preview_palette_line(&q, 0, line, sizeof line, help, sizeof help), PREVIEW_LINE_NONE);
        CHECK_EQ_INT(car_preview_palette_line(&q, 1, line, sizeof line, help, sizeof help), PREVIEW_LINE_NONE);
        q.body_colours = 79;
        CHECK_EQ_INT(car_preview_palette_line(&q, 0, line, sizeof line, help, sizeof help), PREVIEW_LINE_WARN);
        CHECK(strstr(line, "56 of the body's 79 colours") != NULL && strstr(help, "Nothing to fix") == NULL);
        CHECK_EQ_INT(car_preview_palette_line(&q, 1, line, sizeof line, help, sizeof help), PREVIEW_LINE_INFO);
        CHECK(strstr(line, "56 of the body's 79 colours") != NULL && strstr(help, "the note") != NULL && strstr(help, "game's own cars") != NULL);
    }

    /* an added car: the masks are a skin's folder, read through drive_livery_*_masks with the catalogue's usual black: the same as the game's calls with that folder */
    {
        CarPreview a;
        CHECK(car_preview_make(&IN(cfm_path, STEM, masks, NULL, RED_PAINT, 1), &a, why, sizeof why));
        size_t size = 0;
        uint8_t *cfm = make_cfm(&size);
        const uint8_t added_black[3] = CAR_PAINT_ADDED_BLACK;
        const CarLiveryPass pass = { "GOLDEN", 1, 0, ref_mask, (void *)masks };
        CHECK(car_livery_black(cfm, size, &pass, added_black) > 0);
        CHECK(car_livery_paint(cfm, size, &pass, RED_PAINT) > 0);
        Ref r;
        memset(&r, 0, sizeof r);
        const CarBankHooks hooks = { ref_texture, ref_bank };
        car_bank_walk(cfm, size, &hooks, &r);
        CHECK(same_as(&a, &r));
        CHECK(a.blacked > 0 && a.painted > 0);
        ref_free(&r);
        free(cfm);
        car_preview_free(&a);
    }

    /* the refusals */
    CarPreview none;
    CHECK(!car_preview_make(&IN("/nonexistent/car.CFM", STEM, NULL, NULL, NULL, 0), &none, why, sizeof why));
    CHECK(strstr(why, "Cannot read") != NULL);
    char junk[400];
    snprintf(junk, sizeof junk, "%s/junk.CFM", base);
    FILE *f = fopen(junk, "wb");
    fputs("not a cfm", f);
    fclose(f);
    CHECK(!car_preview_make(&IN(junk, STEM, NULL, NULL, NULL, 0), &none, why, sizeof why));
    CHECK(strstr(why, "no textures") != NULL);
    return TEST_REPORT();
}
