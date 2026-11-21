/* The mask editor's model (editor/mask_edit.c): class mapping, pencil, line, fill by class and by texture, the eyedropper, undo and redo, the size note, Save (same size, only the
 * edit changed, nothing written when nothing changed), and the round trip of every mask of the repo's data/car_masks. argv[1]: the repo's data/; argv[2] (optional): the player's
 * game files, for the textures of a real CFM. */
#include "mask_edit.h"
#include "mask_tex.h"
#include "formats/car_mask_codec.h"
#include "formats/png.h"
#include "test.h"

#include <dirent.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char base[512];

static void put(const char *name, const uint8_t *px, int w, int h)
{
    uint8_t *png = NULL;
    size_t n = 0;
    char p[1024];
    snprintf(p, sizeof p, "%s/%s", base, name);
    if (!car_mask_write_grey(px, w, h, &png, &n)) { CHECK(0); return; }
    FILE *f = fopen(p, "wb");
    fwrite(png, 1, n, f);
    fclose(f);
    png_free(png);
}

static uint8_t *slurp(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    *len = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc(*len ? *len : 1);
    if (fread(b, 1, *len, f) != *len) { free(b); fclose(f); return NULL; }
    fclose(f);
    return b;
}

static void check_classes(void)
{
    CHECK_EQ_INT(ME_CLASSES, 9);
    const uint8_t v[ME_CLASSES] = { 0, 128, 192, 224, 240, 244, 248, 252, 255 };
    for (int i = 0; i < ME_CLASSES; i++) {
        CHECK_EQ_INT(mask_edit_class(i)->value, v[i]);
        CHECK_EQ_INT(mask_edit_class_index(v[i]), i);
        CHECK(mask_edit_class(i)->name[0] != 0);
    }
    CHECK_EQ_INT(mask_edit_class_index(37), 8);    /* an unknown non-zero value is matte, as the game reads it */
    CHECK(mask_edit_class(-1) == NULL && mask_edit_class(ME_CLASSES) == NULL);
}

