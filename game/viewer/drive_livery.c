#include "game/viewer/drive_livery.h"

#include "formats/car_bank_walk.h"
#include "formats/car_livery_pass.h"
#include "formats/chunk.h"
#include "formats/fsh.h"
#include "formats/psx_bigf.h"
#include "game/race/car_livery.h"
#include "game/race/car_mask.h"
#include "game/race/lamp_emissive.h"
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

static const uint8_t PAL_NAME[4] = { '!', 'P', 'A', 'L' };

/* The PC palette entries (indices into the car's '!PAL') that are body colour the console version textures do not show as body:
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
    if (fsh_open(&bank, shpi.data, shpi.size) != FSH_OK || !shape_bank_valid(shpi.data, shpi.size)) return 0;
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

enum { MAX_TEXTURES = CAR_BANK_MAX_TEXTURES };

typedef CarBankMeta BankMeta;   /* what the callback knows of the bank beside the texture list: its detail level and the textures' names */

typedef int (*RepaintFn)(const RepaintBank *bank, const BankMeta *meta, void *ctx);
static int run_lamps(const RepaintBank *bank, const BankMeta *meta, void *ctx);

typedef struct {
    const char *stem, *mask_dir;
} MaskSource;

/* the game's part of the pass (formats/car_livery_pass.h): where a texture's mask is read from, by the folder of an added car's skin, else by the car's stem through the data folders */
static uint8_t *load_mask(void *user, const char name[4], int level, int width, int height)
{
    const MaskSource *src = user;
    return src->mask_dir != NULL ? car_mask_load_dir(src->mask_dir, name, level, width, height) : car_mask_load(src->stem, name, level, width, height);
}

/* Runs `fn` on each of the CFM's two banks (high and low detail) with its textures and palettes, as the loader will read them. A body
 * texture (any texture, for the lamps' walk) that has a hand-made mask (car_mask.h, by the car's `stem` and the texture's name; from
 * `mask_dir` alone when it is not NULL, an added car's skin's) is given it. With `mask_dir` (an added car) any texture is given its
 * mask, and one the race does not paint by its name (an added texture) is the body when its mask paints a texel: an original car's
 * masks (no `mask_dir`) keep their names' classes, so the Jetta's BTM1 mask, for instance, paints nothing. */
static int repaint_cfm(uint8_t *cfm, size_t size, const char *stem, const char *mask_dir, const char *upper, RepaintFn fn, void *ctx)
{
    const MaskSource src = { stem, mask_dir };
    const CarLiveryPass pass = { upper, mask_dir != NULL, fn == run_lamps, load_mask, (void *)&src };
    return car_livery_walk(cfm, size, &pass, fn, ctx);
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
        port_log("extra_liveries: no alternate livery for %s in %s\n", stem, psx_dir);
        return 0;
    }
    const RepaintJob job = { pairs, count, NULL, car_livery_glass_floor(upper) };
    repaint_cfm(cfm, size, stem, NULL, upper, run_pairs, (void *)&job);
    if (has_main)
        for (int k = 0; k < 3; k++) main_rgb[k] = (uint8_t)(((main_colour >> (5 * k)) & 31) << 3);
    return count;
}

int drive_livery_paint(const char *stem, uint8_t *cfm, size_t size, const uint8_t rgb[3])
{
    return drive_livery_paint_masks(stem, NULL, cfm, size, rgb);
}

int drive_livery_paint_masks(const char *stem, const char *mask_dir, uint8_t *cfm, size_t size, const uint8_t rgb[3])
{
    char upper[LIVERY_STEM];
    size_t i = 0;
    for (; stem[i] != '\0' && i < LIVERY_STEM - 1; i++) upper[i] = (char)toupper((unsigned char)stem[i]);
    upper[i] = '\0';
    const MaskSource src = { stem, mask_dir };
    const CarLiveryPass pass = { upper, mask_dir != NULL, 0, load_mask, (void *)&src };
    return car_livery_paint(cfm, size, &pass, rgb);
}

/* ----- the dash art ----- */

enum { DASH_TEXTURES = 64, DASH_BODY_FIRST = 128, DASH_BODY_LAST = 143, PALETTE_HEADER = 0x10 };

static const uint8_t DASH_NAME[4] = { 'd', 'a', 's', 'h' }, DASH_PAL_NAME[4] = { '!', 'p', 'a', 'l' };

