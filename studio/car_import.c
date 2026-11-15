#include "studio/car_import.h"

#include "formats/car_import.h"
#include "formats/car_piece.h"
#include "formats/mod_ini.h"
#include "formats/pdn.h"
#include "formats/slots.h"
#include "studio/car_new.h"
#include "studio/fsio.h"

#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define USAGE                                                                                                                       \
    "usage: tnfs-studio car import PIECE_DIR --obj FILE.obj [--low FILE.obj] [--textures DIR] [--low-textures DIR] [--raw] "        \
    "[--scale S] [--steer DEGREES] [--donor CAR] [--keep-pdn] [--data DIR] [--force]"

typedef struct {
    const char *piece, *obj, *low, *textures, *low_textures, *donor, *data;
    double scale, steer;
    int raw, keep_pdn, force;
} Args;

/* an OBJ with what goes with it: its model, its tables (raw) and its textures */
typedef struct {
    ObjModel model;
    FileSet tables, pngs;
    int loaded;
} Source;

static int number(const char *text, double *out)
{
    char *end;
    *out = strtod(text, &end);
    return end != text && *end == '\0';
}

static int parse(int argc, char **argv, Args *a, ExchangeError *err)
{
    memset(a, 0, sizeof *a);
    a->data = "original", a->scale = 1;
    for (int i = 2; i < argc; ++i) {
        const int has_value = i + 1 < argc;
        if (strcmp(argv[i], "--obj") == 0 && has_value) a->obj = argv[++i];
        else if (strcmp(argv[i], "--low") == 0 && has_value) a->low = argv[++i];
        else if (strcmp(argv[i], "--textures") == 0 && has_value) a->textures = argv[++i];
        else if (strcmp(argv[i], "--low-textures") == 0 && has_value) a->low_textures = argv[++i];
        else if (strcmp(argv[i], "--donor") == 0 && has_value) a->donor = argv[++i];
        else if (strcmp(argv[i], "--data") == 0 && has_value) a->data = argv[++i];
        else if (strcmp(argv[i], "--scale") == 0 && has_value) {
            if (!number(argv[++i], &a->scale) || !(a->scale > 0)) { exchange_fail(err, "--scale '%s' is not a number above 0", argv[i]); return 0; }
        } else if (strcmp(argv[i], "--steer") == 0 && has_value) {
            if (!number(argv[++i], &a->steer) || !(a->steer > 0 && a->steer < 90)) {
                exchange_fail(err, "--steer '%s' is not degrees between 0 and 90 (the CD's cars turn 22 to 37)", argv[i]);
                return 0;
            }
        } else if (strcmp(argv[i], "--raw") == 0) a->raw = 1;
        else if (strcmp(argv[i], "--keep-pdn") == 0) a->keep_pdn = 1;
        else if (strcmp(argv[i], "--force") == 0) a->force = 1;
        else if (argv[i][0] != '-' && a->piece == NULL) a->piece = argv[i];
        else { exchange_fail(err, "unexpected argument '%s'; %s", argv[i], USAGE); return 0; }
    }
    if (a->piece == NULL || a->obj == NULL) { exchange_fail(err, USAGE); return 0; }
    return 1;
}

static void dir_of(const char *path, char *out, size_t cap)
{
    const char *slash = strrchr(path, '/');
    if (slash == NULL) snprintf(out, cap, ".");
    else snprintf(out, cap, "%.*s", (int)(slash == path ? 1 : slash - path), path);
}

static void join(char *out, size_t cap, const char *dir, const char *name)
{
    if (name[0] == '/') snprintf(out, cap, "%s", name);
    else snprintf(out, cap, "%s/%s", dir, name);
}

/* the OBJ's first mtllib, as written (to the line's end), or "" */
static void mtllib_of(const uint8_t *obj, size_t len, char *out, size_t cap)
{
    out[0] = '\0';
    for (size_t at = 0; at < len;) {
        const uint8_t *end = memchr(obj + at, '\n', len - at);
        size_t n = (end != NULL ? (size_t)(end - obj) : len) - at;
        if (n > 7 && memcmp(obj + at, "mtllib ", 7) == 0) {
            while (n > 7 && (obj[at + n - 1] == '\r' || obj[at + n - 1] == ' ')) n--;
            snprintf(out, cap, "%.*s", (int)(n - 7), (const char *)obj + at + 7);
            return;
        }
        at += n + 1;
    }
}

