/* A mask changed on disk while the car is open (W4, editor/app_mask.c's app_mask_watch): the files are looked at about once a second; a mask with no
 * unsaved edits is read again (the Masks window, and through mask.generation the Preview and the Model), one with edits keeps them and says
 * «Changed on disk» (Reload or Keep mine), and Save does not write over a file changed under it until Keep mine. A synthetic car (a box with a
 * textured side: mesh_build/shpi_build/cfm_build, no game data). */
#include "app.h"
#include "formats/car_mask_codec.h"
#include "formats/cfm_build.h"
#include "formats/mesh_build.h"
#include "formats/png.h"
#include "formats/shpi_build.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

enum { SIDE = 8 };

static char root[400];

/* a box of 6 quads, the +x one textured with "side" (8 x 8), in both banks */
static uint8_t *make_cfm(size_t *len)
{
    static MeshVertex v[8];
    static MeshUv uv[4] = { { 0, 0 }, { 7, 0 }, { 7, 7 }, { 0, 7 } };
    static MeshDescPolygon p[6];
    static MeshTexture t[1];
    static uint32_t surface[6] = { 0, 1, 2, 3, 4, 5 };
    static MeshDescGroup g[1];
    const int sx[8] = { -1, 1, 1, -1, -1, 1, 1, -1 }, sy[8] = { -1, -1, 1, 1, -1, -1, 1, 1 }, sz[8] = { -1, -1, -1, -1, 1, 1, 1, 1 };
    for (int i = 0; i < 8; i++) v[i] = (MeshVertex){ sx[i] * 600, sy[i] * 300, sz[i] * 400 };
    const uint32_t face[6][4] = { { 0, 1, 2, 3 }, { 5, 4, 7, 6 }, { 4, 0, 3, 7 }, { 1, 5, 6, 2 }, { 3, 2, 6, 7 }, { 4, 5, 1, 0 } };
    for (int i = 0; i < 6; i++) {
        p[i] = (MeshDescPolygon){ .flags = 4 | 0x80, .unknown_1 = 0x11, .texture = 0, .has_uv = 1 };
        for (int k = 0; k < 4; k++) p[i].vertex[k] = face[i][k], p[i].uv[k] = (uint32_t)k;
    }
    memcpy(t[0].label, "side", 4), memcpy(t[0].name, "side", 4);
    memcpy(g[0].name, "surface", 7), g[0].polygons = surface, g[0].count = 6;
    MeshDesc d = { 0 };
    d.vertices = v, d.vertex_count = 8, d.uvs = uv, d.uv_count = 4, d.polygons = p, d.polygon_count = 6, d.textures = t, d.texture_count = 1, d.groups = g, d.group_count = 1;
    uint8_t *orip = NULL;
    size_t orip_len = 0;
    ExchangeError err = { { 0 } };
    if (!mesh_build(&d, &orip, &orip_len, &err)) return NULL;
    static uint8_t pal[768], side[SIDE * SIDE];
    for (int i = 0; i < 256; i++) pal[3 * i] = (uint8_t)i, pal[3 * i + 1] = (uint8_t)(255 - i), pal[3 * i + 2] = (uint8_t)(i / 2);
    for (int i = 0; i < SIDE * SIDE; i++) side[i] = (uint8_t)(40 + i);
    const ShpiDescEntry e[2] = { { .name = { '!', 'P', 'A', 'L' }, .type = 0x24, .width = 256, .height = 1, .data = pal, .block_size = SHPI_BLOCK_AUTO },
                                 { .name = { 's', 'i', 'd', 'e' }, .type = 0x7b, .width = SIDE, .height = SIDE, .data = side, .block_size = SHPI_BLOCK_AUTO, .dir_index = 1 } };
    uint8_t *shpi = NULL;
    size_t shpi_len = 0;
    if (!shpi_build(e, 2, &shpi, &shpi_len, &err)) return free(orip), NULL;
    const uint8_t *const child[CFM_CHILDREN] = { orip, shpi, orip, shpi };
    const size_t clen[CFM_CHILDREN] = { orip_len, shpi_len, orip_len, shpi_len };
    uint8_t *cfm = NULL;
    const int ok = cfm_build(child, clen, &cfm, len, &err);
    free(orip), free(shpi);
    return ok ? cfm : NULL;
}

