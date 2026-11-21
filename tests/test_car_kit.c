/* dep85 step 2: a car kit of flat sources and its build. The kit's paths are spelled in one table (car_kit_paths.h), English only, each
 * once; a new kit's README names the folders a user edits and says build/ is disposable. car_kit_write lays a car's CFM, PBS and PDN out
 * as a kit's flat files and car_kit_build makes them again from those alone, no donor and no binary to start from: every car of the CD
 * (LDIABLO aside, docs/modding/car-model.md "Exceptions") gives its CFM byte for byte, and its PBS and PDN the same record (the files are
 * QFS streams, ours not EA's: the ones that are byte-equal too are counted). The CD part is skipped without original/. */
#include "formats/car_kit.h"
#include "formats/car_kit_paths.h"
#include "formats/cfm_build.h"
#include "formats/pbs.h"
#include "formats/pdn.h"
#include "test.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static void check_paths(void)
{
    static const char *const OLD[] = { "modelo", "coche", "mascaras", "copia", "rehacer", "cerca", "lejos", "nuevas" };
    for (int i = 0; i < CAR_KIT_PATHS; ++i) {
        const char *p = car_kit_path((CarKitPath)i);
        CHECK(p != NULL && p[0] != '\0' && p[0] != '/' && (strstr(p, "..") == NULL || i == CAR_KIT_MESH_TO_BANK) && strchr(p, '\\') == NULL);
        for (int j = 0; p != NULL && j < i; ++j) CHECK(strcmp(p, car_kit_path((CarKitPath)j)) != 0);
        for (size_t k = 0; p != NULL && k < sizeof OLD / sizeof OLD[0]; ++k) CHECK(strstr(p, OLD[k]) == NULL);
    }
    CHECK(strcmp(car_kit_path(CAR_KIT_BUILD), "build") == 0 && strcmp(car_kit_path(CAR_KIT_GEN_CAR_MASKS), "art-generated/car_masks") == 0 &&
          strcmp(car_kit_path(CAR_KIT_GEN_DASH_MASKS), "art-generated/dash_masks") == 0);
    CHECK(strcmp(car_kit_path(CAR_KIT_ART_SOURCE), "art-source") == 0 && strcmp(car_kit_path(CAR_KIT_SRC_MASKS), "art-source/masks") == 0 &&
          strcmp(car_kit_path(CAR_KIT_SRC_MESH), "art-source/mesh") == 0 && strcmp(car_kit_path(CAR_KIT_ART_GENERATED), "art-generated") == 0);
    CHECK(strcmp(car_kit_path(CAR_KIT_GEN_MANIFEST), "art-generated/.generated.tsv") == 0);   /* what Studio generated: a hash a file, kept out of the build */
    CHECK(strcmp(car_kit_path(CAR_KIT_NEAR_BANK), "model/near/bank") == 0 && strcmp(car_kit_path(CAR_KIT_FAR_MESH), "model/far/mesh") == 0);
}

static void check_readme(void)
{
    const char *text = car_kit_readme();
    CHECK(text != NULL && strstr(text, "disposable") != NULL && strstr(text, "_mask.png goes over the texture") != NULL);
    static const CarKitPath EDITED[] = { CAR_KIT_MOD_INI, CAR_KIT_CARD, CAR_KIT_NEAR_MESH, CAR_KIT_NEAR_BANK, CAR_KIT_FAR_MESH,
                                         CAR_KIT_FAR_BANK, CAR_KIT_SPECS, CAR_KIT_PDN, CAR_KIT_GEN_CAR_MASKS, CAR_KIT_GEN_DASH_MASKS, CAR_KIT_PHOTOS, CAR_KIT_BUILD };
    for (size_t i = 0; text != NULL && i < sizeof EDITED / sizeof EDITED[0]; ++i) CHECK(strstr(text, car_kit_path(EDITED[i])) != NULL);
}

static uint8_t *read_all(const char *path, size_t *size)
{
    *size = 0;
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    const long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *d = n > 0 ? malloc((size_t)n) : NULL;
    if (d != NULL && fread(d, 1, (size_t)n, f) != (size_t)n) free(d), d = NULL;
    fclose(f);
    if (d != NULL) *size = (size_t)n;
    return d;
}

static uint8_t *read_cd(const char *dir, const char *stem, const char *ext, size_t *size)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/original/SIMDATA/%s/%s.%s", TNFS_SOURCE_DIR, dir, stem, ext);
    return read_all(path, size);
}

/* which children of the built CFM are not the CD's, and where each first differs */
static void differs(const char *stem, const uint8_t *built, size_t m, const uint8_t *cd, size_t n)
{
    const uint8_t *a[CFM_CHILDREN], *b[CFM_CHILDREN];
    size_t la[CFM_CHILDREN], lb[CFM_CHILDREN];
    ExchangeError err = { { 0 } };
    if (!cfm_describe(built, m, a, la, &err) || !cfm_describe(cd, n, b, lb, &err)) { fprintf(stderr, "car_kit: %s: %s\n", stem, err.message); return; }
    for (int k = 0; k < CFM_CHILDREN; k++) {
        size_t at = 0;
        while (at < la[k] && at < lb[k] && a[k][at] == b[k][at]) at++;
        if (la[k] != lb[k] || at < la[k]) fprintf(stderr, "car_kit: %s: child %d is not the CD's (%zu bytes, the CD's %zu; first at 0x%zx)\n", stem, k, la[k], lb[k], at);
    }
}