/* a 8x6 mask: left half paint, right half matte, one glass texel; a 4x3 texture of four colours in quarters */
static void check_model(void)
{
    uint8_t m[8 * 6];
    for (int y = 0; y < 6; y++)
        for (int x = 0; x < 8; x++) m[y * 8 + x] = x < 4 ? 0 : 255;
    m[2 * 8 + 1] = 128;
    put("a_mask.png", m, 8, 6);
    uint8_t tex[4 * 3 * 3];
    for (int i = 0; i < 12; i++) { tex[i * 3] = (uint8_t)((i % 4) < 2 ? 10 : 200); tex[i * 3 + 1] = (uint8_t)(i < 6 ? 20 : 90); tex[i * 3 + 2] = 5; }
    char p[1024], why[256] = "";
    snprintf(p, sizeof p, "%s/a_mask.png", base);
    MaskEdit e;
    CHECK(mask_edit_open(&e, p, 4, 3, tex, why, sizeof why));
    CHECK_EQ_INT(e.width, 8);
    CHECK_EQ_INT(e.height, 6);
    CHECK_EQ_INT(mask_edit_class_at(&e, 0, 0), 0);
    CHECK_EQ_INT(mask_edit_class_at(&e, 1, 2), 1);     /* the eyedropper reads the class */
    CHECK_EQ_INT(mask_edit_class_at(&e, 7, 5), 8);
    CHECK_EQ_INT(mask_edit_class_at(&e, 8, 0), -1);
    CHECK_EQ_INT(mask_edit_class_at(&e, -1, 0), -1);
    CHECK(!mask_edit_modified(&e));
    CHECK(!mask_edit_can_undo(&e) && !mask_edit_can_redo(&e));
    char note[256];
    CHECK(mask_edit_size_note(&e, note, sizeof note));   /* both sizes, in the sentence */
    CHECK(strstr(note, "8x6") != NULL && strstr(note, "4x3") != NULL && strstr(note, "scales") != NULL);
    uint8_t rgb[3];
    CHECK(mask_edit_texture_at(&e, 0, 0, rgb));
    CHECK_EQ_INT(rgb[0], 10);
    CHECK(mask_edit_texture_at(&e, 7, 5, rgb));
    CHECK_EQ_INT(rgb[0], 200);
    CHECK(!mask_edit_texture_at(&e, 9, 0, rgb));

    /* pencil of 1 */
    CHECK_EQ_INT(mask_edit_pencil(&e, 5, 5, 1, 248), 1);
    CHECK_EQ_INT(mask_edit_class_at(&e, 5, 5), 6);
    CHECK(mask_edit_modified(&e));
    CHECK_EQ_INT(mask_edit_pencil(&e, 5, 5, 1, 248), 0);   /* the same again changes nothing and is no step */
    CHECK_EQ_INT(e.undo_count, 1);
    /* pencil of 3 centred: 9 texels, the edge ones clipped */
    CHECK_EQ_INT(mask_edit_pencil(&e, 7, 0, 3, 252), 4);
    CHECK_EQ_INT(mask_edit_class_at(&e, 6, 0), 7);
    CHECK_EQ_INT(mask_edit_class_at(&e, 7, 1), 7);
    CHECK_EQ_INT(mask_edit_class_at(&e, 5, 0), 8);
    /* a value that is no class of the legend is written as matte, as the game reads it */
    mask_edit_pencil(&e, 0, 5, 1, 241);
    CHECK_EQ_INT(e.now[5 * 8], 255);
    /* a brush past the limits is held to them */
    CHECK_EQ_INT(mask_edit_pencil(&e, 3, 3, 20, 192), 48);
    /* undo each, back to the file's state */
    while (mask_edit_can_undo(&e)) CHECK(mask_edit_undo(&e));
    CHECK(!mask_edit_modified(&e));
    CHECK_MEM(e.now, m, sizeof m);
    CHECK(mask_edit_redo(&e));
    CHECK_EQ_INT(mask_edit_class_at(&e, 5, 5), 6);
    CHECK(mask_edit_undo(&e));

    /* a stroke is one step */
    mask_edit_stroke_begin(&e);
    mask_edit_line(&e, 0, 0, 7, 0, 1, 224);
    mask_edit_pencil(&e, 0, 1, 1, 224);
    mask_edit_stroke_end(&e);
    CHECK_EQ_INT(e.undo_count, 1);
    CHECK_EQ_INT(mask_edit_class_at(&e, 7, 0), 3);
    CHECK_EQ_INT(mask_edit_class_at(&e, 0, 1), 3);
    CHECK(mask_edit_undo(&e));
    CHECK_MEM(e.now, m, sizeof m);
    /* a stroke that changes nothing leaves no step */
    mask_edit_stroke_begin(&e);
    mask_edit_pencil(&e, 0, 0, 1, 0);
    mask_edit_stroke_end(&e);
    CHECK(!mask_edit_can_undo(&e));
    /* two strokes of one class are two steps */
    mask_edit_stroke_begin(&e); mask_edit_pencil(&e, 0, 0, 1, 224); mask_edit_stroke_end(&e);
    mask_edit_stroke_begin(&e); mask_edit_pencil(&e, 1, 0, 1, 224); mask_edit_stroke_end(&e);
    CHECK_EQ_INT(e.undo_count, 2);
    mask_edit_undo(&e); mask_edit_undo(&e);
    /* a drag: each texel the pointer reaches shows at once, so every call of the stroke that changes a texel raises the version the picture is rebuilt by
     * (the owner's drag drew nothing until the next click) */
    mask_edit_stroke_begin(&e);
    unsigned seen = e.version;
    mask_edit_pencil(&e, 0, 2, 1, 224);
    CHECK(e.version != seen);
    seen = e.version;
    CHECK_EQ_INT(mask_edit_line(&e, 0, 2, 3, 2, 1, 224), 3);
    CHECK(e.version != seen);
    seen = e.version;
    mask_edit_line(&e, 3, 2, 3, 3, 1, 224);
    CHECK(e.version != seen);
    seen = e.version;
    CHECK_EQ_INT(mask_edit_pencil(&e, 3, 3, 1, 224), 0);
    CHECK(e.version == seen);   /* a texel already that class: nothing to rebuild */
    mask_edit_stroke_end(&e);
    CHECK_EQ_INT(e.undo_count, 1);   /* still one step to undo */
    mask_edit_undo(&e);
    CHECK_MEM(e.now, m, sizeof m);

    /* fill by class: the paint half (the glass texel is not paint, but 4-connected round it) */
    const int n = mask_edit_fill(&e, 0, 0, 192, ME_FILL_CLASS);
    CHECK_EQ_INT(n, 4 * 6 - 1);
    CHECK_EQ_INT(mask_edit_class_at(&e, 1, 2), 1);     /* the glass stayed */
    CHECK_EQ_INT(mask_edit_class_at(&e, 4, 0), 8);     /* the matte half too */
    CHECK_EQ_INT(mask_edit_fill(&e, 0, 0, 192, ME_FILL_CLASS), 0);   /* nothing to change */
    CHECK_EQ_INT(e.undo_count, 1);
    CHECK(mask_edit_undo(&e));
    CHECK_MEM(e.now, m, sizeof m);
    /* fill by texture colour: the texture's top-left 2x1 texels are alike (colour x<2, row 0), each covering 2x2 mask texels: 4 x 2 mask texels from (0,0) */
    const int t = mask_edit_fill(&e, 0, 0, 244, ME_FILL_TEXTURE);
    CHECK_EQ_INT(t, 4 * 4);   /* columns 0..3, rows 0..3: the texture's two top rows share their colour; the glass texel is in the region too, a fill by colour takes any class */
    CHECK_EQ_INT(mask_edit_class_at(&e, 3, 3), 5);
    CHECK(mask_edit_undo(&e));
    CHECK(mask_edit_redo(&e));
    CHECK(mask_edit_undo(&e));
    mask_edit_undo(&e);

    /* edits after an undo drop the redo */
    mask_edit_pencil(&e, 2, 2, 1, 252);
    mask_edit_undo(&e);
    CHECK(mask_edit_can_redo(&e));
    mask_edit_pencil(&e, 3, 3, 1, 252);
    CHECK(!mask_edit_can_redo(&e));
    mask_edit_undo(&e);

    /* Save: nothing changed writes nothing */
    size_t l0, l1;
    uint8_t *before = slurp(p, &l0);
    CHECK(mask_edit_save(&e, why, sizeof why));
    uint8_t *after = slurp(p, &l1);
    CHECK(l0 == l1 && memcmp(before, after, l0) == 0);
    free(before);
    free(after);
    /* an edit, Save, reopen: the same size, the edited texel, every other texel as it was */
    mask_edit_pencil(&e, 6, 4, 1, 252);
    CHECK(mask_edit_modified(&e));
    CHECK(mask_edit_save(&e, why, sizeof why));
    CHECK(!mask_edit_modified(&e));
    MaskEdit r;
    CHECK(mask_edit_open(&r, p, 0, 0, NULL, why, sizeof why));
    CHECK(r.width == 8 && r.height == 6);
    uint8_t want[8 * 6];
    memcpy(want, m, sizeof want);
    want[4 * 8 + 6] = 252;
    CHECK_MEM(r.now, want, sizeof want);
    CHECK(!mask_edit_size_note(&r, note, sizeof note));   /* no texture: no note */
    mask_edit_free(&r);
    mask_edit_free(&e);
    snprintf(p, sizeof p, "%s/a_mask.png.saving", base);
    CHECK(access(p, F_OK) != 0);   /* the temporary file is gone */

    /* the same size as the texture: no note */
    uint8_t tex2[8 * 6 * 3];
    memset(tex2, 7, sizeof tex2);
    snprintf(p, sizeof p, "%s/a_mask.png", base);
    CHECK(mask_edit_open(&e, p, 8, 6, tex2, why, sizeof why));
    CHECK(!mask_edit_size_note(&e, note, sizeof note));
    CHECK_EQ_INT(mask_edit_fill(&e, 0, 0, 192, ME_FILL_TEXTURE), 48);   /* one colour: the whole mask */
    mask_edit_free(&e);
}

