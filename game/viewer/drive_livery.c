#include "game/viewer/drive_livery.h"

#include "formats/chunk.h"
#include "formats/fsh.h"
#include "formats/psx_bigf.h"
#include "game/race/car_livery.h"
#include "game/race/car_mask.h"
#include "game/race/car_repaint.h"
#include "game/platform/platform.h"
#include "game/sys/port_log.h"
#include "game/sys/shape_bank.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { LIVERY_PATH = 512, LIVERY_STEM = 16, SHAPE_DATA = 0x10, LEVELS = 2 };

static const uint8_t PAL_NAME[4] = { '!', 'P', 'A', 'L' }, OWN_NAME[4] = { '!', 'x', 'x', 'x' };

/* The PC palette entries (indices into the car's '!PAL') that are body colour the PS1 textures do not show as body:
 * the colours both liveries paint, which the count alone leaves unmapped. Found from the PC car's textures: the entries
 * at least four fifths of whose pixels are in the body textures (topv, frnt, rsid, bott, wing), not already mapped, and
 * for TSUPRA red (r > g + 24, r > b + 24). Palette indices only: no colour is copied from the CD. The ANSX's body is
 * a ramp of greys that the glass and trim share, so its set is read from the CFM's textures each time
 * (car_livery_body_set) and not listed. */
static const uint8_t TSUPRA_BODY[] = {
    20, 34, 35, 37, 38, 40, 42, 43, 68, 69, 70, 72, 79, 84, 90, 94, 100, 101, 102, 109, 110, 111, 115, 117, 123, 124,
    129, 130, 133, 134, 135, 136, 138, 139, 150, 153, 154, 155, 156, 157, 159, 160, 164, 165, 169, 171, 172, 173, 176,
    177, 190, 191, 252
};
/* The table for a car's stem (upper case), and its length; 0 entries for a car without one. */
static const uint8_t *forced_table(const char *upper, int *count)
{
    *count = 0;
    if (strcmp(upper, "TSUPRA") == 0) {
        *count = (int)sizeof TSUPRA_BODY;
        return TSUPRA_BODY;
    }
    return NULL;
}

/* The counts of the colours a car's Z<STEM>.<ext> file uses (its texture directory is the lower-case stem .psh), or
 * NULL when the file is missing or does not parse. */
static uint32_t *colours_of(const char *psx_dir, const char *upper, const char *lower, const char *ext)
{
    char path[LIVERY_PATH], member[LIVERY_STEM + 4];
    snprintf(path, sizeof path, "%s/Z%s.%s", psx_dir, upper, ext);
    snprintf(member, sizeof member, "%s.psh", lower);
    size_t size = 0;
    uint8_t *file = platform_read_file(path, &size);
    if (file == NULL) return NULL;
    const uint8_t *texts = NULL;
    size_t length = 0;
    uint32_t *counts = calloc(PSX_COLOURS, sizeof *counts);
    if (counts != NULL && !(psx_bigf_member(file, size, member, &texts, &length) && psx_shpp_colours(texts, length, counts))) {
        free(counts);
        counts = NULL;
    }
    platform_free_file(file);
    return counts;
}

/* The '!PAL' of the CFM's high detail bank: its shape's type and its 256 entries. 0 when the CFM does not parse. */
static int cfm_palette(uint8_t *cfm, size_t size, int *type, uint8_t **palette)
{
    Chunk file, shpi;
    Fsh bank;
    if (chunk_open(&file, cfm, size) != CHUNK_OK) return 0;
    if (chunk_child(&file, 1, &shpi) != CHUNK_OK || shpi.kind != CHUNK_KIND_BITMAP) return 0;
    if (fsh_open(&bank, shpi.data, shpi.size) != FSH_OK) return 0;
    uint8_t *shape = shape_bank_find((uint8_t *)(uintptr_t)bank.data, PAL_NAME);
    if (shape == NULL) return 0;
    *type = shape[0];
    *palette = shape + SHAPE_DATA;
    return 1;
}

/* The counts of each palette index in the CFM's high detail textures: all of them, and the body ones (topv, frnt, rsid,
 * bott, wing). 0 when the CFM does not parse. */
