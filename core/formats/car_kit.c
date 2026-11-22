/* See car_kit.h and car_kit_paths.h. */
#include "formats/car_kit.h"

#include "formats/car_import.h"
#include "formats/car_kit_paths.h"
#include "formats/cfm_build.h"
#include "formats/mesh_exchange.h"
#include "formats/obj_read.h"
#include "formats/pbs.h"
#include "formats/pdn.h"
#include "formats/qfs.h"
#include "formats/shpi_exchange.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const PATHS[CAR_KIT_PATHS] = {
    [CAR_KIT_MOD_INI] = "mod.ini",
    [CAR_KIT_CARD] = "card.png",
    [CAR_KIT_NEAR_MESH] = "art-generated/model/near/mesh",
    [CAR_KIT_MODEL] = "model",
    [CAR_KIT_NEAR_BANK] = "model/near/bank",
    [CAR_KIT_FAR_MESH] = "art-generated/model/far/mesh",
    [CAR_KIT_FAR_BANK] = "model/far/bank",
    [CAR_KIT_SPECS] = "specs/specs.tsv",
    [CAR_KIT_PDN] = "specs/pdn.tsv",
    [CAR_KIT_ART_SOURCE] = "art-source",
    [CAR_KIT_SRC_MASKS] = "art-source/masks",
    [CAR_KIT_SRC_MESH] = "art-source/mesh",
    [CAR_KIT_SRC_NEAR_OBJ] = "art-source/mesh/near.obj",
    [CAR_KIT_SRC_FAR_OBJ] = "art-source/mesh/far.obj",
    [CAR_KIT_GEN_MODEL] = "art-generated/model",
    [CAR_KIT_ART_GENERATED] = "art-generated",
    [CAR_KIT_GEN_CAR_MASKS] = "art-generated/car_masks",
    [CAR_KIT_GEN_DASH_MASKS] = "art-generated/dash_masks",
    [CAR_KIT_GEN_MANIFEST] = "art-generated/.generated.tsv",
    [CAR_KIT_PHOTOS] = "photos",
    [CAR_KIT_BUILD] = "build",
    [CAR_KIT_BUILD_PIECE] = "build/piece",
    [CAR_KIT_README] = "README.txt",
    [CAR_KIT_MESH_OBJ] = "mesh.edit.obj",
    [CAR_KIT_SRC_MESH_TO_KIT] = "../..",
};

const char *car_kit_path(CarKitPath which)
{
    return which >= 0 && which < CAR_KIT_PATHS ? PATHS[which] : "";
}

/* the stem mesh_export names a mesh folder's files by (mesh.obj, mesh.edit.obj, mesh.*.tsv) */
static const char MESH_STEM[] = "mesh";

const char *car_kit_readme(void)
{
    return "This folder is a car for Spirited Drive: its sources. Edit them, then Build (Studio) or `tnfs-studio car build` makes the car.\n"
           "\n"
           "  mod.ini, card.png       the car's card: its name, its slot, its picture\n"
           "  model/near/bank/        the near model's textures: one PNG each (paint them, RGB or on the palette). They stay here, at the kit's top, for now\n"
           "  model/far/bank/         the far model's textures\n"
           "  specs/specs.tsv         the car's physics (optional)\n"
           "  specs/pdn.tsv           its body size and AI (optional)\n"
           "  art-source/             what you edit with other programs: masks/ (a PSD a texture) and mesh/ (the OBJs)\n"
           "  art-source/mesh/        near.obj and far.obj: open them in the 3D editor and export back over them. Each has its .mtl, whose textures are\n"
           "                          ../../model/near/bank (far: ../../model/far/bank); that path is not tested in the 3D editor, so if a texture\n"
           "                          does not show, point the 3D editor at model/near/bank by hand\n"
           "  art-generated/          what Studio writes: do not edit it\n"
           "  art-generated/model/near/mesh/, art-generated/model/far/mesh/   the mesh tables the OBJ does not carry\n"
           "  art-generated/car_masks/   the colour masks, grey: frnt_mask.png goes over the texture model/near/bank/NNNN_frnt.png (same\n"
           "                          size); a _lo mask over model/far/bank\n"
           "  art-generated/dash_masks/  dash_mask.png (and dash_color.png), the same way\n"
           "  photos/                 your showcase pictures, NAME.png, listed in mod.ini's photos= (and photo_small=)\n"
           "  build/                  disposable: what a build makes (the game's files and the zip). Never edit it; delete it any time.\n";
}

