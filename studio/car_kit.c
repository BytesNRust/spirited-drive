/* See car_kit.h. */
#include "studio/car_kit.h"

#include "formats/car_kit.h"
#include "formats/car_kit_paths.h"
#include "formats/car_piece.h"
#include "formats/mod_ini.h"
#include "studio/car_import.h"
#include "studio/fsio.h"
#include "studio/mod_pack.h"

#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* `dir`/`rel` into `out` */
static void joined(char *out, size_t cap, const char *dir, const char *rel) { snprintf(out, cap, "%s/%s", dir, rel); }

/* 1 when `dir` holds anything */
static int has_files(const char *dir)
{
    DIR *d = opendir(dir);
    if (d == NULL) return 0;
    int any = 0;
    for (struct dirent *e; !any && (e = readdir(d)) != NULL;) any = strcmp(e->d_name, ".") != 0 && strcmp(e->d_name, "..") != 0;
    closedir(d);
    return any;
}

/* the file `rel` of `root` (any case) added to `set` as `name`; 1 also when there is none and it may be missing */
static int add_file(const char *root, const char *rel, FileSet *set, const char *name, int needed, ExchangeError *err)
{
    char path[PATH_MAX];
    if (!fsio_find_ci(root, rel, path, sizeof path)) {
        if (needed) exchange_fail(err, "%s has no %s", root, rel);
        return !needed;
    }
    size_t size = 0;
    uint8_t *data = fsio_read_file(path, &size, err);
    if (data == NULL) return 0;
    if (!fileset_add_owned(set, name, data, size)) { free(data); exchange_fail(err, "cannot add %s", name); return 0; }
    return 1;
}

/* the files below `root`/`rel` (none when it is missing) into `set` under `prefix` */
static int add_tree(const char *root, const char *rel, FileSet *set, const char *prefix, ExchangeError *err)
{
    char dir[PATH_MAX];
    if (!fsio_find_ci(root, rel, dir, sizeof dir)) return 1;
    FileSet tree;
    fileset_init(&tree);
    int ok = fsio_read_tree(dir, &tree, err) && fileset_move_prefixed(set, &tree, prefix);
    if (!ok && err->message[0] == '\0') exchange_fail(err, "%s: two files of one name", dir);
    fileset_free(&tree);
    return ok;
}

/* ---- car kit */

/* the car's three files from the piece (the PDN and PBS may be missing): their bytes, owned by `files` */
static int car_files(const char *piece, const char *stem, FileSet *files, ExchangeError *err)
{
    char paths[CAR_PIECE_FILES][CAR_PIECE_PATH];
    car_piece_paths(stem, paths);
    return add_file(piece, paths[0], files, "cfm", 1, err) && add_file(piece, paths[1], files, "pdn", 0, err) &&
           add_file(piece, paths[2], files, "pbs", 0, err);
}

static const char *const LEVEL[CAR_KIT_LEVELS] = { "near", "far" };
static const char EDIT_PREFIX[] = "mesh.edit.";   /* what a CarKitFlat's mesh set calls the the 3D editor files: mesh.edit.obj, .mtl, .<label>.png */

/* `data` with each "mesh.edit." made `to` (the OBJ's mtllib line, the MTL's map_Kd names), in a new buffer; NULL if out of memory */
static uint8_t *rename_in(const uint8_t *data, size_t size, const char *to, size_t *out_size)
{
    const size_t from_len = sizeof EDIT_PREFIX - 1, to_len = strlen(to);
    uint8_t *out = malloc(size * (to_len > from_len ? to_len : from_len) / from_len + 1), *w = out;
    if (out == NULL) return NULL;
    for (size_t i = 0; i < size;) {
        if (size - i >= from_len && memcmp(data + i, EDIT_PREFIX, from_len) == 0) memcpy(w, to, to_len), w += to_len, i += from_len;
        else *w++ = data[i++];
    }
    *out_size = (size_t)(w - out);
    return out;
}

/* one level's mesh files into the kit: the the 3D editor set as art-source/mesh/<near|far>.obj, .mtl, .<label>.png (named for itself); the tables
 * into art-generated/model/<near|far>/mesh, with the game's mesh.obj (the build reads it); mesh.mtl, which it does not, is left out */