static int texture_counts(uint8_t *cfm, size_t size, uint32_t total[256], uint32_t body[256])
{
    static const char *const BODY_TEXTURES[] = { "topv", "frnt", "rsid", "bott", "wing" };
    Chunk file, shpi;
    Fsh bank;
    if (chunk_open(&file, cfm, size) != CHUNK_OK) return 0;
    if (chunk_child(&file, 1, &shpi) != CHUNK_OK || shpi.kind != CHUNK_KIND_BITMAP) return 0;
    if (fsh_open(&bank, shpi.data, shpi.size) != FSH_OK) return 0;
    for (uint32_t i = 0; i < bank.count; i++) {
        FshEntry e;
        if (fsh_entry(&bank, i, &e) != FSH_OK || e.type != FSH_TYPE_BITMAP) continue;
        int is_body = 0;
        for (size_t k = 0; k < sizeof BODY_TEXTURES / sizeof *BODY_TEXTURES; k++)
            is_body |= memcmp(e.name, BODY_TEXTURES[k], 4) == 0;
        size_t pixels = (size_t)e.width * e.height;
        if (pixels > e.payload_size) pixels = e.payload_size;
        for (size_t p = 0; p < pixels; p++) {
            total[e.payload[p]]++;
            body[e.payload[p]] += (uint32_t)is_body;
        }
    }
    return 1;
}

/* The forced colours of the car: the table's palette entries, or the textures' paint entries for the ANSX, read from the
 * CFM's own palette. */
static int forced_colours(uint8_t *cfm, size_t size, const char *upper, uint16_t out[256])
{
    int count = 0, type = 0;
    uint8_t *palette = NULL;
    const uint8_t *table = forced_table(upper, &count);
    if (!cfm_palette(cfm, size, &type, &palette)) return 0;
    uint8_t entries[256];
    if (table == NULL && strcmp(upper, "ANSX") == 0) {
        uint32_t total[256] = { 0 }, body[256] = { 0 };
        if (!texture_counts(cfm, size, total, body)) return 0;
        count = car_livery_body_set(total, body, palette, type, entries);
        table = entries;
    }
    if (table == NULL) return 0;
    for (int k = 0; k < count; k++) out[k] = car_livery_colour(palette + 3 * table[k], type);
    return count;
}

/* the number plates, in the high-detail texture's own texels (x, y, w, h; the texture's width tells it from the low-detail one's): left as they were by the repaint, as the plate's grey would take the paint */
static const struct { const char *stem, name[5]; int width, rects[REPAINT_KEEPS][4]; } PLATES[] = {
    { "ANSX", "rsid", 257, { { 0, 87, 13, 13 } } }, { "ANSX", "frnt", 65, { { 19, 17, 11, 6 } } },
    /* the Diablo VT: the plate, the narrow strip between the inner lamps (reverse and fog lenses, the lettering) and the two vents under
     * the lamps; the rest of the rear is body colour */
    { "LDIABL", "rear", 37, { { 20, 0, 10, 11 } } },
    { "LDIABL", "rsid", 284, { { 246, 83, 21, 16 }, { 220, 83, 64, 10 }, { 196, 100, 44, 9 }, { 268, 100, 16, 9 } } },
    { "LDIABL", "frnt", 24, { { 12, 5, 6, 4 } } },
    /* the Corvette's hatch glass (the top of the rear texture): dark teal the neighbour rule cannot tell from dark paint */
    { "CZR1", "rsid", 334, { { 0, 0, 334, 35 } } },
};

static void keep_plate(RepaintTexture *tex, const char *upper, const char name[4], int width)
{
    for (size_t k = 0; k < sizeof PLATES / sizeof *PLATES; k++)
        if (strcmp(PLATES[k].stem, upper) == 0 && memcmp(PLATES[k].name, name, 4) == 0 && PLATES[k].width == width) {
            tex->width = width;
            memcpy(tex->keep, PLATES[k].rects, sizeof tex->keep);
        }
}

enum { MAX_TEXTURES = 32 };

/* The lightness (8 bits) under which a neutral colour is glass or shadow, not paint: a grey body (the NSX's ramp) needs it high, the
 * Diablo's black one low, a coloured body only for its greys. By the car's stem (upper case). */
static int glass_floor(const char *upper)
{
    if (strcmp(upper, "ANSX") == 0) return 72;
    if (strcmp(upper, "LDIABL") == 0) return 20;
    return 40;
}

