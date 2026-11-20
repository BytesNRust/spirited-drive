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
    return TEST_REPORT();
}
