#include "studio/car_kit_check.h"

#include "formats/car_kit_paths.h"
#include "formats/cfm_build.h"
#include "formats/chunk.h"
#include "formats/mesh.h"
#include "formats/png.h"
#include "studio/car_kit.h"
#include "studio/fsio.h"

#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { CAR_MASK_PAINT_CLASS = 0 };

static void joined(char *out, size_t cap, const char *a, const char *b)
{
    snprintf(out, cap, "%s/%s", a, b);
}

/* the near mesh of a built CFM, described; 0 with the reason in `err` */
static int near_mesh(const uint8_t *cfm, size_t len, MeshDesc *out, ExchangeError *err)
{
    const uint8_t *child[CFM_CHILDREN];
    size_t clen[CFM_CHILDREN];
    if (!cfm_describe(cfm, len, child, clen, err)) return 0;
    Chunk c = { CHUNK_KIND_MESH, child[0], (size_t)(child[0][4] | child[0][5] << 8 | child[0][6] << 16 | (size_t)child[0][7] << 24), 0 };
    Mesh m;
    if (mesh_open(&m, &c) != MESH_OK) return exchange_fail(err, "the near mesh does not open"), 0;
    return mesh_describe(&m, out, err);
}

static int model(const char *kit, CarCheckList *list)
{
    uint8_t *cfm = NULL;
    size_t len = 0;
    ExchangeError err = { { 0 } };
    MeshDesc mesh;
    if (!car_kit_cfm_of_dir(kit, &cfm, &len, &err)) return car_check_add(list, CAR_CHECK_ERROR, "the model does not build: %s", err.message);
    const int ok = near_mesh(cfm, len, &mesh, &err);
    free(cfm);
    if (!ok) return car_check_add(list, CAR_CHECK_ERROR, "the built model does not read: %s", err.message);
    const int added = car_check_wheels(&mesh, list);
    mesh_desc_free(&mesh);
    return added;
}

/* a bank folder has a PNG for `tex` (NNNN_<tex>.png) */
static int bank_has(const char *bank, const char *tex)
{
    DIR *d = opendir(bank);
    if (d == NULL) return 0;
    const size_t n = strlen(tex);
    int found = 0;
    for (struct dirent *e; !found && (e = readdir(d)) != NULL;) {
        const size_t len = strlen(e->d_name);
        found = len == n + 9 && e->d_name[4] == '_' && strncmp(e->d_name + 5, tex, n) == 0 && strcmp(e->d_name + 5 + n, ".png") == 0;
    }
    closedir(d);
    return found;
}

/* does the mask PNG have a texel that takes the paint? -1: it does not read */
static int paints(const char *path)
{
    ExchangeError err = { { 0 } };
    size_t len = 0;
    uint8_t *data = fsio_read_file(path, &len, &err);
    if (data == NULL) return -1;
    PngImage img;
    const PngResult r = png_read(data, len, &img);
    free(data);
    if (r != PNG_OK) return -1;
    int found = 0;
    const size_t texels = (size_t)img.width * img.height, step = img.indexed ? 1 : 4;
    for (size_t i = 0; !found && i < texels; i++) found = img.pixels[i * step] == CAR_MASK_PAINT_CLASS;
    png_free(img.pixels);
    return found;
}

static int masks(const char *kit, CarCheckList *list)
{
    char dir[PATH_MAX], bank[PATH_MAX];
    joined(dir, sizeof dir, kit, car_kit_path(CAR_KIT_GEN_CAR_MASKS));
    DIR *d = opendir(dir);
    if (d == NULL) return 1;   /* a kit with no masks: the family rules paint it */
    int any = 0, painting = 0;
    for (struct dirent *e; (e = readdir(d)) != NULL;) {
        const size_t n = strlen(e->d_name);
        if (n < 10 || strcmp(e->d_name + n - 9, "_mask.png") != 0) continue;
        char tex[16], path[PATH_MAX];
        const size_t t = n - 9;
        if (t >= sizeof tex) continue;
        snprintf(tex, sizeof tex, "%.*s", (int)t, e->d_name);
        const int far = t > 3 && strcmp(tex + t - 3, "_lo") == 0;
        if (far) tex[t - 3] = '\0';
        joined(bank, sizeof bank, kit, car_kit_path(far ? CAR_KIT_FAR_BANK : CAR_KIT_NEAR_BANK));
        any = 1;
        joined(path, sizeof path, dir, e->d_name);
        painting += paints(path) > 0;
        if (!bank_has(bank, tex) && !car_check_add(list, CAR_CHECK_WARNING, "%s has no texture %s in the %s model: nothing goes under it", e->d_name, tex, far ? "far" : "near")) { closedir(d); return 0; }
    }
    closedir(d);
    if (any && painting == 0) return car_check_add(list, CAR_CHECK_WARNING, "no mask has a texel that takes the car's paint: the paints will change nothing (a black texel of a mask paints)");
    return 1;
}

int car_kit_check(const char *kit, CarCheckList *list)
{
    return model(kit, list) && masks(kit, list);
}

int car_check_command(int argc, char **argv, ExchangeError *err)
{
    if (argc != 3) return exchange_fail(err, "usage: tnfs-studio car check KIT_DIR"), 0;
    CarCheckList list = { 0 };
    if (!car_kit_check(argv[2], &list)) return car_check_free(&list), exchange_fail(err, "out of memory"), 0;
    for (size_t i = 0; i < list.count; i++) printf("%s: %s\n", list.item[i].severity == CAR_CHECK_ERROR ? "error" : "warning", list.item[i].text);
    const size_t errors = car_check_count(&list, CAR_CHECK_ERROR), warnings = car_check_count(&list, CAR_CHECK_WARNING);
    printf("%zu errors, %zu warnings\n", errors, warnings);
    car_check_free(&list);
    if (errors > 0) return exchange_fail(err, "%s has %zu errors", argv[2], errors), 0;
    return 1;
}