/* the picture of the mask over its texture: the texture's colour where no class is shown, the legend's colour blended where it is */
static void check_compose(void)
{
    uint8_t m[4] = { 0, 128, 255, 252 };
    put("c_mask.png", m, 2, 2);
    uint8_t tex[2 * 2 * 3];
    for (int i = 0; i < 4; i++) tex[i * 3] = tex[i * 3 + 1] = tex[i * 3 + 2] = 100;
    char p[1024], why[256];
    snprintf(p, sizeof p, "%s/c_mask.png", base);
    MaskEdit e;
    CHECK(mask_edit_open(&e, p, 2, 2, tex, why, sizeof why));
    uint8_t on[ME_CLASSES], rgba[2 * 2 * 4];
    memset(on, 1, sizeof on);
    mask_edit_compose(&e, on, 0, 0.5f, rgba);   /* overlay off: the texture itself */
    CHECK(rgba[0] == 100 && rgba[4] == 100 && rgba[3] == 255);
    mask_edit_compose(&e, on, 1, 0.0f, rgba);   /* no opacity: the same */
    CHECK(rgba[0] == 100 && rgba[8] == 100);
    mask_edit_compose(&e, on, 1, 1.0f, rgba);   /* full: the legend's colour */
    CHECK(rgba[4] == mask_edit_class(1)->rgb[0] && rgba[5] == mask_edit_class(1)->rgb[1] && rgba[6] == mask_edit_class(1)->rgb[2]);
    CHECK(rgba[12] == mask_edit_class(7)->rgb[0]);
    on[1] = 0;   /* a class switched off shows the texture */
    mask_edit_compose(&e, on, 1, 1.0f, rgba);
    CHECK(rgba[4] == 100 && rgba[0] == mask_edit_class(0)->rgb[0]);
    const unsigned v = e.version;
    mask_edit_pencil(&e, 0, 0, 1, 248);
    CHECK(e.version != v);   /* a change shows in the version the picture is rebuilt by */
    mask_edit_undo(&e);
    CHECK(e.version != v);
    mask_edit_free(&e);
    CHECK(mask_edit_open(&e, p, 0, 0, NULL, why, sizeof why));   /* no texture: grey under the overlay */
    mask_edit_compose(&e, on, 0, 0.5f, rgba);
    CHECK_EQ_INT(rgba[0], 40);
    mask_edit_free(&e);
}