void car_kit_flat_init(CarKitFlat *flat)
{
    for (int l = 0; l < CAR_KIT_LEVELS; ++l) fileset_init(&flat->mesh[l]), fileset_init(&flat->bank[l]);
    fileset_init(&flat->specs), fileset_init(&flat->pdn);
}

void car_kit_flat_free(CarKitFlat *flat)
{
    for (int l = 0; l < CAR_KIT_LEVELS; ++l) fileset_free(&flat->mesh[l]), fileset_free(&flat->bank[l]);
    fileset_free(&flat->specs), fileset_free(&flat->pdn);
}

void car_kit_built_free(CarKitBuilt *built)
{
    free(built->cfm), free(built->pbs), free(built->pdn);
    memset(built, 0, sizeof *built);
}

/* ---- write: a car's binaries as a kit's sources */

static const char *level_name(int level) { return level == CAR_KIT_NEAR ? "near" : "far"; }

static int write_level(const uint8_t *const child[CFM_CHILDREN], const size_t len[CFM_CHILDREN], int level, CarKitFlat *out, ExchangeError *err)
{
    char dir[96];   /* the MTL's way to the textures, from art-source/mesh */
    snprintf(dir, sizeof dir, "%s/%s", car_kit_path(CAR_KIT_SRC_MESH_TO_KIT), car_kit_path(level == CAR_KIT_NEAR ? CAR_KIT_NEAR_BANK : CAR_KIT_FAR_BANK));
    const MeshTextureBank bank = { child[2 * level + 1], len[2 * level + 1], dir };
    if (!mesh_export(child[2 * level], len[2 * level], MESH_STEM, &bank, 1, &out->mesh[level], err)) return 0;
    if (!shpi_export(child[2 * level + 1], len[2 * level + 1], NULL, &out->bank[level], err)) {   /* as the CFM holds it: bank.tsv keeps the size word */
        char why[sizeof err->message];
        memcpy(why, err->message, sizeof why);
        exchange_fail(err, "the %s bank: %s", level_name(level), why);
        return 0;
    }
    return 1;
}

int car_kit_write(const uint8_t *cfm, size_t cfm_len, const uint8_t *pbs, size_t pbs_len, const uint8_t *pdn, size_t pdn_len, CarKitFlat *out,
                  ExchangeError *err)
{
    const uint8_t *child[CFM_CHILDREN];
    size_t len[CFM_CHILDREN];
    if (!cfm_describe(cfm, cfm_len, child, len, err)) return 0;
    for (int l = 0; l < CAR_KIT_LEVELS; ++l)
        if (!write_level(child, len, l, out, err)) return 0;
    uint8_t pbs_record[PBS_SIZE], pdn_record[PDN_SIZE];
    if (pbs != NULL && !pbs_read(pbs_record, pbs, pbs_len)) { exchange_fail(err, "the PBS is not one"); return 0; }
    if (pbs != NULL && !pbs_export(pbs_record, &out->specs, err)) return 0;
    if (pdn != NULL && !pdn_read(pdn_record, pdn, pdn_len)) { exchange_fail(err, "the PDN is not one"); return 0; }
    return pdn == NULL || pdn_export(pdn_record, &out->pdn, err);
}

/* ---- build: the car's files from the kit's sources */