/* what the callback needs to know of the bank beside the texture list: its detail level and the textures' names, in the list's order */
typedef struct {
    int level;
    char name[MAX_TEXTURES][4];
} BankMeta;

typedef int (*RepaintFn)(const RepaintBank *bank, const BankMeta *meta, void *ctx);

/* Runs `fn` on each of the CFM's two banks (high and low detail) with its textures and palettes, as the loader will read them. A body
 * texture that has a hand-made mask (car_mask.h, by the car's `stem` and the texture's name) is given it. */
static int repaint_cfm(uint8_t *cfm, size_t size, const char *stem, const char *upper, RepaintFn fn, void *ctx)
{
    Chunk file;
    int changed = 0;
    if (chunk_open(&file, cfm, size) != CHUNK_OK) return 0;
    for (uint32_t level = 0; level < LEVELS; level++) {
        Chunk shpi;
        Fsh bank;
        if (chunk_child(&file, 2 * level + 1, &shpi) != CHUNK_OK || shpi.kind != CHUNK_KIND_BITMAP) continue;
        if (fsh_open(&bank, shpi.data, shpi.size) != FSH_OK) continue;
        uint8_t *data = (uint8_t *)(uintptr_t)bank.data;
        uint8_t *pal = shape_bank_find(data, PAL_NAME), *own = shape_bank_find(data, OWN_NAME);
        if (pal == NULL) continue;
        RepaintTexture textures[MAX_TEXTURES];
        BankMeta meta = { (int)level, { { 0 } } };
        int count = 0;
        for (uint32_t i = 0; i < bank.count && count < MAX_TEXTURES; i++) {
            FshEntry e;
            if (fsh_entry(&bank, i, &e) != FSH_OK || e.type != FSH_TYPE_BITMAP || e.name[0] == '!') continue;
            size_t pixels = (size_t)e.width * e.height;
            if (pixels > e.payload_size) pixels = e.payload_size;
            RepaintTexture tex = { (uint8_t *)(uintptr_t)e.payload, pixels, car_repaint_body_class(e.name, (int)level), 0, { 0 }, NULL };
            tex.width = (int)e.width;
            keep_plate(&tex, upper, e.name, (int)e.width);
            if (tex.body != REPAINT_NONE) tex.mask = car_mask_load(stem, e.name, (int)level, (int)e.width, (int)e.height);
            memcpy(meta.name[count], e.name, 4);
            textures[count++] = tex;
        }
        const RepaintBank rb = { textures, count, { pal + SHAPE_DATA, own != NULL ? own + SHAPE_DATA : NULL }, 2, pal[0],
                                 own != NULL ? own[0] : 0 };
        const int n = fn(&rb, &meta, ctx);
        if (n > 0) changed += n;
        for (int k = 0; k < count; k++) free((void *)(uintptr_t)textures[k].mask);
    }
    return changed;
}

typedef struct {
    const CarLiveryPair *pairs;
    int count;
    const uint8_t *rgb;
    int floor;
} RepaintJob;

static int run_pairs(const RepaintBank *bank, const BankMeta *meta, void *ctx)
{
    (void)meta;
    const RepaintJob *j = ctx;
    return car_repaint_pairs(bank, j->pairs, j->count, j->floor);
}

static int run_paint(const RepaintBank *bank, const BankMeta *meta, void *ctx)
{
    (void)meta;
    const RepaintJob *j = ctx;
    return car_repaint_paint(bank, j->rgb, j->floor);
}

static int run_black(const RepaintBank *bank, const BankMeta *meta, void *ctx)
{
    (void)meta;
    const RepaintJob *j = ctx;
    return car_repaint_fixed_black(bank, j->rgb, j->floor);
}

int drive_livery_swatch(const char *psx_dir, const char *stem, uint16_t colours[2])
{
    char upper[LIVERY_STEM], lower[LIVERY_STEM];
    size_t i = 0;
    for (; stem[i] != '\0' && i < LIVERY_STEM - 1; i++) {
        upper[i] = (char)toupper((unsigned char)stem[i]);
        lower[i] = (char)tolower((unsigned char)stem[i]);
    }
    upper[i] = lower[i] = '\0';
    uint32_t *normal = colours_of(psx_dir, upper, lower, "VSM"), *alternate = colours_of(psx_dir, upper, lower, "WSM");
    CarLiveryPair pairs[CAR_LIVERY_PAIRS];
    int found = 0;
    if (normal != NULL && alternate != NULL) {
        const int count = car_livery_map(normal, alternate, NULL, 0, pairs);
        found = count > 0 && car_livery_main(pairs, count, alternate, &colours[1]) &&
                car_livery_main_from(pairs, count, normal, &colours[0]);
    }
    free(normal);
    free(alternate);
    return found;
}

