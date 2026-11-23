/* What a project's masks have changed from the game's (editor/mask_changes.c), on fixture folders made here: unchanged (the same bytes, and the same texels written by another
 * encoder), changed (the texels, the classes that gained and lost them; another size), new, missing, files that are not masks, a game folder that is not there, and the text. */
#include "mask_changes.h"
#include "formats/car_mask_codec.h"
#include "formats/png.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char base[512], proj[600], game[600];

static void put(const char *dir, const char *name, const uint8_t *px, int w, int h, int grey)
{
    uint8_t *png = NULL;
    size_t n = 0;
    char p[1024];
    snprintf(p, sizeof p, "%s/%s", dir, name);
    const int ok = grey ? car_mask_write_grey(px, w, h, &png, &n) : car_mask_write(px, w, h, &png, &n);
    CHECK(ok);
    if (!ok) return;
    FILE *f = fopen(p, "wb");
    fwrite(png, 1, n, f);
    fclose(f);
    png_free(png);
}

static const MaskChange *find(const MaskChanges *c, const char *file)
{
    for (int i = 0; i < c->count; i++)
        if (strcmp(c->item[i].file, file) == 0) return &c->item[i];
    return NULL;
}

int main(void)
{
    snprintf(base, sizeof base, "/tmp/tnfs-mask-changes-XXXXXX");
    if (mkdtemp(base) == NULL) return 2;
    snprintf(proj, sizeof proj, "%s/project", base);
    snprintf(game, sizeof game, "%s/game", base);
    mkdir(proj, 0777);
    mkdir(game, 0777);
    enum { W = 8, H = 4 };
    uint8_t a[W * H], b[W * H];
    for (int i = 0; i < W * H; i++) a[i] = i % 3 == 0 ? CAR_MASK_PAINT : CAR_MASK_MATTE;
    memcpy(b, a, sizeof a);

    put(game, "same_mask.png", a, W, H, 1);       /* the same bytes in both */
    put(proj, "same_mask.png", a, W, H, 1);
    put(game, "other_mask.png", a, W, H, 1);      /* the same texels, written by another encoder (indexed, not grey) */
    put(proj, "other_mask.png", a, W, H, 0);
    for (int i = 0; i < 5; i++) b[i] = a[i] == CAR_MASK_PAINT ? CAR_MASK_HEADLIGHT : CAR_MASK_REVERSE;   /* five texels changed */
    put(game, "edit_mask.png", a, W, H, 1);
    put(proj, "edit_mask.png", b, W, H, 1);
    put(game, "size_lo_mask.png", a, W, H, 1);    /* another size */
    put(proj, "size_lo_mask.png", a, W, H / 2, 1);
    put(proj, "added_mask.png", a, W, H, 1);      /* only the project has it */
    put(game, "gone_mask.png", a, W, H, 1);       /* only the game has it */
    char p[1024];
    FILE *f;
    snprintf(p, sizeof p, "%s/notes.txt", proj);
    f = fopen(p, "wb");
    fputs("not a mask", f);
    fclose(f);
    snprintf(p, sizeof p, "%s/odd_mask.png", proj);   /* a file named as a mask that is not a picture */
    f = fopen(p, "wb");
    fputs("not a png", f);
    fclose(f);
    put(game, "odd_mask.png", a, W, H, 1);
    put(game, "frnt.before-1650_mask.png", a, W, H, 1);   /* a backup the export leaves out: not compared */

    MaskChanges c;
    char why[256] = "";
    CHECK(mask_changes_compare(proj, game, &c, why, sizeof why));
    CHECK_EQ_INT(c.count, 7);   /* notes.txt is not one */
    CHECK_EQ_INT(c.unchanged, 2);
    CHECK_EQ_INT(c.changed, 3);
    CHECK_EQ_INT(c.added, 1);
    CHECK_EQ_INT(c.missing, 1);
    CHECK(strcmp(c.item[0].file, "added_mask.png") == 0);   /* by name */
    CHECK(find(&c, "same_mask.png")->kind == MC_UNCHANGED);
    CHECK(find(&c, "other_mask.png")->kind == MC_UNCHANGED);
    CHECK(find(&c, "added_mask.png")->kind == MC_NEW);
    CHECK(find(&c, "gone_mask.png")->kind == MC_MISSING);
    const MaskChange *e = find(&c, "edit_mask.png");
    CHECK(e != NULL && e->kind == MC_CHANGED);
    CHECK_EQ_INT(e->texels, 5);
    int gained = 0, lost = 0;
    for (int k = 0; k < ME_CLASSES; k++) gained += e->gained[k], lost += e->lost[k];
    CHECK(gained == 5 && lost == 5);
    CHECK_EQ_INT(e->gained[mask_edit_class_index(CAR_MASK_HEADLIGHT)] + e->gained[mask_edit_class_index(CAR_MASK_REVERSE)], 5);
    CHECK(e->lost[mask_edit_class_index(CAR_MASK_PAINT)] > 0 && e->lost[mask_edit_class_index(CAR_MASK_MATTE)] > 0);
    const MaskChange *s = find(&c, "size_lo_mask.png");
    CHECK(s != NULL && s->kind == MC_CHANGED && strstr(s->note, "8x4") != NULL && strstr(s->note, "8x2") != NULL);
    const MaskChange *o = find(&c, "odd_mask.png");
    CHECK(o != NULL && o->kind == MC_CHANGED && strstr(o->note, "project's file") != NULL);

    char text[2048];
    mask_changes_text(&c, "Wagon", text, sizeof text);
    CHECK(strstr(text, "car_masks/Wagon: 3 changed, 1 new, 1 missing, 2 unchanged") != NULL);
    CHECK(strstr(text, "changed: edit_mask.png 5 texels: ") != NULL);
    CHECK(strstr(text, "headlight +") != NULL && strstr(text, "paint -") != NULL);
    CHECK(strstr(text, "changed: size_lo_mask.png 32 texels (8x4 -> 8x2)") != NULL || strstr(text, "changed: size_lo_mask.png 16 texels (8x4 -> 8x2)") != NULL);
    CHECK(strstr(text, "new: added_mask.png") != NULL && strstr(text, "missing: gone_mask.png") != NULL);
    CHECK(strstr(text, "same_mask.png") == NULL);   /* the unchanged are counted, not listed */
    char tiny[40];
    mask_changes_text(&c, "Wagon", tiny, sizeof tiny);   /* a short buffer is cut, never overrun */
    CHECK(strlen(tiny) < sizeof tiny);

    char nowhere[700];
    snprintf(nowhere, sizeof nowhere, "%s/nowhere", base);
    CHECK(!mask_changes_compare(proj, nowhere, &c, why, sizeof why) && strstr(why, "not there") != NULL);
    CHECK(!mask_changes_compare(nowhere, game, &c, why, sizeof why) && strstr(why, "Cannot read") != NULL);

    /* Copy to game repo: the changed and the new are written into the game's folder, the unchanged and the missing are not, and the project is not touched */
    int written = -1;
    CHECK(mask_changes_compare(proj, game, &c, why, sizeof why));
    CHECK(mask_changes_publish(proj, game, &c, &written, why, sizeof why));
    CHECK_EQ_INT(written, 4);   /* edit, size_lo, odd (changed) and added */
    CHECK(mask_changes_compare(proj, game, &c, why, sizeof why));
    CHECK_EQ_INT(c.changed, 0);
    CHECK_EQ_INT(c.added, 0);
    CHECK_EQ_INT(c.missing, 1);   /* gone_mask.png is still only the game's: never deleted */
    CHECK_EQ_INT(c.unchanged, 6);
    CHECK(mask_changes_publish(proj, game, &c, &written, why, sizeof why) && written == 0);   /* nothing left: nothing written */
    CHECK(!mask_changes_publish(proj, nowhere, &c, &written, why, sizeof why) || written == 0);   /* no changes listed: no file is opened in a folder that is not there */

    /* the dash folder: dash_mask.png and dash_color.png only, compared as pictures (an indexed and an RGBA PNG of the same colours are one picture) */
    char dproj[700], dgame[700];
    snprintf(dproj, sizeof dproj, "%s/dash_project", base);
    snprintf(dgame, sizeof dgame, "%s/dash_game", base);
    mkdir(dproj, 0777);
    mkdir(dgame, 0777);
    enum { DW = 4, DH = 2 };
    const uint8_t index[DW * DH] = { 0, 1, 2, 0, 1, 2, 0, 1 };
    uint8_t pal5[768] = { 0 }, pal6[768] = { 0 }, rgba[DW * DH * 4], rgba2[DW * DH * 4];
    for (int k = 0; k < 3; k++) pal5[3 * k] = pal6[3 * k] = (uint8_t)(40 * k), pal5[3 * k + 1] = pal6[3 * k + 1] = (uint8_t)(90 + k), pal5[3 * k + 2] = pal6[3 * k + 2] = 7;
    pal6[3 * 5] = 250;   /* a sixth entry the picture does not use */
    for (int i = 0; i < DW * DH; i++) memcpy(rgba + 4 * i, pal5 + 3 * index[i], 3), rgba[4 * i + 3] = 255;
    memcpy(rgba2, rgba, sizeof rgba);
    rgba2[0] ^= 0x10;   /* one texel's colour */
    uint8_t *png = NULL;
    size_t n = 0;
    const struct { const char *dir, *name; int rgb; const uint8_t *pal; } WRITE[] = {
        { dgame, "dash_mask.png", 0, pal5 }, { dproj, "dash_mask.png", 0, pal6 },     /* another palette length: unchanged */
        { dgame, "dash_color.png", 1, NULL }, { dproj, "dash_color.png", 2, NULL },   /* RGBA, one texel different: changed */
    };
    for (size_t i = 0; i < sizeof WRITE / sizeof WRITE[0]; i++) {
        const int ok = WRITE[i].rgb == 0 ? png_write_indexed(index, DW, DH, WRITE[i].pal, &png, &n) == PNG_OK : png_write_rgba(WRITE[i].rgb == 1 ? rgba : rgba2, DW, DH, &png, &n) == PNG_OK;
        CHECK(ok);
        if (!ok) continue;
        snprintf(p, sizeof p, "%s/%s", WRITE[i].dir, WRITE[i].name);
        f = fopen(p, "wb");
        fwrite(png, 1, n, f);
        fclose(f);
        png_free(png);
    }
    put(dproj, "notes_mask.png", a, W, H, 1);   /* not a dash file: left out */
    MaskChanges d;
    CHECK(mask_changes_compare_dash(dproj, dgame, &d, why, sizeof why));
    CHECK_EQ_INT(d.count, 2);
    CHECK(find(&d, "dash_mask.png")->kind == MC_UNCHANGED);
    const MaskChange *dc = find(&d, "dash_color.png");
    CHECK(dc != NULL && dc->kind == MC_CHANGED && dc->texels == 1);
    mask_changes_text(&d, "ANSX", text, sizeof text);
    CHECK(strstr(text, "dash_masks/ANSX: 1 changed, 0 new, 0 missing, 1 unchanged (compared with the game's data/dash_masks/ANSX)") != NULL);
    CHECK(strstr(text, "changed: dash_color.png 1 texels") != NULL);
    CHECK(mask_changes_publish(dproj, dgame, &d, &written, why, sizeof why) && written == 1);
    CHECK(mask_changes_compare_dash(dproj, dgame, &d, why, sizeof why) && d.changed == 0 && d.unchanged == 2);
    return TEST_REPORT();
}