/* `level`'s mesh folder's OBJ, read */
static int level_obj(const CarKitFlat *in, int level, ObjModel *obj, ExchangeError *err)
{
    const SetFile *f = fileset_find(&in->mesh[level], car_kit_path(CAR_KIT_MESH_OBJ));
    if (f == NULL) {
        exchange_fail(err, "the %s mesh has no OBJ (%s)", level_name(level), car_kit_path(level == CAR_KIT_NEAR ? CAR_KIT_SRC_NEAR_OBJ : CAR_KIT_SRC_FAR_OBJ));
        return 0;
    }
    return obj_read(f->data, f->size, NULL, 0, obj, err);
}

/* the CFM: both banks from their flat files, both meshes by the raw builder with those banks and no donor */
static int build_cfm(const CarKitFlat *in, CarKitBuilt *out, ExchangeError *err)
{
    uint8_t *bank[CAR_KIT_LEVELS] = { NULL, NULL };
    size_t bank_len[CAR_KIT_LEVELS] = { 0, 0 };
    ObjModel obj[CAR_KIT_LEVELS];
    memset(obj, 0, sizeof obj);
    int ok = 1;
    for (int l = 0; ok && l < CAR_KIT_LEVELS; ++l) {
        ok = shpi_from_flat(&in->bank[l], NULL, &bank[l], &bank_len[l], &out->bank_stats[l], err);
        if (!ok) {
            char why[sizeof err->message];
            memcpy(why, err->message, sizeof why);
            exchange_fail(err, "%s: %s", car_kit_path(l == CAR_KIT_NEAR ? CAR_KIT_NEAR_BANK : CAR_KIT_FAR_BANK), why);
        }
        ok = ok && level_obj(in, l, &obj[l], err);
    }
    if (ok) {
        CarImportIn ci;
        memset(&ci, 0, sizeof ci);
        ci.high = &obj[CAR_KIT_NEAR], ci.low = &obj[CAR_KIT_FAR], ci.high_tables = &in->mesh[CAR_KIT_NEAR], ci.low_tables = &in->mesh[CAR_KIT_FAR];
        ci.raw = 1, ci.scale = 1.0;
        for (int l = 0; l < CAR_KIT_LEVELS; ++l) ci.bank[l] = bank[l], ci.bank_len[l] = bank_len[l];
        int32_t half[3];
        ok = car_import(&ci, &out->cfm, &out->cfm_len, half, err);
    }
    for (int l = 0; l < CAR_KIT_LEVELS; ++l) obj_free(&obj[l]), free(bank[l]);
    return ok;
}

/* a record's file: `import` onto zeros (the TSV covers every byte), then `pack` (sealed where it has a checksum, QFS) */
static int build_record(const FileSet *tsv, size_t size, int (*import)(uint8_t *, const FileSet *, ExchangeError *),
                        QfsResult (*pack)(const uint8_t *, uint8_t *, size_t, size_t *), uint8_t **out, size_t *out_len, ExchangeError *err)
{
    if (tsv->count == 0) return 1;   /* none in the kit: none made */
    uint8_t record[PBS_SIZE > PDN_SIZE ? PBS_SIZE : PDN_SIZE];
    memset(record, 0, sizeof record);
    if (!import(record, tsv, err)) return 0;
    const size_t cap = qfs_compress_bound(size);
    *out = malloc(cap);
    if (*out == NULL) { exchange_fail(err, "out of memory"); return 0; }
    if (pack(record, *out, cap, out_len) != QFS_OK) { exchange_fail(err, "cannot pack the record"); return 0; }
    return 1;
}

int car_kit_build(const CarKitFlat *in, CarKitBuilt *out, ExchangeError *err)
{
    memset(out, 0, sizeof *out);
    const int ok = build_cfm(in, out, err) && build_record(&in->specs, PBS_SIZE, pbs_import, pbs_pack, &out->pbs, &out->pbs_len, err) &&
                   build_record(&in->pdn, PDN_SIZE, pdn_import, pdn_pack, &out->pdn, &out->pdn_len, err);
    if (!ok) car_kit_built_free(out);
    return ok;
}
