/* The paths of a car kit (dep85), relative to the kit's folder: the one place that spells them, so a rename is one edit. English only
 * (owner 2026-11-20). A kit is sources a user edits and build/, which car build writes and anything may delete:
 *
 *   mod.ini, card.png        the card
 *   model/near/bank/         the near model's textures: entries.tsv, bank.tsv, a PNG per bitmap, a .bin per palette (shpi_exchange.h)
 *   model/far/bank/          the far model's textures
 *   specs/specs.tsv          the PBS (optional: a car with none keeps the game's)
 *   specs/pdn.tsv            the PDN (optional)
 *   art-source/              what is edited with other programs (dep100): masks/ (a PSD a texture, <tex>.psd and <tex>_lo.psd, and dash.psd; a texture with no
 *                            PSD has its mask PNG there as its source) and mesh/ (near.obj and far.obj, the Blender OBJs, each with its .mtl, whose textures
 *                            are ../../model/<near|far>/bank/, and the .<label>.png copies Blender previews). Studio never writes into it
 *   art-generated/           what Studio writes, never edited by hand: model/near/mesh/ and model/far/mesh/ (the mesh tables the OBJ does not carry), car_masks/ (the masks, grey, as the game reads them: <tex>_mask.png over the near
 *                            texture <tex>, <tex>_lo_mask.png over the far one; no stem level, the build adds it), dash_masks/ (dash_mask.png and
 *                            dash_color.png, the same way) and .generated.tsv (a hash a generated PNG, for the warning on a hand-edited one; the
 *                            build ignores it). The textures stay in model/near/bank and model/far/bank at the kit's top until art-source/textures/ takes them
 *   photos/                  the showcase's pictures, <name>.png, named by mod.ini's photos= and photo_small=; the build puts them in the piece
 *   build/                   disposable: build/piece/ (the mod as the game installs it: mod.ini, card.png, SIMDATA/, car_masks/<STEM>/,
 *                            dash_masks/<STEM>/, the photos) and build/<id>.zip */
#ifndef TNFS_CAR_KIT_PATHS_H
#define TNFS_CAR_KIT_PATHS_H

typedef enum {
    CAR_KIT_MOD_INI,
    CAR_KIT_CARD,
    CAR_KIT_NEAR_MESH,      /* art-generated/model/near/mesh: the near mesh's tables (mesh.*.tsv) */
    CAR_KIT_NEAR_BANK,
    CAR_KIT_FAR_MESH,       /* art-generated/model/far/mesh */
    CAR_KIT_FAR_BANK,
    CAR_KIT_SPECS,
    CAR_KIT_PDN,
    CAR_KIT_ART_SOURCE,
    CAR_KIT_SRC_MASKS,      /* art-source/masks: <tex>.psd, <tex>_lo.psd, dash.psd, or the PNG that is a texture's source */
    CAR_KIT_SRC_MESH,       /* art-source/mesh: the Blender OBJs */
    CAR_KIT_SRC_NEAR_OBJ,   /* art-source/mesh/near.obj, with near.mtl and near.<label>.png beside it */
    CAR_KIT_SRC_FAR_OBJ,    /* art-source/mesh/far.obj */
    CAR_KIT_ART_GENERATED,
    CAR_KIT_GEN_MODEL,      /* art-generated/model: the tables of both meshes */
    CAR_KIT_GEN_CAR_MASKS,  /* art-generated/car_masks: flat, <tex>_mask.png; what the build and the editor read */
    CAR_KIT_GEN_DASH_MASKS, /* art-generated/dash_masks: flat, dash_mask.png and dash_color.png */
    CAR_KIT_GEN_MANIFEST,   /* art-generated/.generated.tsv: name and sha256 of each generated PNG; never copied to build/piece */
    CAR_KIT_PHOTOS,
    CAR_KIT_BUILD,
    CAR_KIT_BUILD_PIECE,
    CAR_KIT_README,
    CAR_KIT_MESH_OBJ,       /* the Blender OBJ's name in a CarKitFlat's mesh set (the kit's places are above) */
    CAR_KIT_SRC_MESH_TO_KIT,/* the kit's top, from art-source/mesh: what an MTL's texture paths start with, then model/<near|far>/bank */
    CAR_KIT_MODEL,          /* the folder of both models, near and far */
    CAR_KIT_PATHS
} CarKitPath;

/* the path, never NULL for one of the above */
const char *car_kit_path(CarKitPath which);

#endif