static int write_file(const char *path, const uint8_t *data, size_t n)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL) return 0;
    const int ok = fwrite(data, 1, n, f) == n;
    return fclose(f) == 0 && ok;
}

static char mask_path[600];

/* side_mask.png, every texel `value`, as another program (a picture editor) would write it */
static int write_mask(uint8_t value)
{
    uint8_t mask[SIDE * SIDE], *png = NULL;
    size_t n = 0;
    memset(mask, value, sizeof mask);
    if (!car_mask_write(mask, SIDE, SIDE, &png, &n)) return 0;
    const int ok = write_file(mask_path, png, n);
    png_free(png);
    return ok;
}

/* the class of texel (x, y) of the mask file on disk */
static int class_on_disk(int x, int y)
{
    MaskEdit e = { 0 };
    char why[256];
    if (!mask_edit_open(&e, mask_path, 0, 0, NULL, why, sizeof why)) return -1;
    const int c = mask_edit_class_at(&e, x, y);
    mask_edit_free(&e);
    return c;
}

static App *open_car(void)
{
    char dir[500], cfm[600];
    snprintf(dir, sizeof dir, "%s/car_masks/BOX", root);
    snprintf(cfm, sizeof cfm, "%s/car_masks", root);
    mkdir(cfm, 0755);   /* there already from the first test: fine */
    mkdir(dir, 0755);
    snprintf(cfm, sizeof cfm, "%s/BOX.CFM", root);
    size_t len = 0;
    uint8_t *bytes = make_cfm(&len);
    const int wrote = bytes != NULL && write_file(cfm, bytes, len);
    free(bytes);
    if (!wrote) return NULL;
    snprintf(mask_path, sizeof mask_path, "%s/side_mask.png", dir);
    if (!write_mask(CAR_MASK_HEADLIGHT)) return NULL;
    App *a = calloc(1, sizeof *a);
    if (a == NULL) return NULL;
    app_begin(a, "./tnfs", "", NULL);
    a->kind = APP_KIND_CAR;
    snprintf(a->car.piece, sizeof a->car.piece, "%s", root);
    snprintf(a->car.folder, sizeof a->car.folder, "BOX");
    a->car.legacy = 1;   /* a piece folder: its masks under car_masks/<folder>/ (car_project_masks_dir) */
    snprintf(a->car.cfm, sizeof a->car.cfm, "%s", cfm);
    app_mask_scan(a);
    return a;
}