/* the texel under the pointer: a missing mouse position (-FLT_MAX, which ImGui uses) and other non-positions are no texel, and nothing is cast out of range (UBSan halts on it) */
static void check_texel_under(void)
{
    int x = 7, y = 7;
    CHECK(mask_edit_texel_under(110.0f, 125.0f, 100.0f, 100.0f, 4.0f, 8, 8, &x, &y) && x == 2 && y == 6);
    CHECK(mask_edit_texel_under(100.0f, 100.0f, 100.0f, 100.0f, 4.0f, 8, 8, &x, &y) && x == 0 && y == 0);   /* the first texel's corner */
    CHECK(!mask_edit_texel_under(132.0f, 100.0f, 100.0f, 100.0f, 4.0f, 8, 8, &x, &y) && x == -1 && y == -1);   /* the right edge is outside */
    CHECK(!mask_edit_texel_under(99.9f, 110.0f, 100.0f, 100.0f, 4.0f, 8, 8, &x, &y));
    CHECK(!mask_edit_texel_under(-FLT_MAX, -FLT_MAX, 100.0f, 100.0f, 4.0f, 8, 8, &x, &y) && x == -1 && y == -1);   /* no mouse position */
    CHECK(!mask_edit_texel_under(FLT_MAX, FLT_MAX, 100.0f, 100.0f, 4.0f, 8, 8, &x, &y));
    CHECK(!mask_edit_texel_under(-FLT_MAX, 100.0f, 100.0f, 100.0f, 0.001f, 8, 8, &x, &y));
    CHECK(!mask_edit_texel_under(NAN, 100.0f, 100.0f, 100.0f, 4.0f, 8, 8, &x, &y));
    CHECK(!mask_edit_texel_under(INFINITY, 100.0f, 100.0f, 100.0f, 4.0f, 8, 8, &x, &y));
    CHECK(!mask_edit_texel_under(110.0f, 110.0f, 100.0f, 100.0f, 0.0f, 8, 8, &x, &y));   /* a scale of nothing */
    CHECK(!mask_edit_texel_under(110.0f, 110.0f, 100.0f, 100.0f, -1.0f, 8, 8, &x, &y));
    CHECK(!mask_edit_texel_under(110.0f, 110.0f, -FLT_MAX, 100.0f, 4.0f, 8, 8, &x, &y));
}

static void check_refusals(void)
{
    char why[256] = "", p[1024];
    MaskEdit e;
    snprintf(p, sizeof p, "%s/none_mask.png", base);
    CHECK(!mask_edit_open(&e, p, 0, 0, NULL, why, sizeof why));
    CHECK(strstr(why, "Cannot read") != NULL);
    snprintf(p, sizeof p, "%s/junk_mask.png", base);
    FILE *f = fopen(p, "wb");
    fputs("not a png", f);
    fclose(f);
    CHECK(!mask_edit_open(&e, p, 0, 0, NULL, why, sizeof why));
    CHECK(strstr(why, "not a mask") != NULL);
    /* a fill by texture without a texture does nothing */
    uint8_t m[4] = { 0, 0, 255, 255 };
    put("b_mask.png", m, 2, 2);
    snprintf(p, sizeof p, "%s/b_mask.png", base);
    CHECK(mask_edit_open(&e, p, 0, 0, NULL, why, sizeof why));
    CHECK_EQ_INT(mask_edit_fill(&e, 0, 0, 192, ME_FILL_TEXTURE), 0);
    CHECK(!mask_edit_can_undo(&e));
    mask_edit_free(&e);
}