static int mesh_into(const FileSet *mesh, int level, FileSet *out)
{
    char name[PATH_MAX], to[32];
    snprintf(to, sizeof to, "%s.", LEVEL[level]);
    const size_t prefix_len = sizeof EDIT_PREFIX - 1;
    int ok = 1;
    for (size_t i = 0; ok && i < mesh->count; ++i) {
        const SetFile *f = &mesh->file[i];
        const size_t n = strlen(f->name);
        if (strncmp(f->name, EDIT_PREFIX, prefix_len) == 0) {
            snprintf(name, sizeof name, "%s/%s%s", car_kit_path(CAR_KIT_SRC_MESH), to, f->name + prefix_len);
            const int text = n > 4 && (strcmp(f->name + n - 4, ".obj") == 0 || strcmp(f->name + n - 4, ".mtl") == 0);
            size_t size = f->size;
            uint8_t *data = text ? rename_in(f->data, f->size, to, &size) : NULL;
            ok = text ? data != NULL && fileset_add_owned(out, name, data, size) : fileset_add(out, name, f->data, f->size);
            if (text && !ok) free(data);
        } else if ((n > 4 && strcmp(f->name + n - 4, ".tsv") == 0) || strcmp(f->name, "mesh.obj") == 0) {   /* the build reads the tables and the game's own mesh.obj */
            snprintf(name, sizeof name, "%s/%s", car_kit_path(level == CAR_KIT_NEAR ? CAR_KIT_NEAR_MESH : CAR_KIT_FAR_MESH), f->name);
            ok = fileset_add(out, name, f->data, f->size);
        }
    }
    return ok;
}

/* the kit's flat model and specs files of `flat`, each under its folder of the path table */
static int flat_into(CarKitFlat *flat, FileSet *out)
{
    static const CarKitPath BANK[CAR_KIT_LEVELS] = { CAR_KIT_NEAR_BANK, CAR_KIT_FAR_BANK };
    char prefix[PATH_MAX];
    int ok = 1;
    for (int l = 0; ok && l < CAR_KIT_LEVELS; ++l) {
        ok = mesh_into(&flat->mesh[l], l, out);
        snprintf(prefix, sizeof prefix, "%s/", car_kit_path(BANK[l]));
        ok = ok && fileset_move_prefixed(out, &flat->bank[l], prefix);
    }
    for (size_t i = 0; ok && i < flat->specs.count; ++i) ok = fileset_add(out, car_kit_path(CAR_KIT_SPECS), flat->specs.file[i].data, flat->specs.file[i].size);
    for (size_t i = 0; ok && i < flat->pdn.count; ++i) ok = fileset_add(out, car_kit_path(CAR_KIT_PDN), flat->pdn.file[i].data, flat->pdn.file[i].size);
    return ok;
}

/* the piece's card, masks (their stem level dropped) and photos into the kit's places */
static int piece_extras(const char *piece, const ModInfo *info, FileSet *out, ExchangeError *err)
{
    char rel[PATH_MAX], prefix[PATH_MAX];
    int ok = add_file(piece, "mod.ini", out, car_kit_path(CAR_KIT_MOD_INI), 1, err) && add_file(piece, "card.png", out, car_kit_path(CAR_KIT_CARD), 0, err);
    snprintf(rel, sizeof rel, "car_masks/%s", info->stem), snprintf(prefix, sizeof prefix, "%s/", car_kit_path(CAR_KIT_GEN_CAR_MASKS));
    ok = ok && add_tree(piece, rel, out, prefix, err);
    snprintf(rel, sizeof rel, "dash_masks/%s", info->stem), snprintf(prefix, sizeof prefix, "%s/", car_kit_path(CAR_KIT_GEN_DASH_MASKS));
    ok = ok && add_tree(piece, rel, out, prefix, err);
    for (int i = 0; ok && i <= info->photo_count; ++i) {   /* photos=, then photo_small= */
        const char *name = i < info->photo_count ? info->photo[i] : info->photo_small;
        snprintf(rel, sizeof rel, "%s/%s", car_kit_path(CAR_KIT_PHOTOS), name);
        if (name[0] != '\0' && fileset_find(out, rel) == NULL) ok = add_file(piece, name, out, rel, 0, err);
    }
    const char *readme = car_kit_readme();
    return ok && fileset_add(out, car_kit_path(CAR_KIT_README), (const uint8_t *)readme, strlen(readme));
}

