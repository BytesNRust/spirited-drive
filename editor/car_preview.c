#include "car_preview.h"

#include "formats/car_bank_walk.h"
#include "game/race/car_mask.h"
#include "game/race/car_paint.h"
#include "game/sys/palette.h"
#include "game/viewer/drive_livery.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

enum { PATH = 4096, SCREEN_FIRST = 0xfa, TRANSPARENT_INDEX = 0xff, OWN_INDEX = 0xfe };

static uint8_t *read_file(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    uint8_t *buf = NULL;
    size_t n = 0, cap = 0;
    for (;;) {
        if (n == cap) {
            cap = cap ? cap * 2 : 1 << 16;
            uint8_t *bigger = realloc(buf, cap);
            if (bigger == NULL) { free(buf); fclose(f); return NULL; }
            buf = bigger;
        }
        const size_t got = fread(buf + n, 1, cap - n, f);
        n += got;
        if (got == 0) break;
    }
    fclose(f);
    *len = n;
    return buf;
}

static int write_file(const char *path, const uint8_t *data, size_t len)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL) return 0;
    const int ok = fwrite(data, 1, len, f) == len;
    return fclose(f) == 0 && ok;
}

static const char *leaf(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash != NULL ? slash + 1 : path;
}

/* the masks of the project (and the edit not yet saved) as the folder the game's override reads: <root>/<stem>/<tex>_mask.png */
static int spill_masks(const PreviewIn *in, const char *dir)
{
    if (in->masks_dir != NULL) {
        DIR *d = opendir(in->masks_dir);
        struct dirent *de;
        while (d != NULL && (de = readdir(d)) != NULL) {
            const size_t n = strlen(de->d_name);
            if (n < 10 || strcmp(de->d_name + n - 9, "_mask.png") != 0) continue;
            char from[PATH], to[PATH];
            snprintf(from, sizeof from, "%s/%s", in->masks_dir, de->d_name);
            snprintf(to, sizeof to, "%s/%s", dir, de->d_name);
            size_t len = 0;
            uint8_t *b = read_file(from, &len);
            if (b != NULL) write_file(to, b, len);
            free(b);
        }
        if (d != NULL) closedir(d);
    }
    if (in->edit != NULL && in->edit->now != NULL) {
        uint8_t *png = NULL;
        size_t len = 0;
        char to[PATH];
        snprintf(to, sizeof to, "%s/%s", dir, leaf(in->edit->path));
        if (!mask_edit_render(in->edit, &png, &len)) return 0;
        const int ok = write_file(to, png, len);
        free(png);
        return ok;
    }
    return 1;
}

static void remove_spill(const char *root, const char *dir)
{
    DIR *d = opendir(dir);
    struct dirent *de;
    while (d != NULL && (de = readdir(d)) != NULL) {
        const size_t n = strlen(de->d_name);
        if (n < 10 || strcmp(de->d_name + n - 9, "_mask.png") != 0) continue;   /* only what this made */
        char p[PATH];
        snprintf(p, sizeof p, "%s/%s", dir, de->d_name);
        remove(p);
    }
    if (d != NULL) closedir(d);
    rmdir(dir);
    rmdir(root);
}

typedef struct {
    CarPreview *out;
    int level_count[2];       /* textures seen by the hook in each bank, in the walk's order */
    int widths[2][CAR_BANK_MAX_TEXTURES], heights[2][CAR_BANK_MAX_TEXTURES];
    uint8_t seen[1 << 15];    /* the distinct body colours: a bitmap of 6 bits a channel */
    const char *stem, *skin;  /* how the masks are looked up while the scratch folder is up: by stem through TNFS_CAR_MASKS, or in the skin's folder */
    int ok;
} Collect;

static void on_texture(void *user, int level, const char name[4], int width, int height, RepaintTexture *tex)
{
    (void)name, (void)tex;
    Collect *c = user;
    const int k = c->level_count[level]++;
    if (k < CAR_BANK_MAX_TEXTURES) c->widths[level][k] = width, c->heights[level][k] = height;
}