typedef struct {
    int cars, cfm_same, pbs, pbs_record, pbs_file, pdn, pdn_record, pdn_file;
} Tally;

/* one car of the CD through its kit; the counts in `t` */
static void one_car(const char *stem, Tally *t)
{
    size_t cfm_len = 0, pbs_len = 0, pdn_len = 0;
    uint8_t *cfm = read_cd("CARFAMS", stem, "CFM", &cfm_len), *pbs = read_cd("CARSPECS", stem, "PBS", &pbs_len);
    uint8_t *pdn = read_cd("CARFAMS", stem, "PDN", &pdn_len);
    CarKitFlat flat;
    car_kit_flat_init(&flat);
    CarKitBuilt built;
    memset(&built, 0, sizeof built);
    ExchangeError err = { { 0 } };
    const int ok = cfm != NULL && car_kit_write(cfm, cfm_len, pbs, pbs_len, pdn, pdn_len, &flat, &err) && car_kit_build(&flat, &built, &err);
    if (!ok) fprintf(stderr, "car_kit: %s: %s\n", stem, err.message);
    t->cars++;
    const int cfm_same = ok && built.cfm_len == cfm_len && memcmp(built.cfm, cfm, cfm_len) == 0;
    if (ok && !cfm_same) differs(stem, built.cfm, built.cfm_len, cfm, cfm_len);
    t->cfm_same += cfm_same;
    uint8_t a[PBS_SIZE], b[PBS_SIZE], c[PDN_SIZE], d[PDN_SIZE];
    if (pbs != NULL) {
        t->pbs++;
        const int record = ok && built.pbs != NULL && pbs_read(a, pbs, pbs_len) && pbs_read(b, built.pbs, built.pbs_len) && memcmp(a, b, PBS_SIZE) == 0;
        if (!record) fprintf(stderr, "car_kit: %s: the PBS record is not the CD's\n", stem);
        t->pbs_record += record;
        t->pbs_file += record && built.pbs_len == pbs_len && memcmp(built.pbs, pbs, pbs_len) == 0;
    } else {
        CHECK(!ok || built.pbs == NULL);   /* no specs in the kit: no PBS made up */
    }
    if (pdn != NULL) {
        t->pdn++;
        const int record = ok && built.pdn != NULL && pdn_read(c, pdn, pdn_len) && pdn_read(d, built.pdn, built.pdn_len) && memcmp(c, d, PDN_SIZE) == 0;
        if (!record) fprintf(stderr, "car_kit: %s: the PDN record is not the CD's\n", stem);
        t->pdn_record += record;
        const int file = record && built.pdn_len == pdn_len && memcmp(built.pdn, pdn, pdn_len) == 0;
        t->pdn_file += file;
        if (record && !file) printf("car_kit: %s.PDN: the same record, another QFS stream\n", stem);
    } else {
        CHECK(!ok || built.pdn == NULL);
    }
    if (pbs != NULL && ok && built.pbs != NULL && !(built.pbs_len == pbs_len && memcmp(built.pbs, pbs, pbs_len) == 0))
        printf("car_kit: %s.PBS: the same record, another QFS stream\n", stem);
    car_kit_built_free(&built);
    car_kit_flat_free(&flat);
    free(cfm), free(pbs), free(pdn);
}

static void check_cd(void)
{
    char dir[512];
    snprintf(dir, sizeof dir, "%s/original/SIMDATA/CARFAMS", TNFS_SOURCE_DIR);
    DIR *h = opendir(dir);
    if (h == NULL) return;
    Tally t;
    memset(&t, 0, sizeof t);
    for (struct dirent *de; (de = readdir(h)) != NULL;) {
        const size_t n = strlen(de->d_name);
        if (n < 5 || n - 4 >= 32 || strcasecmp(de->d_name + n - 4, ".CFM") != 0) continue;
        char stem[32];
        memcpy(stem, de->d_name, n - 4), stem[n - 4] = '\0';
        if (strcmp(stem, "LDIABLO") == 0) continue;   /* its low mesh's 19-byte references: no built mesh is that */
        one_car(stem, &t);
    }
    closedir(h);
    printf("car_kit: %d cars, CFM byte for byte %d; PBS %d, record %d, file %d; PDN %d, record %d, file %d\n", t.cars, t.cfm_same, t.pbs,
           t.pbs_record, t.pbs_file, t.pdn, t.pdn_record, t.pdn_file);
    CHECK_EQ_INT(t.cars, 38);
    CHECK_EQ_INT(t.cfm_same, t.cars);
    CHECK_EQ_INT(t.pbs, 9);
    CHECK_EQ_INT(t.pbs_record, t.pbs);
    CHECK_EQ_INT(t.pdn_record, t.pdn);
}

int main(void)
{
    check_paths();
    check_readme();
    check_cd();
    return TEST_REPORT();
}