static uint32_t word_at(const uint8_t *b) { return (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24; }

/* the bank's palette as cockpit_load finds it: a '!pal' shape, else the one that follows the 'dash' shape's pixels. NULL when there is
 * none, or (port) when the bank is not one the game can read (shape_bank_valid) or that palette runs past it. */
static uint8_t *dash_palette(uint8_t *bank, size_t size)
{
    if (!shape_bank_valid(bank, size)) return NULL;
    uint8_t *shape = shape_bank_find(bank, DASH_PAL_NAME);
    if (shape != NULL) return shape;
    uint8_t *dash = shape_bank_find(bank, DASH_NAME);
    if (dash == NULL) return NULL;
    const size_t at = (size_t)(dash - bank) + (word_at(dash) >> 8);
    return at <= size && size - at >= PALETTE_HEADER + 768 ? bank + at : NULL;
}

int drive_livery_black(const char *stem, const char *mask_dir, uint8_t *cfm, size_t size, const uint8_t rgb[3])
{
    char upper[LIVERY_STEM];
    size_t i = 0;
    for (; stem[i] != '\0' && i < LIVERY_STEM - 1; i++) upper[i] = (char)toupper((unsigned char)stem[i]);
    upper[i] = '\0';
    const MaskSource src = { stem, mask_dir };
    const CarLiveryPass pass = { upper, mask_dir != NULL, 0, load_mask, (void *)&src };
    return car_livery_black(cfm, size, &pass, rgb);
}

typedef struct {
    const uint8_t *cfm;
    LampTexels *out;
} LampJob;

static int run_lamps(const RepaintBank *bank, const BankMeta *meta, void *ctx)
{
    (void)meta;
    const LampJob *j = ctx;
    int added = 0;
    for (int t = 0; t < bank->texture_count; t++) {
        const RepaintTexture *tex = &bank->textures[t];
        if (tex->mask == NULL || tex->pixels < j->cfm) continue;
        const int n = lamp_texels_collect(j->out, (uint32_t)(tex->pixels - j->cfm), tex->mask, tex->count);
        if (n > 0) added += n;
    }
    return added;
}

int drive_livery_lamps(const char *stem, uint8_t *cfm, size_t size, LampTexels *out)
{
    return drive_livery_lamps_masks(stem, NULL, cfm, size, out);
}

int drive_livery_lamps_masks(const char *stem, const char *mask_dir, uint8_t *cfm, size_t size, LampTexels *out)
{
    char upper[LIVERY_STEM];
    size_t i = 0;
    for (; stem[i] != '\0' && i < LIVERY_STEM - 1; i++) upper[i] = (char)toupper((unsigned char)stem[i]);
    upper[i] = '\0';
    const LampJob job = { cfm, out };
    return repaint_cfm(cfm, size, stem, mask_dir, upper, run_lamps, (void *)&job);
}

int drive_livery_dash(const char *stem, uint8_t *bank, size_t size, const uint8_t rgb[3])
{
    char upper[LIVERY_STEM];
    size_t i = 0;
    for (; stem[i] != '\0' && i < LIVERY_STEM - 1; i++) upper[i] = (char)toupper((unsigned char)stem[i]);
    upper[i] = '\0';
    Fsh fsh;
    uint8_t *const pal = dash_palette(bank, size);
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
    const RepaintJob job = { NULL, 0, rgb, car_livery_glass_floor(upper) };
    const int changed = run_paint(&rb, NULL, (void *)&job);
    for (int k = 0; k < n; k++) free(masks[k]);
    return changed > 0 ? changed : 0;
}

/* ----- the drafts of the masks: core/formats/car_livery_pass.h car_livery_draft ----- */

int drive_livery_draft(const char *stem, uint8_t *cfm, size_t size, LiveryDraftFn fn, void *user)
{
    return drive_livery_draft_masks(stem, NULL, cfm, size, fn, user);
}

int drive_livery_draft_masks(const char *stem, const char *mask_dir, uint8_t *cfm, size_t size, LiveryDraftFn fn, void *user)
{
    char upper[LIVERY_STEM];
    size_t i = 0;
    for (; stem[i] != '\0' && i < LIVERY_STEM - 1; i++) upper[i] = (char)toupper((unsigned char)stem[i]);
    upper[i] = '\0';
    const MaskSource src = { stem, mask_dir };
    const CarLiveryPass pass = { upper, mask_dir != NULL, 0, load_mask, (void *)&src };
    return car_livery_draft(cfm, size, &pass, fn, user);
}