int drive_livery_car(const char *psx_dir, const char *stem, uint8_t *cfm, size_t size, uint8_t main_rgb[3])
{
    char upper[LIVERY_STEM], lower[LIVERY_STEM];
    size_t i = 0;
    for (; stem[i] != '\0' && i < LIVERY_STEM - 1; i++) {
        upper[i] = (char)toupper((unsigned char)stem[i]);
        lower[i] = (char)tolower((unsigned char)stem[i]);
    }
    upper[i] = lower[i] = '\0';
    uint32_t *normal = colours_of(psx_dir, upper, lower, "VSM"), *alternate = colours_of(psx_dir, upper, lower, "WSM");
    CarLiveryPair pairs[CAR_LIVERY_PAIRS];
    uint16_t forced[256];
    int count = 0;
    if (normal != NULL && alternate != NULL) {
        const int nforced = forced_colours(cfm, size, upper, forced);
        count = car_livery_map(normal, alternate, forced, nforced, pairs);
    }
    uint16_t main_colour = 0;
    const int has_main = count > 0 && main_rgb != NULL && car_livery_main(pairs, count, alternate, &main_colour);
    free(normal);
    free(alternate);
    if (count == 0) {
        port_log("psx_liveries: no alternate livery for %s in %s\n", stem, psx_dir);
        return 0;
    }
    const RepaintJob job = { pairs, count, NULL, glass_floor(upper) };
    repaint_cfm(cfm, size, stem, upper, run_pairs, (void *)&job);
    if (has_main)
        for (int k = 0; k < 3; k++) main_rgb[k] = (uint8_t)(((main_colour >> (5 * k)) & 31) << 3);
    return count;
}

int drive_livery_paint(const char *stem, uint8_t *cfm, size_t size, const uint8_t rgb[3])
{
    char upper[LIVERY_STEM];
    size_t i = 0;
    for (; stem[i] != '\0' && i < LIVERY_STEM - 1; i++) upper[i] = (char)toupper((unsigned char)stem[i]);
    upper[i] = '\0';
    const RepaintJob job = { NULL, 0, rgb, glass_floor(upper) };
    return repaint_cfm(cfm, size, stem, upper, run_paint, (void *)&job);
}

/* ----- the dash art ----- */

enum { DASH_TEXTURES = 64, DASH_BODY_FIRST = 128, DASH_BODY_LAST = 143, PALETTE_HEADER = 0x10 };

static const uint8_t DASH_NAME[4] = { 'd', 'a', 's', 'h' }, DASH_PAL_NAME[4] = { '!', 'p', 'a', 'l' };