/* the OBJ and its MTL into `model`; `mtl_dir` gets the MTL's directory (the OBJ's when it has none) */
static int read_obj(const char *path, ObjModel *model, char *mtl_dir, size_t cap, ExchangeError *err)
{
    size_t len = 0, mtl_len = 0;
    uint8_t *obj = fsio_read_file(path, &len, err), *mtl = NULL;
    if (obj == NULL) return 0;
    char dir[PATH_MAX], name[PATH_MAX], mtl_path[PATH_MAX];
    dir_of(path, dir, sizeof dir);
    snprintf(mtl_dir, cap, "%s", dir);
    mtllib_of(obj, len, name, sizeof name);
    if (name[0] != '\0') {
        join(mtl_path, sizeof mtl_path, dir, name);
        if ((mtl = fsio_read_file(mtl_path, &mtl_len, err)) == NULL) { free(obj); return 0; }
        dir_of(mtl_path, mtl_dir, cap);
    }
    const int ok = obj_read(obj, len, mtl, mtl_len, model, err);
    if (!ok) {
        char why[sizeof err->message];
        snprintf(why, sizeof why, "%s", err->message);
        exchange_fail(err, "%s: %s", path, why);
    }
    free(obj), free(mtl);
    return ok;
}

/* the .tsv files beside the OBJ (unpack's tables, lists.tsv among them) */
static int read_tables(const char *dir, FileSet *set, ExchangeError *err)
{
    DIR *d = opendir(dir);
    if (d == NULL) { exchange_fail(err, "cannot read the directory %s", dir); return 0; }
    int ok = 1;
    for (struct dirent *e; ok && (e = readdir(d)) != NULL;) {
        const size_t n = strlen(e->d_name);
        if (n < 5 || strcmp(e->d_name + n - 4, ".tsv") != 0) continue;
        char path[PATH_MAX];
        size_t size = 0;
        join(path, sizeof path, dir, e->d_name);
        uint8_t *bytes = fsio_read_file(path, &size, err);
        ok = bytes != NULL && fileset_add_owned(set, e->d_name, bytes, size);
        if (bytes != NULL && !ok) free(bytes), exchange_fail(err, "out of memory");
    }
    closedir(d);
    return ok;
}

/* each texture the MTL names, under its map_Kd path, that is there (the importer names a missing one) */
static int read_pngs(const ObjModel *m, const char *base, FileSet *set, ExchangeError *err)
{
    for (uint32_t i = 0; i < m->material_count; i++) {
        const char *tex = m->materials[i].texture;
        char path[PATH_MAX];
        if (tex[0] == '\0' || fileset_find(set, tex) != NULL) continue;
        join(path, sizeof path, base, tex);
        if (!fsio_exists(path)) continue;
        size_t size = 0;
        uint8_t *bytes = fsio_read_file(path, &size, err);
        if (bytes == NULL) return 0;
        if (!fileset_add_owned(set, tex, bytes, size)) { free(bytes); exchange_fail(err, "out of memory"); return 0; }
    }
    return 1;
}

static int load(const char *obj, const char *textures, int raw, Source *s, ExchangeError *err)
{
    char mtl_dir[PATH_MAX], obj_dir[PATH_MAX];
    fileset_init(&s->tables), fileset_init(&s->pngs);
    if (!read_obj(obj, &s->model, mtl_dir, sizeof mtl_dir, err)) return 0;
    s->loaded = 1;
    dir_of(obj, obj_dir, sizeof obj_dir);
    return raw ? read_tables(obj_dir, &s->tables, err) : read_pngs(&s->model, textures != NULL ? textures : mtl_dir, &s->pngs, err);
}

static void unload(Source *s)
{
    if (s->loaded) obj_free(&s->model);
    fileset_free(&s->tables), fileset_free(&s->pngs);
}

static int read_card(const char *piece, ModInfo *info, ExchangeError *err)
{
    char path[PATH_MAX], why[160];
    if (!fsio_find_ci(piece, "mod.ini", path, sizeof path)) {
        exchange_fail(err, "%s has no mod.ini: car import works on a car piece (tnfs-studio car new makes one)", piece);
        return 0;
    }
    size_t size = 0;
    uint8_t *text = fsio_read_file(path, &size, err);
    if (text == NULL) return 0;
    const int ok = mod_ini_parse(text, size, info, why, sizeof why);
    free(text);
    if (!ok) { exchange_fail(err, "%s: %s", path, why); return 0; }
    if (info->type != MOD_TYPE_CAR || info->stem[0] == '\0') { exchange_fail(err, "%s: not a car piece (type=car with a stem)", path); return 0; }
    return 1;
}

