#include "formats/car_livery_pass.h"

#include "formats/car_brake_lamp.h"
#include "formats/car_mask_codec.h"

#include <stdlib.h>
#include <string.h>

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

static void keep_plate_of(RepaintTexture *tex, const char *upper, const char name[4], int width)
{
    for (size_t k = 0; k < sizeof PLATES / sizeof *PLATES; k++)
        if (strcmp(PLATES[k].stem, upper) == 0 && memcmp(PLATES[k].name, name, 4) == 0 && PLATES[k].width == width) {
            tex->width = width;
            memcpy(tex->keep, PLATES[k].rects, sizeof tex->keep);
        }
}

/* The lightness (8 bits) under which a neutral colour is glass or shadow, not paint: a grey body (the NSX's ramp) needs it high, the
 * Diablo's black one low, a coloured body only for its greys. By the car's stem (upper case). */
int car_livery_glass_floor(const char *upper)
{
    if (strcmp(upper, "ANSX") == 0) return 72;
    if (strcmp(upper, "LDIABL") == 0) return 20;
    return 40;
}

/* 1 when `mask` (NULL: none) marks a texel of the `count` as paint */
static int mask_paints(const uint8_t *mask, size_t count)
{
    if (mask == NULL) return 0;
    for (size_t i = 0; i < count; i++)
        if (mask[i] == CAR_MASK_PAINT) return 1;
    return 0;
}

typedef struct {
    const CarLiveryPass *pass;
    CarLiveryBankFn fn;
    void *ctx;
} WalkJob;

/* the part of the walk (formats/car_bank_walk.h) that is the car's: the number plates, and the texture's mask */
static void texture_hook(void *user, int level, const char name[4], int width, int height, RepaintTexture *tex)
{
    const WalkJob *job = user;
    const CarLiveryPass *pass = job->pass;
    const size_t pixels = tex->count;
    keep_plate_of(tex, pass->upper, name, width);
    const int high_side = level == 0 && memcmp(name, "side", 4) == 0, high_back = level == 0 && memcmp(name, "back", 4) == 0;
    if (pass->mask != NULL && (tex->body != REPAINT_NONE || pass->every_texture || pass->skin || high_side || high_back))
        tex->mask = pass->mask(pass->mask_user, name, level, width, height);
    if (tex->body == REPAINT_NONE && pass->skin && mask_paints(tex->mask, pixels))
        tex->body = REPAINT_MAIN;   /* an added car's own texture (car texture add) is the body where its own mask paints */
    if (tex->body == REPAINT_NONE && tex->mask != NULL)   /* a traffic car's high bank has a side (some a back) the catalogue cars have not (car_repaint_body_class leaves them out): a mask makes it a body texture */
        tex->body = high_side ? REPAINT_MAIN : high_back ? REPAINT_END : REPAINT_NONE;
}

static int bank_hook(void *user, const RepaintBank *bank, const CarBankMeta *meta)
{
    const WalkJob *job = user;
    return job->fn(bank, meta, job->ctx);
}

int car_livery_walk(uint8_t *cfm, size_t size, const CarLiveryPass *pass, CarLiveryBankFn bank, void *ctx)
{
    const WalkJob job = { pass, bank, ctx };
    const CarBankHooks hooks = { texture_hook, bank_hook };
    return car_bank_walk(cfm, size, &hooks, (void *)&job);
}

typedef struct {
    const uint8_t *rgb;
    int floor;
} ColourJob;

static int run_paint(const RepaintBank *bank, const CarBankMeta *meta, void *ctx)
{
    (void)meta;
    const ColourJob *j = ctx;
    return car_repaint_paint(bank, j->rgb, j->floor);
}

static int run_black(const RepaintBank *bank, const CarBankMeta *meta, void *ctx)
{
    (void)meta;
    const ColourJob *j = ctx;
    return car_repaint_fixed_black(bank, j->rgb, j->floor);
}

int car_livery_black(uint8_t *cfm, size_t size, const CarLiveryPass *pass, const uint8_t rgb[3])
{
    const ColourJob job = { rgb, car_livery_glass_floor(pass->upper) };
    return car_livery_walk(cfm, size, pass, run_black, (void *)&job);
}

int car_livery_paint(uint8_t *cfm, size_t size, const CarLiveryPass *pass, const uint8_t rgb[3])
{
    const ColourJob job = { rgb, car_livery_glass_floor(pass->upper) };
    return car_livery_walk(cfm, size, pass, run_paint, (void *)&job);
}

/* ----- lamps by the masks ----- */

enum { UNLAMP_RESERVED = 0xfa };   /* the entries from here on are the race's (the police's lights, the lamp's own) */

/* the entry (not a reserved one) whose colour is nearest the unlit lamp colour `six` (6 bits a channel), in the palette's own depth */
static int unlit_entry(const RepaintBank *bank, const uint8_t six[3])
{
    const int shift = bank->type == 0x22 ? 0 : 2;   /* 0x22: 6 bits a channel, 0x24: 8 */
    int best = 0;
    long best_d = -1;
    for (int e = 0; e < UNLAMP_RESERVED; e++) {
        long d = 0;
        for (int k = 0; k < 3; k++) {
            const long diff = (long)bank->palette[0][e * 3 + k] - ((long)six[k] << shift);
            d += diff * diff;
        }
        if (best_d < 0 || d < best_d) best = e, best_d = d;
    }
    return best;
}

static int run_unlamp(const RepaintBank *bank, const CarBankMeta *meta, void *ctx)
{
    (void)meta;
    const int *const row = ctx;
    if (bank->palette[0] == NULL || (bank->type != 0x22 && bank->type != 0x24)) return 0;
    uint8_t six[3];
    car_brake_lamp_colour(*row, 0, six);
    const uint8_t entry = (uint8_t)unlit_entry(bank, six);
    int changed = 0;
    for (int t = 0; t < bank->texture_count; t++) {
        const RepaintTexture *tex = &bank->textures[t];
        if (tex->mask == NULL) continue;
        for (size_t i = 0; i < tex->count; i++) {
            if (tex->pixels[i] != CAR_BRAKE_LAMP_ENTRY || (tex->mask[i] >= CAR_MASK_BRAKE && tex->mask[i] < CAR_MASK_BRAKE + 4)) continue;
            tex->pixels[i] = entry, changed++;
        }
    }
    return changed;
}

int car_livery_unlamp(uint8_t *cfm, size_t size, const CarLiveryPass *pass)
{
    int row = car_brake_lamp_row(pass->upper);
    return car_livery_walk(cfm, size, pass, run_unlamp, &row);
}

/* ----- the drafts of the masks: the race and the Studio's 3D view shine the same texels ----- */

typedef struct {
    CarLiveryDraftFn fn;
    void *user;
    int floor;
} DraftJob;

static int run_draft(const RepaintBank *bank, const CarBankMeta *meta, void *ctx)
{
    const DraftJob *j = ctx;
    uint8_t *keeps[CAR_BANK_MAX_TEXTURES] = { NULL };
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

int car_livery_draft(uint8_t *cfm, size_t size, const CarLiveryPass *pass, CarLiveryDraftFn fn, void *user)
{
    /* the Corvette's dark teal body lies under its runtime floor (a lower one repaints eight lamp texels): the draft starts its fascia as paint */
    const DraftJob job = { fn, user, strcmp(pass->upper, "CZR1") == 0 ? 20 : car_livery_glass_floor(pass->upper) };
    return car_livery_walk(cfm, size, pass, run_draft, (void *)&job);
}