/* a texel the screen palette's own entries stand for (not a colour of the bank): drawn as a pattern, so that nobody takes the bank's colour for it */
static void special_texel(uint8_t index, int x, int y, uint8_t rgba[4])
{
    const int on = ((x >> 1) + (y >> 1)) & 1;
    static const uint8_t POLICE[3] = { 255, 0, 130 };   /* the police lights' entries (0xfa..0xfd): hatched magenta */
    static const uint8_t OWN[3] = { 120, 200, 120 };    /* 0xfe: the car's own slot colour */
    const uint8_t *c = index == OWN_INDEX ? OWN : POLICE;
    if (index == TRANSPARENT_INDEX) {   /* 0xff: transparent in a polygon of mode 1, black in mode 0: a chequer */
        rgba[0] = rgba[1] = rgba[2] = on ? 70 : 30;
    } else {
        for (int k = 0; k < 3; k++) rgba[k] = on ? c[k] : (uint8_t)(c[k] / 3);
    }
    rgba[3] = 255;
}

static int on_bank(void *user, const RepaintBank *bank, const CarBankMeta *meta)
{
    Collect *c = user;
    CarPreview *out = c->out;
    uint8_t rgb[256 * 3];
    const uint8_t *pal = bank->palette[0];
    if (bank->type == 0x24) {   /* an 8-bit palette: the race takes it to 6 bits a channel */
        uint8_t six[768];
        for (int i = 0; i < 768; i++) six[i] = (uint8_t)(pal[i] >> 2);
        palette_6_to_8(256, six, rgb);
    } else {
        palette_6_to_8(256, pal, rgb);
    }
    for (int k = 0; k < bank->texture_count && k < CAR_BANK_MAX_TEXTURES; k++) {
        const RepaintTexture *t = &bank->textures[k];
        const int w = c->widths[meta->level][k], h = c->heights[meta->level][k];
        if (w <= 0 || h <= 0 || t->count < (size_t)w * h || out->count >= MASK_TEX_LIST) continue;
        PreviewTex *pt = &out->tex[out->count];
        memset(pt, 0, sizeof *pt);
        pt->rgba = malloc((size_t)w * h * 4);
        pt->index = malloc((size_t)w * h);
        if (pt->rgba == NULL || pt->index == NULL) { free(pt->rgba); free(pt->index); pt->rgba = pt->index = NULL; c->ok = 0; continue; }
        memcpy(pt->index, t->pixels, (size_t)w * h);
        pt->mask = c->skin != NULL ? car_mask_load_dir(c->skin, meta->name[k], meta->level, w, h) : car_mask_load(c->stem, meta->name[k], meta->level, w, h);   /* as the race read it, the unsaved edit in place of its file */
        memcpy(out->palette[meta->level], rgb, 768);
        pt->info.level = meta->level;
        int n = 0;
        for (int i = 0; i < 4 && meta->name[k][i] != 0 && meta->name[k][i] != ' '; i++) pt->info.name[n++] = meta->name[k][i];
        pt->info.name[n] = '\0';
        snprintf(pt->info.file, sizeof pt->info.file, meta->level == 0 ? "%s_mask.png" : "%s_lo_mask.png", pt->info.name);
        pt->info.width = w;
        pt->info.height = h;
        pt->body = t->body != REPAINT_NONE;
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                const uint8_t index = t->pixels[(size_t)y * w + x];
                uint8_t *px = pt->rgba + ((size_t)y * w + x) * 4;
                if (index >= SCREEN_FIRST) {
                    special_texel(index, x, y, px);
                    out->special++;
                    continue;
                }
                memcpy(px, rgb + index * 3, 3);
                px[3] = 255;
                if (pt->body && meta->level == 0) {   /* the body's distinct colours: one bit for each of the 6-bit colours */
                    const int r6 = (pal[index * 3] >> (bank->type == 0x24 ? 2 : 0)) & 63, g6 = (pal[index * 3 + 1] >> (bank->type == 0x24 ? 2 : 0)) & 63,
                              b6 = (pal[index * 3 + 2] >> (bank->type == 0x24 ? 2 : 0)) & 63;
                    const int bit = (r6 << 12) | (g6 << 6) | b6;
                    if (!(c->seen[bit >> 3] & (1 << (bit & 7)))) {
                        c->seen[bit >> 3] |= (uint8_t)(1 << (bit & 7));
                        out->body_colours++;
                    }
                }
            }
        out->count++;
    }
    return 0;
}

static void say_colours(CarPreview *p)
{
    const int own = PREVIEW_OWN_ENTRIES, shared = PREVIEW_SHARED_ENTRIES;
    if (p->body_colours <= own)
        snprintf(p->note, sizeof p->note, "The body shows %d colours; a car has %d entries of its own in the race palette, so they all fit. (The track's light and the screen palette are not applied here.)", p->body_colours, own);
    else
        snprintf(p->note, sizeof p->note, "The body shows %d colours but a car has only %d entries of its own in the race palette (%d shared when several cars race): at least %d of the body's colours will look like their nearest. (The track's light and the screen palette are not applied here.)",
                 p->body_colours, own, shared, p->body_colours - own);
}