static uint32_t word_at(const uint8_t *b) { return (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24; }

/* the bank's palette as cockpit_load finds it: a '!pal' shape, else the one that follows the 'dash' shape's pixels. NULL when there is none. */
static uint8_t *dash_palette(uint8_t *bank)
{
    uint8_t *shape = shape_bank_find(bank, DASH_PAL_NAME);
    if (shape == NULL) {
        uint8_t *dash = shape_bank_find(bank, DASH_NAME);
        if (dash == NULL) return NULL;
        shape = dash + (word_at(dash) >> 8);
    }
    return shape;
}

int drive_livery_black(const char *stem, uint8_t *cfm, size_t size, const uint8_t rgb[3])
{
    char upper[LIVERY_STEM];
    size_t i = 0;
    for (; stem[i] != '\0' && i < LIVERY_STEM - 1; i++) upper[i] = (char)toupper((unsigned char)stem[i]);
    upper[i] = '\0';
    const RepaintJob job = { NULL, 0, rgb, glass_floor(upper) };
    return repaint_cfm(cfm, size, stem, upper, run_black, (void *)&job);
}

int drive_livery_dash(const char *stem, uint8_t *bank, size_t size, const uint8_t rgb[3])
{
    char upper[LIVERY_STEM];
    size_t i = 0;
    for (; stem[i] != '\0' && i < LIVERY_STEM - 1; i++) upper[i] = (char)toupper((unsigned char)stem[i]);
    upper[i] = '\0';
    Fsh fsh;
    uint8_t *const pal = dash_palette(bank);
    if (pal == NULL || fsh_open(&fsh, bank, size) != FSH_OK || pal + PALETTE_HEADER + 768 > bank + size) return 0;
    RepaintTexture textures[DASH_TEXTURES];
    uint8_t *masks[DASH_TEXTURES] = { NULL };
    int n = 0;
    for (uint32_t k = 0; k < fsh.count && n < DASH_TEXTURES; k++) {
        FshEntry e;
        if (fsh_entry(&fsh, k, &e) != FSH_OK || e.type != FSH_TYPE_BITMAP || e.name[0] == '!') continue;
        size_t pixels = (size_t)e.width * e.height;
        if (pixels > e.payload_size) pixels = e.payload_size;
        RepaintTexture tex = { (uint8_t *)(uintptr_t)e.payload, pixels, REPAINT_NONE, (int)e.width, { { 0 } }, NULL };
        if (memcmp(e.name, DASH_NAME, 4) == 0) {   /* the body the dash shows is the entries DASH_BODY_FIRST..LAST, in this texture alone */
            masks[n] = malloc(pixels);
            if (masks[n] == NULL) continue;
            for (size_t t = 0; t < pixels; t++)
                masks[n][t] = tex.pixels[t] >= DASH_BODY_FIRST && tex.pixels[t] <= DASH_BODY_LAST ? 0 : CAR_MASK_MATTE;
            tex.body = REPAINT_MAIN, tex.mask = masks[n];
        }
        textures[n++] = tex;
    }
    const RepaintBank rb = { textures, n, { pal + PALETTE_HEADER, NULL }, 1, pal[0], 0 };
    const RepaintJob job = { NULL, 0, rgb, glass_floor(upper) };
    const int changed = run_paint(&rb, NULL, (void *)&job);
    for (int k = 0; k < n; k++) free(masks[k]);
    return changed > 0 ? changed : 0;
}

/* ----- the drafts of the masks ----- */

typedef struct {
    LiveryDraftFn fn;
    void *user;
    int floor;
} DraftJob;

static int run_draft(const RepaintBank *bank, const BankMeta *meta, void *ctx)
{
    const DraftJob *j = ctx;
    uint8_t *keeps[MAX_TEXTURES] = { NULL };
    int ok = 1;
    for (int t = 0; t < bank->texture_count; t++) {
        keeps[t] = malloc(bank->textures[t].count > 0 ? bank->textures[t].count : 1);
        ok = ok && keeps[t] != NULL;
    }
    int paint = -1;
    if (ok) paint = car_repaint_draft(bank, j->floor, keeps);
    if (paint >= 0) {
        /* the bank's first palette ('!PAL') is what the textures' indices show; the second ('!xxx') holds only the car's own colours */
        const uint8_t *pal = bank->palette[0];
        const int shift = bank->type == 0x22 ? 2 : 0;
        uint8_t rgb[768];
        for (int i = 0; i < 768; i++) rgb[i] = (uint8_t)(pal[i] << shift);
        for (int t = 0; t < bank->texture_count; t++) {
            const RepaintTexture *tex = &bank->textures[t];
            if (tex->body == REPAINT_NONE || tex->width <= 0) continue;
            j->fn(j->user, meta->level, meta->name[t], tex->width, (int)(tex->count / (size_t)tex->width), tex->pixels, keeps[t], rgb);
        }
    }
    for (int t = 0; t < bank->texture_count; t++) free(keeps[t]);
    return paint > 0 ? paint : 0;
}

int drive_livery_draft(const char *stem, uint8_t *cfm, size_t size, LiveryDraftFn fn, void *user)
{
    char upper[LIVERY_STEM];
    size_t i = 0;
    for (; stem[i] != '\0' && i < LIVERY_STEM - 1; i++) upper[i] = (char)toupper((unsigned char)stem[i]);
    upper[i] = '\0';
    /* the Corvette's dark teal body lies under its runtime floor (a lower one repaints eight lamp texels): the draft starts its fascia as paint */
    const DraftJob job = { fn, user, strcmp(upper, "CZR1") == 0 ? 20 : glass_floor(upper) };
    return repaint_cfm(cfm, size, stem, upper, run_draft, (void *)&job);
}
