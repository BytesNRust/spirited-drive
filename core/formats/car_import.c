/* The car importer (formats/car_import.h): the donor taken apart, each level's mesh and bank made (car_import_mesh.c from the
 * author's OBJ, car_import_tables.c in raw mode), and the CFM put together again. Without a low OBJ the low slot holds the high
 * mesh and its bank (the owner's decision 3); in raw mode, which retouches the donor, the donor's own far model and bank stay. Raw
 * mode's banks are the donor's chunks byte for byte (fix/raw-import-banks): it only names the donor's bitmaps; or the piece's own,
 * when they are a retouch of the donor's (CarImportIn.own_cfm: car resize's other sizes survive a the 3D editor round trip); or a kit's own
 * banks (CarImportIn.bank, dep85), which are the car's whatever the donor has, and with which no donor is read. */
#include "formats/car_import_internal.h"
#include "formats/cfm_build.h"
#include "formats/chunk.h"

#include <stdlib.h>
#include <string.h>

enum { DONOR_BANK_CAP = 256 };

typedef struct {
    MeshDesc mesh;
    ShpiDescEntry bank[DONOR_BANK_CAP];
    size_t bank_count;
    const uint8_t *shpi;   /* the level's SHPI chunk as the donor has it */
    size_t shpi_len;
} DonorLevel;

static int donor_level(const uint8_t *const child[CFM_CHILDREN], const size_t len[CFM_CHILDREN], int level, DonorLevel *d, ExchangeError *err)
{
    Chunk c;
    Mesh m;
    memset(&d->mesh, 0, sizeof d->mesh);
    if (chunk_open(&c, child[2 * level], len[2 * level]) != CHUNK_OK || mesh_open(&m, &c) != MESH_OK) {
        exchange_fail(err, "the donor's %s mesh does not open", level == 0 ? "high" : "low");
        return 0;
    }
    d->shpi = child[2 * level + 1], d->shpi_len = len[2 * level + 1];
    return mesh_describe(&m, &d->mesh, err) && shpi_describe(child[2 * level + 1], len[2 * level + 1], d->bank, DONOR_BANK_CAP, &d->bank_count, err);
}

int car_import_mesh(const CarImportIn *in, int level, const MeshDesc *donor, const ShpiDescEntry *donor_bank, size_t donor_count,
                    MeshDesc *out, CarImportBank *bank, ExchangeError *err)
{
    if ((level == 0 ? in->high : in->low) == NULL) { exchange_fail(err, "no OBJ for the %s mesh", level == 0 ? "high" : "low"); return 0; }
    return in->raw ? ci_raw_mesh(in, level, donor, donor_bank, donor_count, out, bank, err)
                   : ci_obj_mesh(in, level, donor, donor_bank, donor_count, out, bank, err);
}

void car_import_halves(const MeshDesc *mesh, int32_t half[3])
{
    int32_t most[3] = { 0 }, least[3] = { 0 };
    for (uint32_t i = 0; i < mesh->real_vertices && i < mesh->vertex_count; i++) {
        const int32_t v[3] = { mesh->vertices[i].x, mesh->vertices[i].y, mesh->vertices[i].z };
        for (int k = 0; k < 3; k++) {
            if (v[k] > most[k]) most[k] = v[k];
            if (v[k] < least[k]) least[k] = v[k];
        }
    }
    for (int k = 0; k < 3; k++) half[k] = (int32_t)(((int64_t)(most[k] - least[k]) << 9) / 2);
}

/* the piece's own bank of `level` in place of the donor's when it names the donor's entries in its order with the donor's palettes,
 * perhaps with bitmaps of its own after them (raw, dep71 follow-ups: a retouch of the donor, whose bitmaps car resize may have given
 * other sizes, and car texture add bitmaps of its own); 1 when it took it */
static int own_bank(const CarImportIn *in, int level, DonorLevel *d)
{
    const uint8_t *child[CFM_CHILDREN];
    size_t len[CFM_CHILDREN], n = 0;
    ExchangeError ignored = { { 0 } };
    if (!in->raw || in->own_cfm == NULL || !cfm_describe(in->own_cfm, in->own_cfm_len, child, len, &ignored)) return 0;
    ShpiDescEntry *own = malloc(DONOR_BANK_CAP * sizeof *own);
    int same = own != NULL && shpi_describe(child[2 * level + 1], len[2 * level + 1], own, DONOR_BANK_CAP, &n, &ignored) && n >= d->bank_count;
    for (size_t i = 0; same && i < d->bank_count; i++) {   /* the donor's entries, and its palettes byte for byte (cars name theirs alike) */
        const int palette = own[i].type != 0x7b;
        same = memcmp(own[i].name, d->bank[i].name, 4) == 0 && own[i].type == d->bank[i].type &&
               (!palette || memcmp(own[i].data, d->bank[i].data, SHPI_PALETTE_BYTES) == 0);
    }
    for (size_t i = d->bank_count; same && i < n; i++) same = own[i].type == 0x7b;   /* then bitmaps of its own (car texture add) */
    if (same) {
        memcpy(d->bank, own, n * sizeof *own);
        d->bank_count = n;
        d->shpi = child[2 * level + 1], d->shpi_len = len[2 * level + 1];
    }
    free(own);
    return same;
}

