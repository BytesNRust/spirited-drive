/* The car half of the reference mod (BYTES-0009 AC11): through the platform overlay the port's loader reads the
 * edited performance record (power +20 %, a valid checksum), the CFM is the edited one and still opens, and the
 * other cars come from the data root unchanged.
 *   test_reference_mod DATA_ROOT MODS_DIR */
#include "formats/chunk.h"
#include "formats/pbs.h"
#include "game/race/car_load.h"
#include "platform/platform.h"
#include "test.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *read_file(const char *data, const char *name, size_t *size)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/SIMDATA/%s", data, name);
    return platform_read_file(path, size);
}

static int load_specs(const char *data, const char *car, CarSpecs *specs, uint8_t record[PBS_SIZE])
{
    char name[64];
    snprintf(name, sizeof name, "CARSPECS/%s.PBS", car);
    size_t size = 0;
    uint8_t *file = read_file(data, name, &size);
    const int ok = file != NULL && car_specs_read(specs, file, size) && pbs_read(record, file, size);
    platform_free_file(file);
    return ok;
}

int main(int argc, char **argv)
{
    if (argc != 3) { fprintf(stderr, "usage: %s DATA_ROOT MODS_DIR\n", argv[0]); return 2; }
    const char *data = argv[1], *mods = argv[2];
    CarSpecs original, edited, other_original, other_edited;
    uint8_t original_record[PBS_SIZE], edited_record[PBS_SIZE], scratch[PBS_SIZE];

    CHECK(load_specs(data, "DVIPER", &original, original_record));   /* before the overlay: the data root's file */
    CHECK(load_specs(data, "ANSX", &other_original, scratch));
    CHECK(platform_set_mods(data, mods));
    CHECK(platform_mods_error() == NULL || platform_mods_error()[0] == '\0');
    CHECK(load_specs(data, "DVIPER", &edited, edited_record));
    CHECK(load_specs(data, "ANSX", &other_edited, scratch));

    CHECK_EQ_INT(edited.power, (int64_t)original.power * 12 / 10);   /* +20 %, as build.sh computes it */
    CHECK(edited.power > original.power);
    CHECK_EQ_INT(edited.mass, original.mass);             /* only the power changed */
    CHECK_EQ_INT(edited.drag, original.drag);
    CHECK(pbs_checksum_ok(edited_record));                /* the pack recomputed it: the game would not zero the power */
    CHECK_EQ_INT(other_edited.power, other_original.power);   /* the rest of the garage comes from the data root */

    size_t size = 0;
    uint8_t *cfm = read_file(data, "CARFAMS/DVIPER.CFM", &size);
    CHECK(cfm != NULL);
    if (cfm != NULL) {
        Chunk root;
        CHECK_EQ_INT(chunk_open(&root, cfm, size), CHUNK_OK);
        char path[1024];
        snprintf(path, sizeof path, "%s/SIMDATA/CARFAMS/DVIPER.CFM", data);
        FILE *f = fopen(path, "rb");           /* the data root's own file, bypassing the overlay */
        CHECK(f != NULL);
        if (f != NULL) {
            uint8_t *plain = malloc(size + 1);
            const size_t got = plain != NULL ? fread(plain, 1, size + 1, f) : 0;
            CHECK(got != size || memcmp(plain, cfm, size) != 0);   /* the mod's CFM is not the original */
            free(plain);
            fclose(f);
        }
        platform_free_file(cfm);
    }
    return TEST_REPORT();
}