static void test_a_mask_changed_on_disk(void)
{
    App *a = open_car();
    CHECK(a != NULL);
    if (a == NULL) return;
    const int head = mask_edit_class_index(CAR_MASK_HEADLIGHT), matte = mask_edit_class_index(CAR_MASK_MATTE), paint = mask_edit_class_index(CAR_MASK_PAINT);
    CHECK(app_mask_open_by_name(a, "side"));
    CHECK_EQ_INT(mask_edit_class_at(&a->mask.me, 3, 3), head);
    CHECK_EQ_INT(app_mask_watch(a, 10.0), 0);   /* nothing changed: the first look only records the files */

    /* no unsaved edits: read again, at the next look a second on */
    unsigned gen = a->mask.generation;
    CHECK(write_mask(CAR_MASK_MATTE));
    CHECK_EQ_INT(app_mask_watch(a, 10.4), 0);   /* not yet a second */
    CHECK_EQ_INT(mask_edit_class_at(&a->mask.me, 3, 3), head);
    CHECK_EQ_INT(app_mask_watch(a, 11.1), 1);
    CHECK(a->mask.sel >= 0 && !app_mask_modified(a) && !a->mask.disk_changed);
    CHECK_EQ_INT(mask_edit_class_at(&a->mask.me, 3, 3), matte);   /* the new texels */
    CHECK(a->mask.generation != gen);                              /* the Preview and the Model take it */
    CHECK(strstr(a->status, "changed on disk") != NULL);
    CHECK_EQ_INT(app_mask_watch(a, 12.2), 0);                      /* once */

    /* our own Save is not a change on disk */
    CHECK(mask_edit_pencil(&a->mask.me, 1, 1, 1, CAR_MASK_PAINT) > 0);
    CHECK(app_mask_save(a));
    CHECK_EQ_INT(app_mask_watch(a, 13.3), 0);
    CHECK(mask_edit_can_undo(&a->mask.me));   /* the history stays */

    /* unsaved edits: kept, the state says so, Save does not write over the file */
    CHECK(mask_edit_pencil(&a->mask.me, 2, 2, 1, CAR_MASK_PAINT) > 0);
    gen = a->mask.generation;
    CHECK(write_mask(CAR_MASK_HEADLIGHT));
    CHECK_EQ_INT(app_mask_watch(a, 14.4), 1);
    CHECK(a->mask.disk_changed && app_mask_modified(a));
    CHECK_EQ_INT(mask_edit_class_at(&a->mask.me, 2, 2), paint);
    CHECK(!app_mask_save(a));
    CHECK(strstr(a->mask.error, "changed on disk") != NULL);
    CHECK_EQ_INT(class_on_disk(2, 2), head);
    CHECK_EQ_INT(app_mask_watch(a, 15.5), 0);                      /* said once */
    /* Keep mine: then Save writes it */
    app_mask_keep_mine(a);
    CHECK(!a->mask.disk_changed);
    CHECK(app_mask_save(a));
    CHECK_EQ_INT(class_on_disk(2, 2), paint);
    CHECK_EQ_INT(class_on_disk(3, 3), matte);                      /* mine, all of it */

    /* Reload: the file's texels, the edits gone */
    CHECK(mask_edit_pencil(&a->mask.me, 4, 4, 1, CAR_MASK_HEADLIGHT) > 0);
    CHECK(write_mask(CAR_MASK_PAINT));
    CHECK_EQ_INT(app_mask_watch(a, 16.6), 1);
    CHECK(a->mask.disk_changed);
    CHECK(app_mask_reload(a));
    CHECK(!a->mask.disk_changed && !app_mask_modified(a));
    CHECK_EQ_INT(mask_edit_class_at(&a->mask.me, 4, 4), paint);

    /* changed between two looks, then Save at once: asked all the same */
    CHECK(mask_edit_pencil(&a->mask.me, 5, 5, 1, CAR_MASK_MATTE) > 0);
    CHECK(write_mask(CAR_MASK_HEADLIGHT));
    CHECK(!app_mask_save(a));
    CHECK(a->mask.disk_changed);
    CHECK_EQ_INT(class_on_disk(5, 5), head);

    /* no mask open: a change still reaches the Preview and the Model */
    CHECK(app_mask_reload(a));
    app_mask_close(a);
    gen = a->mask.generation;
    CHECK(write_mask(CAR_MASK_MATTE));
    CHECK_EQ_INT(app_mask_watch(a, 17.7), 1);
    CHECK(a->mask.generation != gen);
    /* removed: the list says so, a mask that comes back is listed again */
    CHECK(unlink(mask_path) == 0);
    CHECK_EQ_INT(app_mask_watch(a, 18.8), 1);
    CHECK(!a->mask.has_mask[0]);
    CHECK(write_mask(CAR_MASK_PAINT));
    CHECK_EQ_INT(app_mask_watch(a, 19.9), 1);
    CHECK(a->mask.has_mask[0]);

    app_free(a);
    free(a);
}