/* one level's ORIP and SHPI chunks (malloc'd) */
static int level_chunks(const CarImportIn *in, int level, const DonorLevel *d, uint8_t **mesh, size_t *mesh_len, uint8_t **shpi,
                        size_t *shpi_len, int32_t half[3], ExchangeError *err)
{
    MeshDesc m;
    CarImportBank bank;
    if (!car_import_mesh(in, level, &d->mesh, d->bank, d->bank_count, &m, &bank, err)) return 0;
    int ok = mesh_build(&m, mesh, mesh_len, err);
    if (ok && in->raw) {   /* the donor's bank as it is */
        *shpi = malloc(d->shpi_len ? d->shpi_len : 1);
        if (*shpi == NULL) exchange_fail(err, "out of memory"), ok = 0;
        else memcpy(*shpi, d->shpi, d->shpi_len), *shpi_len = d->shpi_len;
    } else if (ok) {
        ok = shpi_build(bank.entry, bank.count, shpi, shpi_len, err);
    }
    if (ok && half != NULL) car_import_halves(&m, half);
    mesh_desc_free(&m);
    car_import_bank_free(&bank);
    return ok;
}

/* a kit's own bank of `level` (CarImportIn.bank) as the level's bank: no donor mesh (raw makes the mesh from its tables) */
static int kit_level(const CarImportIn *in, int level, DonorLevel *d, ExchangeError *err)
{
    memset(&d->mesh, 0, sizeof d->mesh);
    d->shpi = in->bank[level], d->shpi_len = in->bank_len[level];
    return shpi_describe(d->shpi, d->shpi_len, d->bank, DONOR_BANK_CAP, &d->bank_count, err);
}

/* a kit's build (CarImportIn.bank): both levels from their OBJs and tables with the kit's banks, no donor read */
static int kit_import(const CarImportIn *in, DonorLevel d[2], uint8_t *made[CFM_CHILDREN], size_t made_len[CFM_CHILDREN], int32_t half[3],
                      ExchangeError *err)
{
    if (!in->raw || in->bank[0] == NULL || in->bank[1] == NULL) { exchange_fail(err, "a kit's banks are given for both models, in raw mode"); return 0; }
    if (in->low == NULL) { exchange_fail(err, "a kit's far model needs its own OBJ (--low): there is no donor's far model to keep"); return 0; }
    return kit_level(in, 0, &d[0], err) && level_chunks(in, 0, &d[0], &made[0], &made_len[0], &made[1], &made_len[1], half, err) &&
           kit_level(in, 1, &d[1], err) && level_chunks(in, 1, &d[1], &made[2], &made_len[2], &made[3], &made_len[3], NULL, err);
}

int car_import(const CarImportIn *in, uint8_t **cfm, size_t *cfm_len, int32_t half[3], ExchangeError *err)
{
    const uint8_t *child[CFM_CHILDREN];
    size_t len[CFM_CHILDREN];
    *cfm = NULL, *cfm_len = 0;
    const int kit = in->bank[0] != NULL || in->bank[1] != NULL;
    if (!kit && !cfm_describe(in->donor_cfm, in->donor_cfm_len, child, len, err)) return 0;
    DonorLevel *d = calloc(2, sizeof *d);
    if (d == NULL) { exchange_fail(err, "out of memory"); return 0; }
    uint8_t *made[CFM_CHILDREN] = { NULL };
    size_t made_len[CFM_CHILDREN] = { 0 };
    if (kit) {
        int ok = kit_import(in, d, made, made_len, half, err) && cfm_build((const uint8_t *const *)made, made_len, cfm, cfm_len, err);
        for (int k = 0; k < CFM_CHILDREN; k++) free(made[k]);
        mesh_desc_free(&d[0].mesh), mesh_desc_free(&d[1].mesh);
        free(d);
        return ok;
    }
    int ok = donor_level(child, len, 0, &d[0], err);
    if (ok) own_bank(in, 0, &d[0]);
    ok = ok && level_chunks(in, 0, &d[0], &made[0], &made_len[0], &made[1], &made_len[1], half, err);
    if (ok && (in->low != NULL || in->raw)) ok = donor_level(child, len, 1, &d[1], err);
    const int own_far = ok && own_bank(in, 1, &d[1]);
    if (ok && in->low != NULL) ok = level_chunks(in, 1, &d[1], &made[2], &made_len[2], &made[3], &made_len[3], NULL, err);
    if (ok && own_far && in->low == NULL) {   /* raw, no low OBJ, the piece's far model a retouch of the donor's: it stays */
        const uint8_t *oc[CFM_CHILDREN];
        size_t ol[CFM_CHILDREN];
        ok = cfm_describe(in->own_cfm, in->own_cfm_len, oc, ol, err);
        if (ok) child[2] = oc[2], len[2] = ol[2], child[3] = oc[3], len[3] = ol[3];
    }
    if (ok) {
        /* the low slot: the low OBJ's; without one, raw keeps the donor's, and an OBJ import repeats the high level */
        const int own_low = in->low != NULL, donor_low = !own_low && in->raw;
        const uint8_t *const parts[CFM_CHILDREN] = { made[0], made[1], own_low ? made[2] : donor_low ? child[2] : made[0],
                                                     own_low ? made[3] : donor_low ? child[3] : made[1] };
        const size_t parts_len[CFM_CHILDREN] = { made_len[0], made_len[1], own_low ? made_len[2] : donor_low ? len[2] : made_len[0],
                                                 own_low ? made_len[3] : donor_low ? len[3] : made_len[1] };
        ok = cfm_build(parts, parts_len, cfm, cfm_len, err);
    }
    for (int k = 0; k < CFM_CHILDREN; k++) free(made[k]);
    mesh_desc_free(&d[0].mesh), mesh_desc_free(&d[1].mesh);
    free(d);
    return ok;
}