/* the painted file, and (`out` not NULL) the preview of it read while the scratch masks are still up */
static int paint_and_walk(const PreviewIn *in, uint8_t **out_cfm, size_t *out_len, CarPreview *out, int *blacked, int *painted, char *why, size_t why_cap)
{
    size_t size = 0;
    uint8_t *cfm = read_file(in->cfm, &size);
    if (cfm == NULL) {
        snprintf(why, why_cap, "Cannot read the car file %s", in->cfm);
        return 0;
    }
    const char *tmp = getenv("TMPDIR");
    char root[PATH], dir[PATH];
    snprintf(root, sizeof root, "%s/studio-preview-XXXXXX", tmp != NULL && tmp[0] != '\0' ? tmp : "/tmp");
    if (mkdtemp(root) == NULL) {
        free(cfm);
        snprintf(why, why_cap, "Cannot make a scratch folder for the masks");
        return 0;
    }
    snprintf(dir, sizeof dir, "%s/%s", root, in->stem);
    if (mkdir(dir, 0700) != 0 || !spill_masks(in, dir)) {
        remove_spill(root, dir);
        free(cfm);
        snprintf(why, why_cap, "Cannot prepare the masks for the preview");
        return 0;
    }
    char *old = getenv("TNFS_CAR_MASKS") != NULL ? strdup(getenv("TNFS_CAR_MASKS")) : NULL;
    setenv("TNFS_CAR_MASKS", root, 1);
    const CarPaint *black = car_paint_black_of(in->stem);
    const uint8_t added_black[3] = CAR_PAINT_ADDED_BLACK;
    const char *const skin = in->added ? dir : NULL;   /* an added car's masks are a skin's folder: read from it alone */
    int nb = 0, np = 0, walked = 1;
    if (black != NULL || in->added) nb = drive_livery_black(in->stem, skin, cfm, size, black != NULL ? black->rgb : added_black);   /* as the race does, before any paint */
    if (in->paint != NULL) np = in->added ? drive_livery_paint_masks(in->stem, skin, cfm, size, in->paint) : drive_livery_paint(in->stem, cfm, size, in->paint);
    if (out != NULL) {
        Collect *c = calloc(1, sizeof *c);
        walked = c != NULL;
        if (walked) {
            c->out = out;
            c->ok = 1;
            c->stem = in->stem;
            c->skin = skin;
            const CarBankHooks hooks = { on_texture, on_bank };
            car_bank_walk(cfm, size, &hooks, c);
            walked = c->ok;
        }
        free(c);
    }
    if (old != NULL) setenv("TNFS_CAR_MASKS", old, 1);
    else unsetenv("TNFS_CAR_MASKS");
    free(old);
    remove_spill(root, dir);
    if (blacked != NULL) *blacked = nb;
    if (painted != NULL) *painted = np;
    if (!walked) {
        free(cfm);
        snprintf(why, why_cap, "Out of memory making the preview");
        return 0;
    }
    if (out_cfm != NULL) *out_cfm = cfm, *out_len = size;
    else free(cfm);
    return 1;
}

int car_preview_painted_cfm(const PreviewIn *in, uint8_t **out_cfm, size_t *out_len, int *blacked, int *painted, char *why, size_t why_cap)
{
    return paint_and_walk(in, out_cfm, out_len, NULL, blacked, painted, why, why_cap);
}

int car_preview_make(const PreviewIn *in, CarPreview *out, char *why, size_t why_cap)
{
    memset(out, 0, sizeof *out);
    if (!paint_and_walk(in, NULL, NULL, out, &out->blacked, &out->painted, why, why_cap)) {
        car_preview_free(out);
        return 0;
    }
    if (out->count == 0) {
        car_preview_free(out);
        snprintf(why, why_cap, "The car file %s has no textures the preview can read", leaf(in->cfm));
        return 0;
    }
    say_colours(out);
    return 1;
}

void car_preview_free(CarPreview *p)
{
    for (int i = 0; i < p->count; i++) free(p->tex[i].rgba), free(p->tex[i].index), free(p->tex[i].mask);
    memset(p, 0, sizeof *p);
}
