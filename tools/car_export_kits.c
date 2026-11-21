/* The originals as kits (dep100 item 6): car_export_kits CD_DIR DATA_DIR OUT_DIR exports every car Export original lists (the 20 with masks) as a kit in the
 * current layout, OUT_DIR/<stem>/ (editor/car_export_kit.h). tools/migrate_originals.py adds the PSDs and checks the masks. Exit 0 when every car was made. */
#include "car_export_kit.h"
#include "studio/car_kit.h"
#include "studio/fsio.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* 1 when the CFM the kit builds is the game's <stem>.CFM */
static int same_cfm(const ExportSource *src, const char *stem, const char *kit, char *why, size_t cap)
{
    char path[EXPORT_PATH], rel[64];
    ExchangeError err = { { 0 } };
    snprintf(rel, sizeof rel, "SIMDATA/CARFAMS/%s.CFM", stem);
    if (!fsio_find_ci(src->cd, rel, path, sizeof path)) return snprintf(why, cap, "the game has no %s", rel), 0;
    size_t want_len = 0, got_len = 0;
    uint8_t *want = fsio_read_file(path, &want_len, &err), *got = NULL;
    if (want == NULL) return snprintf(why, cap, "%s", err.message), 0;
    const int ok = car_kit_cfm_of_dir(kit, &got, &got_len, &err);
    if (!ok) snprintf(why, cap, "the kit does not build: %s", err.message);
    else if (got_len != want_len || memcmp(got, want, want_len) != 0) snprintf(why, cap, "the kit's CFM is not the original's (%zu bytes against %zu)", got_len, want_len);
    const int same = ok && got_len == want_len && memcmp(got, want, want_len) == 0;
    free(want), free(got);
    return same;
}

int main(int argc, char **argv)
{
    if (argc != 4) {
        fprintf(stderr, "usage: car_export_kits CD_DIR DATA_DIR OUT_DIR\n");
        return 2;
    }
    ExportSource src;
    snprintf(src.cd, sizeof src.cd, "%s", argv[1]);
    snprintf(src.data, sizeof src.data, "%s", argv[2]);
    mkdir(argv[3], 0777);
    int failed = 0, made = 0;
    for (int i = 0; car_export_car(i) != NULL; ++i) {
        char why[512] = "", name[64], dir[EXPORT_PATH];
        if (!car_export_available(&src, i, why, sizeof why)) {
            printf("skipped %s: %s\n", car_export_car(i)->folder, why);
            continue;
        }
        snprintf(name, sizeof name, "%s", car_export_car(i)->file);   /* the folder is the car's stem, lower case */
        for (char *c = name; *c != '\0'; ++c) *c = (char)tolower((unsigned char)*c);
        snprintf(dir, sizeof dir, "%s/%s", argv[3], name);
        if (!car_export_kit(&src, i, dir, why, sizeof why)) {
            printf("FAILED %s: %s\n", car_export_car(i)->folder, why), ++failed;
            continue;
        }
        printf("made %s\n", dir), ++made;
        if (!same_cfm(&src, car_export_car(i)->file, dir, why, sizeof why)) printf("FAILED %s: %s\n", car_export_car(i)->folder, why), ++failed;
        else printf("cfm %s: the kit builds the original's CFM byte for byte\n", car_export_car(i)->file);
    }
    printf("%d kits made, %d failed\n", made, failed);
    return failed != 0;
}
