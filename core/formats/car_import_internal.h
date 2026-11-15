/* What the car importer's files share (formats/car_import.h). Not for other callers. */
#ifndef TNFS_CAR_IMPORT_INTERNAL_H
#define TNFS_CAR_IMPORT_INTERNAL_H

#include "formats/car_import.h"

void ci_note(const CarImportIn *in, const char *format, ...) __attribute__((format(printf, 2, 3)));
/* *p grown to `n` items of `size` (realloc); 0 with "out of memory" in `err` */
int ci_grow(void **p, size_t n, size_t size, ExchangeError *err);

/* the mesh of `level` from the author's OBJ (car_import_mesh.c), or from mesh_export's OBJ and tables (car_import_tables.c);
 * `bank` gets the entries the mesh's texture references name */
int ci_obj_mesh(const CarImportIn *in, int level, const MeshDesc *donor, const ShpiDescEntry *donor_bank, size_t donor_count,
                MeshDesc *out, CarImportBank *bank, ExchangeError *err);
int ci_raw_mesh(const CarImportIn *in, int level, const MeshDesc *donor, const ShpiDescEntry *donor_bank, size_t donor_count,
                MeshDesc *out, CarImportBank *bank, ExchangeError *err);

/* banks (car_import_bank.c): `entry` of the donor kept as it is, no tail, no gap */
int ci_bank_keep(CarImportBank *bank, const ShpiDescEntry *entry, ExchangeError *err);
/* a PNG fitted to the bank's !PAL (it must already be in the bank) as a bitmap named `name` */
int ci_bank_png(CarImportBank *bank, const char name[4], const uint8_t *png, size_t len, const CarImportIn *in, ExchangeError *err);
const ShpiDescEntry *ci_bank_find(const ShpiDescEntry *entries, size_t n, const char name[4]);
/* the bank's palette entries first (!PAL, then !xxx on the high bank), as the race needs them */
int ci_bank_start(CarImportBank *bank, int level, const ShpiDescEntry *donor_bank, size_t donor_count, ExchangeError *err);

#endif