/* a removed open mask with no edits closes; with edits it stays, Changed on disk */
static void test_the_open_mask_removed(void)
{
    App *a = open_car();
    CHECK(a != NULL);
    if (a == NULL) return;
    CHECK(app_mask_open_by_name(a, "side"));
    CHECK_EQ_INT(app_mask_watch(a, 1.0), 0);
    CHECK(unlink(mask_path) == 0);
    CHECK_EQ_INT(app_mask_watch(a, 2.1), 1);
    CHECK_EQ_INT(a->mask.sel, -1);
    CHECK(write_mask(CAR_MASK_PAINT));
    CHECK_EQ_INT(app_mask_watch(a, 3.2), 1);
    CHECK(app_mask_open_by_name(a, "side"));
    CHECK(mask_edit_pencil(&a->mask.me, 1, 1, 1, CAR_MASK_MATTE) > 0);
    CHECK(unlink(mask_path) == 0);
    CHECK_EQ_INT(app_mask_watch(a, 4.3), 1);
    CHECK(a->mask.sel >= 0 && a->mask.disk_changed && app_mask_modified(a));
    app_mask_keep_mine(a);
    CHECK(app_mask_save(a));   /* Keep mine writes it back */
    CHECK_EQ_INT(class_on_disk(1, 1), mask_edit_class_index(CAR_MASK_MATTE));
    CHECK(a->mask.has_mask[0]);   /* listed again */
    /* half written (another program still writing it): the open mask stays, read at a later look */
    CHECK(write_file(mask_path, (const uint8_t *)"\x89PNG", 4));
    CHECK_EQ_INT(app_mask_watch(a, 5.4), 0);
    CHECK(a->mask.sel >= 0 && !a->mask.disk_changed);
    CHECK(write_mask(CAR_MASK_HEADLIGHT));
    CHECK_EQ_INT(app_mask_watch(a, 6.5), 1);
    CHECK(a->mask.sel >= 0 && mask_edit_class_at(&a->mask.me, 1, 1) == mask_edit_class_index(CAR_MASK_HEADLIGHT));
    app_free(a);
    free(a);
}

/* the state a texture's mask shows in the Masks list: from the PSD times when there are PSDs to compare with, else the PNG is the source */
static void test_the_state_of_a_mask(void)
{
    App *a = open_car();
    CHECK(a != NULL);
    if (a == NULL) return;
    const int side = 0;
    CHECK(a->mask.tex_count > 0 && a->mask.has_mask[side]);
    CHECK(strcmp(app_mask_state(a, side), "PNG only") == 0);   /* no PSD folder: the PNG is the source */
    a->stale_ok = 1;
    a->stale.count = 1;
    snprintf(a->stale.item[0].png, sizeof a->stale.item[0].png, "%s", a->mask.tex[side].file);
    a->stale.item[0].state = STALE_OK;
    CHECK(strcmp(app_mask_state(a, side), "in sync") == 0);
    a->stale.item[0].state = STALE_NEWER;
    CHECK(strcmp(app_mask_state(a, side), "PSD newer") == 0);
    a->stale.item[0].state = STALE_MISSING;
    CHECK(strcmp(app_mask_state(a, side), "PSD found, no mask yet") == 0);
    a->stale.item[0].dash = 1;   /* the dash's PSD is not a body texture's */
    CHECK(strcmp(app_mask_state(a, side), "PNG only") == 0);
    a->mask.has_mask[side] = 0;
    CHECK(strcmp(app_mask_state(a, side), "no mask") == 0);
    CHECK(strcmp(app_mask_state(a, -1), "no mask") == 0);
    CHECK(strcmp(app_mask_state(a, 999), "no mask") == 0);
    app_free(a);
    free(a);
}

int main(void)
{
    const char *tmp = getenv("TMPDIR");
    snprintf(root, sizeof root, "%s/tnfs-mask-reload-XXXXXX", tmp != NULL && tmp[0] != '\0' ? tmp : "/tmp");
    if (mkdtemp(root) == NULL) {
        fprintf(stderr, "no scratch folder\n");
        return 1;
    }
    test_a_mask_changed_on_disk();
    test_the_open_mask_removed();
    test_the_state_of_a_mask();
    char p[600];
    unlink(mask_path);
    snprintf(p, sizeof p, "%s/car_masks/BOX", root), rmdir(p);
    snprintf(p, sizeof p, "%s/car_masks", root), rmdir(p);
    snprintf(p, sizeof p, "%s/BOX.CFM", root), unlink(p);
    rmdir(root);
    return TEST_REPORT();
}