static int make_kit(const char *piece, const char *kit, ExchangeError *err)
{
    ModInfo info;
    if (!car_piece_card(piece, &info, err)) return 0;
    FileSet bin, out;
    fileset_init(&bin), fileset_init(&out);
    CarKitFlat flat;
    car_kit_flat_init(&flat);
    int ok = car_files(piece, info.stem, &bin, err);
    if (ok) {
        const SetFile *c = fileset_find(&bin, "cfm"), *s = fileset_find(&bin, "pbs"), *d = fileset_find(&bin, "pdn");
        ok = car_kit_write(c->data, c->size, s ? s->data : NULL, s ? s->size : 0, d ? d->data : NULL, d ? d->size : 0, &flat, err);
    }
    if (ok && !flat_into(&flat, &out)) exchange_fail(err, "cannot lay the model out"), ok = 0;
    ok = ok && piece_extras(piece, &info, &out, err) && fsio_write_tree(kit, &out, 0, err);
    if (ok) printf("kit %s: %zu files; build it with tnfs-studio car build %s\n", kit, out.count, kit);
    car_kit_flat_free(&flat);
    fileset_free(&bin), fileset_free(&out);
    return ok;
}

int car_kit_from_piece(const char *piece, const char *kit, ExchangeError *err)
{
    return make_kit(piece, kit, err);
}

int car_kit_command(int argc, char **argv, ExchangeError *err)
{
    if (argc != 4) { exchange_fail(err, "usage: tnfs-studio car kit PIECE_DIR KIT_DIR"); return 0; }
    if (has_files(argv[3])) { exchange_fail(err, "%s has files already: a kit is made into an empty folder", argv[3]); return 0; }
    return make_kit(argv[2], argv[3], err);
}

/* ---- car build */

/* the kit's sources as car_kit_build reads them */
static int read_flat(const char *kit, CarKitFlat *flat, ExchangeError *err)
{
    static const CarKitPath MESH[CAR_KIT_LEVELS] = { CAR_KIT_NEAR_MESH, CAR_KIT_FAR_MESH }, BANK[CAR_KIT_LEVELS] = { CAR_KIT_NEAR_BANK, CAR_KIT_FAR_BANK },
                            OBJ[CAR_KIT_LEVELS] = { CAR_KIT_SRC_NEAR_OBJ, CAR_KIT_SRC_FAR_OBJ };
    char dir[PATH_MAX];
    for (int l = 0; l < CAR_KIT_LEVELS; ++l) {
        joined(dir, sizeof dir, kit, car_kit_path(MESH[l]));
        if (!fsio_read_tree(dir, &flat->mesh[l], err)) return 0;   /* the tables */
        if (!add_file(kit, car_kit_path(OBJ[l]), &flat->mesh[l], car_kit_path(CAR_KIT_MESH_OBJ), 1, err)) return 0;
        joined(dir, sizeof dir, kit, car_kit_path(BANK[l]));
        if (!fsio_read_tree(dir, &flat->bank[l], err)) return 0;
    }
    return add_file(kit, car_kit_path(CAR_KIT_SPECS), &flat->specs, "specs.tsv", 0, err) && add_file(kit, car_kit_path(CAR_KIT_PDN), &flat->pdn, "pdn.tsv", 0, err);
}

/* the piece as the game installs it: the card, the car's files, the masks under the stem, the photos at the root */
static int piece_files(const char *kit, const ModInfo *info, const CarKitBuilt *built, FileSet *out, ExchangeError *err)
{
    char paths[CAR_PIECE_FILES][CAR_PIECE_PATH], prefix[PATH_MAX];
    car_piece_paths(info->stem, paths);
    int ok = add_file(kit, car_kit_path(CAR_KIT_MOD_INI), out, "mod.ini", 1, err) && add_file(kit, car_kit_path(CAR_KIT_CARD), out, "card.png", 0, err) &&
             fileset_add(out, paths[0], built->cfm, built->cfm_len) && (built->pdn == NULL || fileset_add(out, paths[1], built->pdn, built->pdn_len)) &&
             (built->pbs == NULL || fileset_add(out, paths[2], built->pbs, built->pbs_len));
    snprintf(prefix, sizeof prefix, "car_masks/%s/", info->stem);
    ok = ok && add_tree(kit, car_kit_path(CAR_KIT_GEN_CAR_MASKS), out, prefix, err);
    snprintf(prefix, sizeof prefix, "dash_masks/%s/", info->stem);
    ok = ok && add_tree(kit, car_kit_path(CAR_KIT_GEN_DASH_MASKS), out, prefix, err);
    return ok && add_tree(kit, car_kit_path(CAR_KIT_PHOTOS), out, "", err);
}

