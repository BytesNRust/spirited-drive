/* A car's model made from an OBJ and a donor car of the CD (dep59 tools B2; docs/modding/car-model.md, "Importing a model").
 *
 * The OBJ's objects say what they are, by name, case aside (the owner's decision 1): `wheel_fl`, `wheel_fr`, `wheel_rl`,
 * `wheel_rr` the wheels (the front two the steered parts left_turn and right_turn, the tyre frames on all four); `inside*` and
 * `outside*` those groups, everything else `surface`; `smok` and `fl0`..`rr1` an object with no face whose first vertex is the
 * point; `lt_frnt`, `rt_frnt`, `lt_rear`, `rt_rear` an object whose first face is the light's polygon. A point or a light the OBJ
 * lacks is made (fl0..rr1 from the wheel's bottom corners, smok a new vertex at the exhaust's usual place, a light the polygon
 * nearest its corner of the body) and noted. What is not drawn copies the donor: the polygons' flag bytes, the texture
 * references' words, the groups' word 0, the header's words, the tyre references and their bitmaps.
 *
 * The OBJ is Blender's (Y up, -Z forward, faces counter-clockwise from outside, uvs 0..1 from the bottom left); the mesh negates z
 * and scales, which makes its faces clockwise as the race wants them, and gives uvs in texels from the top. In raw mode the OBJ
 * is mesh_export's and its tables (`.tsv`, with lists.tsv) beside it give everything the OBJ does not carry. */
#ifndef TNFS_CAR_IMPORT_H
#define TNFS_CAR_IMPORT_H

#include "formats/exchange.h"
#include "formats/mesh_build.h"
#include "formats/obj_read.h"
#include "formats/shpi_build.h"

#include <stddef.h>
#include <stdint.h>

typedef void (*CarImportNote)(void *ctx, const char *text);   /* what was generated, guessed or clamped */

typedef struct {
    const uint8_t *donor_cfm;
    size_t donor_cfm_len;                  /* the donor car's CFM from the CD */
    const ObjModel *high, *low;            /* low NULL: the high mesh and its bank in the low-detail slot too (decision 3) */
    const FileSet *high_tables, *low_tables;   /* raw: mesh_export's tables beside each OBJ (any stem); else NULL */
    const FileSet *high_pngs, *low_pngs;   /* the textures, each under its map_Kd path as the MTL writes it */
    int raw;                               /* the OBJ is mesh_export's: the mesh's frame, units and texel uvs */
    double scale;                          /* mesh units per OBJ unit when not raw */
    double steer_degrees;                  /* 0: each front wheel turns as the donor's (car_steer_fit) */
    CarImportNote note;
    void *note_ctx;
} CarImportIn;

/* A bank in the making: its entries, and the pixels it owns. */
typedef struct {
    ShpiDescEntry *entry;
    size_t count;
    uint8_t **owned;
    size_t owned_count;
} CarImportBank;

/* the car's CFM (malloc'd) and its body's half sizes for the PDN (x y z, as car_piece_check measures them); 0 with the reason in
 * `err` */
int car_import(const CarImportIn *in, uint8_t **cfm, size_t *cfm_len, int32_t half[3], ExchangeError *err);

/* the steps, for the tests: a mesh (level 0 high, 1 low) and its bank from the OBJ, the donor's mesh and bank of that level */
int car_import_mesh(const CarImportIn *in, int level, const MeshDesc *donor, const ShpiDescEntry *donor_bank, size_t donor_count,
                    MeshDesc *out, CarImportBank *bank, ExchangeError *err);
void car_import_bank_free(CarImportBank *bank);
/* ((most - least) << 9) / 2 over the first real_vertices, the box from 0 (car_piece.c model_halves, car_model.c) */
void car_import_halves(const MeshDesc *mesh, int32_t half[3]);

#endif