/* every mask of the repo: open, Save with no edit leaves the file's bytes; one texel edited and saved changes that texel only */
static void check_repo(const char *data)
{
    char root[1024];
    snprintf(root, sizeof root, "%s/car_masks", data);
    DIR *d = opendir(root);
    if (d == NULL) { CHECK(0); return; }
    int masks = 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;
        char sub[1024];
        snprintf(sub, sizeof sub, "%s/%s", root, de->d_name);
        DIR *s = opendir(sub);
        if (s == NULL) continue;
        struct dirent *fe;
        while ((fe = readdir(s)) != NULL) {
            const size_t n = strlen(fe->d_name);
            if (n < 9 || strcmp(fe->d_name + n - 9, "_mask.png") != 0) continue;
            char src[1024], dst[1024], why[256] = "";
            snprintf(src, sizeof src, "%s/%s", sub, fe->d_name);
            snprintf(dst, sizeof dst, "%s/copy.png", base);
            size_t len;
            uint8_t *orig = slurp(src, &len);
            if (orig == NULL) { CHECK(0); continue; }
            FILE *o = fopen(dst, "wb");
            fwrite(orig, 1, len, o);
            fclose(o);
            MaskEdit e;
            if (!mask_edit_open(&e, dst, 0, 0, NULL, why, sizeof why)) { CHECK(0); fprintf(stderr, "%s: %s\n", src, why); free(orig); continue; }
            masks++;
            CHECK(mask_edit_save(&e, why, sizeof why));
            size_t l2;
            uint8_t *same = slurp(dst, &l2);
            CHECK(l2 == len && memcmp(same, orig, len) == 0);   /* no edit, the file's own bytes */
            free(same);
            const size_t px = (size_t)e.width * e.height, at = px / 3;
            uint8_t *copy = malloc(px);
            memcpy(copy, e.now, px);
            const uint8_t old = e.now[at];
            const uint8_t put_cls = old == 252 ? 255 : 252;
            mask_edit_pencil(&e, (int)(at % e.width), (int)(at / e.width), 1, put_cls);
            CHECK(mask_edit_save(&e, why, sizeof why));
            mask_edit_free(&e);
            MaskEdit r;
            CHECK(mask_edit_open(&r, dst, 0, 0, NULL, why, sizeof why));
            CHECK(r.width * r.height == (int)px);
            copy[at] = put_cls;
            CHECK(memcmp(r.now, copy, px) == 0);
            mask_edit_free(&r);
            free(copy);
            free(orig);
        }
        closedir(s);
    }
    closedir(d);
    CHECK(masks >= 100);
}

static void check_textures(const char *cd, const char *data)
{
    char cfm[1024], mask[1024];
    snprintf(cfm, sizeof cfm, "%s/SIMDATA/CARFAMS/P911.CFM", cd);
    if (access(cfm, R_OK) != 0) { fprintf(stderr, "no game files at %s: the texture checks are skipped\n", cd); return; }
    MaskTexInfo list[MASK_TEX_LIST];
    const int n = mask_tex_list(cfm, list, MASK_TEX_LIST);
    CHECK(n > 4);
    int found = 0;
    for (int i = 0; i < n; i++) {
        if (list[i].level != 0 || strcmp(list[i].name, "topv") != 0) continue;
        found = 1;
        CHECK_EQ_INT(list[i].width * list[i].height > 0, 1);
        uint8_t *rgb = NULL;
        int w = 0, h = 0;
        CHECK(mask_tex_rgb(cfm, 0, "topv", &w, &h, &rgb));
        CHECK(w == list[i].width && h == list[i].height);
        snprintf(mask, sizeof mask, "%s/car_masks/P911/%s", data, list[i].file);
        char why[256];
        MaskEdit e;
        CHECK(mask_edit_open(&e, mask, w, h, rgb, why, sizeof why));
        char note[256];
        fprintf(stderr, "P911 topv: mask %dx%d, texture %dx%d, note: %s\n", e.width, e.height, w, h, mask_edit_size_note(&e, note, sizeof note) ? note : "none");
        mask_edit_free(&e);
        free(rgb);
    }
    CHECK(found);
    uint8_t *none = NULL;
    int w, h;
    CHECK(!mask_tex_rgb(cfm, 0, "zzzz", &w, &h, &none));
    CHECK(!mask_tex_rgb("/nonexistent.CFM", 0, "topv", &w, &h, &none));
    CHECK_EQ_INT(mask_tex_list("/nonexistent.CFM", list, MASK_TEX_LIST), 0);
}