/* build/piece/ written again from `files` and zipped as build/<id>.zip (mod pack: validated, the installation's own check); for an original's stem only build/piece/ */
static int write_build(const char *kit, const ModInfo *info, const FileSet *files, int verbose, ExchangeError *err)
{
    const int original = car_piece_stem_is_original(info->stem);   /* an exported original: its car file for the Preview, and no package (nothing to validate or install) */
    char dir[PATH_MAX], zip[PATH_MAX];
    joined(dir, sizeof dir, kit, car_kit_path(CAR_KIT_BUILD_PIECE));
    snprintf(zip, sizeof zip, "%s/%s/%s.zip", kit, car_kit_path(CAR_KIT_BUILD), info->id[0] != '\0' ? info->id : info->stem);
    if (fsio_exists(dir) && !fsio_remove_tree(dir)) { exchange_fail(err, "cannot empty %s", dir); return 0; }
    if (!fsio_write_tree(dir, files, 0, err)) return 0;
    if (original) {
        if (verbose) printf("built %s: the car's files only, an original car has no package\n", dir);
        return 1;
    }
    char *pack[] = { "mod", "pack", dir, zip, "--force" };
    if (!mod_command(5, pack, err)) return 0;
    if (verbose) printf("built %s and %s\n", dir, zip);
    return 1;
}

int car_kit_build_dir(const char *kit, int verbose, ExchangeError *err)
{
    char obj[PATH_MAX], old[PATH_MAX];
    joined(obj, sizeof obj, kit, car_kit_path(CAR_KIT_SRC_NEAR_OBJ));
    snprintf(old, sizeof old, "%s/model/near/mesh/%s", kit, car_kit_path(CAR_KIT_MESH_OBJ));   /* where the first layout kept it */
    if (!fsio_exists(obj) && fsio_exists(old)) {
        exchange_fail(err, "%s: old layout (model/near/mesh/): not built. Make a new kit with tnfs-studio car kit; its OBJs are in %s/", kit, car_kit_path(CAR_KIT_SRC_MESH));
        return 0;
    }
    if (!fsio_exists(obj)) { exchange_fail(err, "%s is not a car kit: it has no %s (tnfs-studio car kit makes one)", kit, car_kit_path(CAR_KIT_SRC_NEAR_OBJ)); return 0; }
    ModInfo info;
    if (!car_piece_card(kit, &info, err)) return 0;
    CarKitFlat flat;
    car_kit_flat_init(&flat);
    CarKitBuilt built;
    memset(&built, 0, sizeof built);
    FileSet files;
    fileset_init(&files);
    int ok = read_flat(kit, &flat, err) && car_kit_build(&flat, &built, err) && piece_files(kit, &info, &built, &files, err) && write_build(kit, &info, &files, verbose, err);
    for (int l = 0; ok && verbose && l < CAR_KIT_LEVELS; ++l)
        if (built.bank_stats[l].reassigned > 0) printf("%s: %u pixels moved to the nearest palette colour\n", car_kit_path(l == 0 ? CAR_KIT_NEAR_BANK : CAR_KIT_FAR_BANK), built.bank_stats[l].reassigned);
    fileset_free(&files);
    car_kit_built_free(&built);
    car_kit_flat_free(&flat);
    return ok;
}

int car_build_command(int argc, char **argv, ExchangeError *err)
{
    if (argc != 3) { exchange_fail(err, "usage: tnfs-studio car build KIT_DIR"); return 0; }
    return car_kit_build_dir(argv[2], 1, err);
}

int car_kit_cfm_of_dir(const char *kit, uint8_t **cfm, size_t *cfm_len, ExchangeError *err)
{
    CarKitFlat flat;
    car_kit_flat_init(&flat);
    CarKitBuilt built;
    memset(&built, 0, sizeof built);
    int ok = read_flat(kit, &flat, err) && car_kit_build(&flat, &built, err);
    if (ok) {
        *cfm = built.cfm, *cfm_len = built.cfm_len;
        built.cfm = NULL;
    }
    car_kit_built_free(&built);
    car_kit_flat_free(&flat);
    return ok;
}