/* the donor's CFM from the data root */
static uint8_t *read_donor(const Args *a, const ModInfo *info, size_t *size, char *stem, size_t cap, ExchangeError *err)
{
    const char *word = a->donor != NULL ? a->donor : info->sound_from;
    const int model = car_template_model(word);
    if (model < 0) { exchange_fail(err, "the donor '%s' is not one of the eight race cars (--donor viper, or DVIPER)", word); return NULL; }
    snprintf(stem, cap, "%s", slot_stems(SLOT_CAR)[model]);
    char paths[CAR_PIECE_FILES][CAR_PIECE_PATH], real[PATH_MAX];
    car_piece_paths(stem, paths);
    if (!fsio_find_ci(a->data, paths[0], real, sizeof real)) { exchange_fail(err, "no such file in the data root %s: %s", a->data, paths[0]); return NULL; }
    return fsio_read_file(real, size, err);
}

/* the piece's file `rel` (any case), or where it goes */
static int piece_path(const char *piece, const char *rel, char *out, size_t cap)
{
    if (fsio_find_ci(piece, rel, out, cap)) return 1;
    join(out, cap, piece, rel);
    return 0;
}

/* the piece's CFM may be replaced: absent, the donor's (car new's copy), or --force */
static int may_replace(const char *path, int exists, const uint8_t *donor, size_t donor_len, int force, ExchangeError *err)
{
    if (!exists || force) return 1;
    size_t size = 0;
    uint8_t *now = fsio_read_file(path, &size, err);
    if (now == NULL) return 0;
    const int same = size == donor_len && memcmp(now, donor, size) == 0;
    free(now);
    if (!same) exchange_fail(err, "%s is not the CFM car new copied from the donor: --force to replace it", path);
    return same;
}

static void put_i32(uint8_t *p, int32_t v)
{
    for (int i = 0; i < 4; i++) p[i] = (uint8_t)((uint32_t)v >> (8 * i));
}

/* the piece's PDN with its first three words the body's half sizes, packed as it was (malloc'd) */
static uint8_t *pdn_with(const char *path, const int32_t half[3], int32_t was[3], size_t *len, ExchangeError *err)
{
    size_t size = 0;
    uint8_t *file = fsio_read_file(path, &size, err), record[PDN_SIZE];
    if (file == NULL) return NULL;
    const int ok = pdn_read(record, file, size);
    free(file);
    if (!ok) { exchange_fail(err, "%s is not a PDN", path); return NULL; }
    for (int k = 0; k < 3; k++) was[k] = pdn_i32(record, 4 * k), put_i32(record + 4 * k, half[k]);
    const size_t cap = 2 * PDN_SIZE + 64;
    uint8_t *out = malloc(cap);
    if (out == NULL) { exchange_fail(err, "out of memory"); return NULL; }
    if (size == PDN_SIZE) memcpy(out, record, PDN_SIZE), *len = PDN_SIZE;
    else if (pdn_pack(record, out, cap, len) != QFS_OK) { free(out); exchange_fail(err, "%s: cannot pack the PDN", path); return NULL; }
    return out;
}

static void print_note(void *ctx, const char *text)
{
    (void)ctx;
    printf("car import: %s\n", text);
}

/* the piece's car masks (car_masks/STEM) of a bank that is no longer the donor's removed (R9): the high ones (TEX_mask.png) unless
 * raw, whose high bank is the donor's bitmaps; the low ones (TEX_lo_mask.png) unless raw with a low OBJ. The count removed */
static int remove_masks(const char *piece, const char *stem, int keep_high, int keep_low)
{
    char rel[64], dir[PATH_MAX], parent[PATH_MAX];
    snprintf(rel, sizeof rel, "car_masks/%s", stem);
    if (!fsio_find_ci(piece, rel, dir, sizeof dir)) return 0;
    DIR *d = opendir(dir);
    if (d == NULL) return 0;
    int removed = 0;
    for (struct dirent *e; (e = readdir(d)) != NULL;) {
        const size_t n = strlen(e->d_name);
        if (n < 9 || strcmp(e->d_name + n - 9, "_mask.png") != 0) continue;
        const int low = n >= 12 && strcmp(e->d_name + n - 12, "_lo_mask.png") == 0;
        if (low ? keep_low : keep_high) continue;
        char path[PATH_MAX];
        join(path, sizeof path, dir, e->d_name);
        removed += fsio_remove(path);
    }
    closedir(d);
    fsio_prune_empty(dir, piece);
    dir_of(dir, parent, sizeof parent);
    fsio_prune_empty(parent, piece);
    return removed;
}