/* the mask view's zoom: the keys and the wheel step through the zooms, and every zoom (the slider's too) keeps the texel at the view's middle where it was */
static void check_zoom(void)
{
    CHECK(mask_edit_zoom_step(1.0f, 1) == 1.5f);
    CHECK(mask_edit_zoom_step(4.0f, 1) == 6.0f);
    CHECK(mask_edit_zoom_step(5.0f, 1) == 6.0f);   /* a slider's zoom between steps goes to the next */
    CHECK(mask_edit_zoom_step(5.0f, -1) == 4.0f);
    CHECK(mask_edit_zoom_step(16.0f, 1) == 16.0f);   /* held to the ends */
    CHECK(mask_edit_zoom_step(1.0f, -1) == 1.0f);
    CHECK(mask_edit_zoom_step(0.0f, 1) == 1.5f);
    /* a 400-pixel view scrolled 600 at 4x shows texel (600 + 200) / 4 = 200 in its middle; at 8x that texel is at 1600, so the scroll is 1400 */
    CHECK(mask_edit_zoom_scroll(600.0f, 400.0f, 4.0f, 8.0f) == 1400.0f);
    CHECK(mask_edit_zoom_scroll(1400.0f, 400.0f, 8.0f, 4.0f) == 600.0f);   /* and back */
    CHECK(mask_edit_zoom_scroll(0.0f, 400.0f, 4.0f, 2.0f) == 0.0f);   /* never before the drawing's start */
    CHECK(mask_edit_zoom_scroll(10.0f, 400.0f, 0.0f, 2.0f) == 0.0f);   /* no scale: no scroll */
    /* the wheel: a notch is a step; a trackpad's small deltas a frame add up to one step a whole notch, not one a frame (the owner's two-finger zoom ran away) */
    float acc = 0.0f;
    CHECK_EQ_INT(mask_edit_wheel_steps(&acc, 1.0f), 1);
    CHECK_EQ_INT(mask_edit_wheel_steps(&acc, -1.0f), -1);
    int steps = 0;
    for (int i = 0; i < 10; i++) steps += mask_edit_wheel_steps(&acc, 0.25f);   /* 2.5 notches over ten frames */
    CHECK_EQ_INT(steps, 2);
    CHECK_EQ_INT(mask_edit_wheel_steps(&acc, -0.25f), 0);   /* turned back: the half notch left is dropped, a quarter is no step */
    CHECK_EQ_INT(mask_edit_wheel_steps(&acc, -0.75f), -1);
    CHECK_EQ_INT(mask_edit_wheel_steps(&acc, 0.0f), 0);
    CHECK_EQ_INT(mask_edit_wheel_steps(&acc, 3.0f), 3);   /* a fast mouse wheel: every notch */
    CHECK_EQ_INT(mask_edit_zoom_key('+'), ME_ZOOM_IN);
    CHECK_EQ_INT(mask_edit_zoom_key('='), ME_ZOOM_IN);
    CHECK_EQ_INT(mask_edit_zoom_key('-'), ME_ZOOM_OUT);
    CHECK_EQ_INT(mask_edit_zoom_key('0'), ME_ZOOM_FIT);
    CHECK_EQ_INT(mask_edit_zoom_key('z'), ME_ZOOM_NONE);
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: test_editor_mask_edit data-dir [game-files-dir]\n"); return 2; }
    snprintf(base, sizeof base, "/tmp/tnfs-mask-edit-XXXXXX");
    if (mkdtemp(base) == NULL) return 2;
    check_classes();
    check_model();
    check_refusals();
    check_texel_under();
    check_zoom();
    check_compose();
    check_repo(argv[1]);
    if (argc > 2) check_textures(argv[2], argv[1]);
    return TEST_REPORT();
}
