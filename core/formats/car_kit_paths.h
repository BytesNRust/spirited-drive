/* The paths of a car kit (dep85), relative to the kit's folder: the one place that spells them, so a rename is one edit. English only
 * (owner 2026-11-20). A kit is sources a user edits and build/, which car build writes and anything may delete:
 *
 *   mod.ini, card.png        the card
 *   model/near/mesh/         the near model: its Blender OBJ (mesh.blender.obj) and the tables beside it (mesh_export's)
 *   model/near/bank/         its textures: entries.tsv, bank.tsv, a PNG per bitmap, a .bin per palette (shpi_exchange.h)
 *   model/far/mesh/, model/far/bank/   the far model, the same way
 *   specs/specs.tsv          the PBS (optional: a car with none keeps the game's)
 *   specs/pdn.tsv            the PDN (optional)
 *   car_masks/               the masks, grey, as the game reads them: <tex>_mask.png over the near texture <tex>, <tex>_lo_mask.png over the
 *                            far one; the only mask source (colour masks are an export, not a source). No stem level: the build adds it
 *   dash_masks/              dash_mask.png (and dash_color.png), the same way
 *   photos/                  the showcase's pictures, <name>.png, named by mod.ini's photos= and photo_small=; the build puts them in the piece
 *   build/                   disposable: build/piece/ (the mod as the game installs it: mod.ini, card.png, SIMDATA/, car_masks/<STEM>/,
 *                            dash_masks/<STEM>/, the photos) and build/<id>.zip */
#ifndef TNFS_CAR_KIT_PATHS_H
#define TNFS_CAR_KIT_PATHS_H

typedef enum {
    CAR_KIT_MOD_INI,
    CAR_KIT_CARD,
    CAR_KIT_NEAR_MESH,
    CAR_KIT_NEAR_BANK,
    CAR_KIT_FAR_MESH,
    CAR_KIT_FAR_BANK,
    CAR_KIT_SPECS,
    CAR_KIT_PDN,
    CAR_KIT_CAR_MASKS,
    CAR_KIT_DASH_MASKS,
    CAR_KIT_PHOTOS,
    CAR_KIT_BUILD,
    CAR_KIT_BUILD_PIECE,
    CAR_KIT_README,
    CAR_KIT_MESH_OBJ,       /* the OBJ in a mesh folder: the user's Blender export */
    CAR_KIT_MESH_TO_BANK,   /* a mesh folder's bank, from the mesh folder (its MTL's textures) */
    CAR_KIT_MODEL,          /* the folder of both models, near and far */
    CAR_KIT_PATHS
} CarKitPath;

/* the path, never NULL for one of the above */
const char *car_kit_path(CarKitPath which);

#endif