typedef struct {
    uint8_t *cfm, *pdn;
    size_t cfm_len, pdn_len;
    char cfm_path[PATH_MAX], pdn_path[PATH_MAX];
    int32_t half[3], was[3];
} Made;

static int make(const Args *a, const ModInfo *info, const Source *high, const Source *low, Made *m, ExchangeError *err)
{
    char donor_stem[16], paths[CAR_PIECE_FILES][CAR_PIECE_PATH];
    size_t donor_len = 0;
    uint8_t *donor = read_donor(a, info, &donor_len, donor_stem, sizeof donor_stem, err);
    if (donor == NULL) return 0;
    car_piece_paths(info->stem, paths);
    const int exists = piece_path(a->piece, paths[0], m->cfm_path, sizeof m->cfm_path);
    const int has_pdn = piece_path(a->piece, paths[1], m->pdn_path, sizeof m->pdn_path);
    CarImportIn in;
    memset(&in, 0, sizeof in);
    in.donor_cfm = donor, in.donor_cfm_len = donor_len, in.raw = a->raw, in.scale = a->scale, in.steer_degrees = a->steer;
    in.high = &high->model, in.high_tables = &high->tables, in.high_pngs = &high->pngs, in.note = print_note;
    if (low != NULL) in.low = &low->model, in.low_tables = &low->tables, in.low_pngs = &low->pngs;
    int ok = may_replace(m->cfm_path, exists, donor, donor_len, a->force, err) && car_import(&in, &m->cfm, &m->cfm_len, m->half, err);
    if (ok && !a->keep_pdn && !has_pdn) ok = 0, exchange_fail(err, "the piece has no %s for the body's size (--keep-pdn to go without)", paths[1]);
    if (ok && !a->keep_pdn) ok = (m->pdn = pdn_with(m->pdn_path, m->half, m->was, &m->pdn_len, err)) != NULL;
    free(donor);
    return ok;
}

static int write_out(const Args *a, const ModInfo *info, const Made *m, ExchangeError *err)
{
    if (!fsio_write_atomic(m->cfm_path, m->cfm, m->cfm_len, err)) return 0;
    printf("car import: %s written; the body measures %d x %d x %d units (x y z)\n", m->cfm_path, m->half[0] >> 8, m->half[1] >> 8, m->half[2] >> 8);
    if (m->pdn != NULL) {
        if (!fsio_write_atomic(m->pdn_path, m->pdn, m->pdn_len, err)) return 0;
        printf("car import: %s: the body's half sizes %d %d %d (were %d %d %d)\n", m->pdn_path, m->half[0], m->half[1], m->half[2], m->was[0], m->was[1], m->was[2]);
    } else {
        fprintf(stderr, "car import: --keep-pdn: the PDN keeps the template's body size; the model's half sizes are %d %d %d\n", m->half[0], m->half[1], m->half[2]);
    }
    const int removed = remove_masks(a->piece, info->stem, a->raw, a->raw && a->low != NULL);
    if (removed > 0) printf("car import: removed %d masks of car_masks/%s: they paint the template's textures\n", removed, info->stem);
    printf("try it: tnfs --mods %s --car %s\n", a->piece, info->id);
    return 1;
}

int car_import_command(int argc, char **argv, ExchangeError *err)
{
    Args a;
    ModInfo info;
    if (!parse(argc, argv, &a, err) || !read_card(a.piece, &info, err)) return 0;
    Source high, low;
    memset(&high, 0, sizeof high), memset(&low, 0, sizeof low);
    Made m;
    memset(&m, 0, sizeof m);
    int ok = load(a.obj, a.textures, a.raw, &high, err) && (a.low == NULL || load(a.low, a.low_textures, a.raw, &low, err)) &&
             make(&a, &info, &high, a.low != NULL ? &low : NULL, &m, err) && write_out(&a, &info, &m, err);
    unload(&high), unload(&low);
    free(m.cfm), free(m.pdn);
    return ok;
}
