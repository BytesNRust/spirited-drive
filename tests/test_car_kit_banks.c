/* dep85 (a): a car kit gives car_import its own banks (CarImportIn.bank), and its model comes back from its the 3D editor OBJ and tables with
 * those banks and no donor: every car of the CD, the traffic and police ones too, rebuilt byte for byte. Needs the CD in original/
 * (skipped without it, as CI is). */
#include "formats/car_import.h"
#include "formats/cfm_build.h"
#include "formats/mesh_exchange.h"
#include "formats/obj_read.h"
#include "test.h"
#include "modeller_sim.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { NAME_CAP = 64 };

static uint8_t *read_file(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    const long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *d = n > 0 ? malloc((size_t)n) : NULL;
    if (d != NULL && fread(d, 1, (size_t)n, f) != (size_t)n) free(d), d = NULL;
    fclose(f);
    *size = d != NULL ? (size_t)n : 0;
    return d;
}

static uint8_t *read_cd(const char *stem, size_t *size)
{
    char path[512];
    snprintf(path, sizeof path, "%s/original/SIMDATA/CARFAMS/%s.CFM", TNFS_SOURCE_DIR, stem);
    return read_file(path, size);
}

/* one level of `cfm` as a kit holds it: the mesh's tables (mesh_export, its bank beside it) and its the 3D editor OBJ as the 3D editor writes it back
 * (`grouped`: its faces in material blocks, as the 3D editor may write them) */
static int kit_level(const uint8_t *const child[CFM_CHILDREN], const size_t len[CFM_CHILDREN], int level, int grouped, FileSet *tables,
                     ObjModel *obj, ExchangeError *err)
{
    const MeshTextureBank bank = { child[2 * level + 1], len[2 * level + 1], level == 0 ? "../c1" : "../c3" };
    if (!mesh_export(child[2 * level], len[2 * level], "mesh", &bank, 1, tables, err)) return 0;
    const SetFile *f = fileset_find(tables, "mesh.edit.obj");
    if (f == NULL) { exchange_fail(err, "no mesh.edit.obj"); return 0; }
    char *plain = malloc(f->size + 1);
    if (plain == NULL) { exchange_fail(err, "out of memory"); return 0; }
    memcpy(plain, f->data, f->size), plain[f->size] = '\0';
    char *faces = grouped ? by_material(plain) : NULL;
    char *edited = modeller_sim(faces != NULL ? faces : plain);
    free(faces);
    const int ok = obj_read((const uint8_t *)edited, strlen(edited), NULL, 0, obj, err);
    free(edited), free(plain);
    return ok;
}

/* `cfm` rebuilt as a kit builds it: its the 3D editor OBJs and tables, its own banks given, `donor` (NULL: none) (malloc'd; NULL with `err`) */
static uint8_t *kit_build(const uint8_t *cfm, size_t n, const uint8_t *donor, size_t donor_len, int grouped, size_t *out_len, ExchangeError *err)
{
    const uint8_t *child[CFM_CHILDREN];
    size_t len[CFM_CHILDREN];
    FileSet hi, lo;
    fileset_init(&hi), fileset_init(&lo);
    ObjModel high, far;
    memset(&high, 0, sizeof high), memset(&far, 0, sizeof far);
    uint8_t *out = NULL;
    *out_len = 0;
    int ok = cfm_describe(cfm, n, child, len, err) && kit_level(child, len, 0, grouped, &hi, &high, err) && kit_level(child, len, 1, grouped, &lo, &far, err);
    if (ok) {
        CarImportIn in;
        memset(&in, 0, sizeof in);
        in.donor_cfm = donor, in.donor_cfm_len = donor_len;
        in.high = &high, in.low = &far, in.high_tables = &hi, in.low_tables = &lo, in.raw = 1, in.scale = 1.0;
        in.bank[0] = child[1], in.bank_len[0] = len[1], in.bank[1] = child[3], in.bank_len[1] = len[3];
        int32_t half[3];
        ok = car_import(&in, &out, out_len, half, err);
    }
    obj_free(&high), obj_free(&far);
    fileset_free(&hi), fileset_free(&lo);
    if (!ok) free(out), out = NULL;
    return out;
}

/* every CFM of the CD, by its stem, into `stems` (at most `cap`) */
static int cd_stems(char stems[][NAME_CAP], int cap)
{
    char dir[512];
    snprintf(dir, sizeof dir, "%s/original/SIMDATA/CARFAMS", TNFS_SOURCE_DIR);
    DIR *d = opendir(dir);
    if (d == NULL) return 0;
    int count = 0;
    for (struct dirent *e; (e = readdir(d)) != NULL && count < cap;) {
        const size_t l = strlen(e->d_name);
        if (l > 4 && l - 4 < NAME_CAP && strcasecmp(e->d_name + l - 4, ".CFM") == 0) memcpy(stems[count], e->d_name, l - 4), stems[count++][l - 4] = '\0';
    }
    closedir(d);
    return count;
}

/* which children of the built CFM are not the CD's, and where the first of them first differs */
static void differs(const char *stem, const uint8_t *built, size_t m, const uint8_t *cd, size_t n)
{
    const uint8_t *a[CFM_CHILDREN], *b[CFM_CHILDREN];
    size_t la[CFM_CHILDREN], lb[CFM_CHILDREN];
    ExchangeError err = { { 0 } };
    if (!cfm_describe(built, m, a, la, &err) || !cfm_describe(cd, n, b, lb, &err)) { printf("car_kit_banks: %s: %s\n", stem, err.message); return; }
    for (int k = 0; k < CFM_CHILDREN; k++) {
        size_t at = 0;
        while (at < la[k] && at < lb[k] && a[k][at] == b[k][at]) at++;
        if (la[k] != lb[k] || at < la[k]) printf("car_kit_banks: %s: child %d is not the CD's (%zu bytes, the CD's %zu; first at 0x%zx)\n", stem, k, la[k], lb[k], at);
    }
}

/* every car of the CD, race, rival, traffic and police: its kit's build with no donor is the CD's CFM. LDIABLO is left out by name: its
 * low mesh's texture references are 19 bytes, not 20, so the tables after them sit 3 bytes early, which no built mesh does; the game
 * never opens it (docs/modding/car-model.md, Exceptions) */
static void test_every_car_without_a_donor(void)
{
    static char stems[64][NAME_CAP];
    const int count = cd_stems(stems, 64);
    CHECK_EQ_INT(count, 39);
    int same = 0, tried = 0;
    for (int i = 0; i < count; i++) {
        if (strcmp(stems[i], "LDIABLO") == 0) continue;
        tried++;
        size_t n = 0, m = 0;
        uint8_t *cd = read_cd(stems[i], &n);
        ExchangeError err = { { 0 } };
        uint8_t *built = cd != NULL ? kit_build(cd, n, NULL, 0, 0, &m, &err) : NULL;
        const int ok = built != NULL && m == n && memcmp(built, cd, n) == 0;
        if (!ok && built == NULL) printf("car_kit_banks: %s: %s\n", stems[i], err.message);
        if (!ok && built != NULL) differs(stems[i], built, m, cd, n);
        same += ok;
        free(built), free(cd);
    }
    CHECK_EQ_INT(tried, 38);
    CHECK_EQ_INT(same, tried);
}

/* the kit's banks win over a donor's: the Wagon's model (its faces in material blocks) and banks with the Viper as donor still give the Wagon */
static void test_own_banks_over_a_donor(void)
{
    size_t vn = 0, wn = 0, m = 0;
    uint8_t *viper = read_cd("DVIPER", &vn), *wagon = read_cd("WAGON", &wn);
    ExchangeError err = { { 0 } };
    uint8_t *built = viper != NULL && wagon != NULL ? kit_build(wagon, wn, viper, vn, 1, &m, &err) : NULL;
    CHECK(built != NULL && m == wn && memcmp(built, wagon, wn) == 0);
    if (built == NULL) printf("car_kit_banks: Wagon over the Viper: %s\n", err.message);
    free(built), free(viper), free(wagon);
}

/* a level's bank without its OBJ cannot keep a donor's far model: no low OBJ with banks given is refused, naming it */
static void test_banks_need_both_objs(void)
{
    size_t wn = 0, m = 0;
    uint8_t *wagon = read_cd("WAGON", &wn), *out = NULL;
    if (wagon == NULL) return;
    const uint8_t *child[CFM_CHILDREN];
    size_t len[CFM_CHILDREN];
    ExchangeError err = { { 0 } };
    FileSet hi;
    fileset_init(&hi);
    ObjModel high;
    memset(&high, 0, sizeof high);
    int ok = cfm_describe(wagon, wn, child, len, &err) && kit_level(child, len, 0, 0, &hi, &high, &err);
    CHECK(ok);
    if (ok) {
        CarImportIn in;
        memset(&in, 0, sizeof in);
        in.high = &high, in.high_tables = &hi, in.raw = 1, in.scale = 1.0;
        in.bank[0] = child[1], in.bank_len[0] = len[1], in.bank[1] = child[3], in.bank_len[1] = len[3];
        int32_t half[3];
        CHECK(!car_import(&in, &out, &m, half, &err));
        CHECK(strstr(err.message, "far") != NULL);
        if (strstr(err.message, "far") == NULL) printf("car_kit_banks: no low OBJ: '%s'\n", err.message);
    }
    free(out), obj_free(&high), fileset_free(&hi), free(wagon);
}

int main(void)
{
    size_t n = 0;
    uint8_t *probe = read_cd("WAGON", &n);
    if (probe == NULL) {
        fprintf(stderr, "car_kit_banks: no CD in original/, skipped\n");
        return TEST_REPORT();
    }
    free(probe);
    test_every_car_without_a_donor();
    test_own_banks_over_a_donor();
    test_banks_need_both_objs();
    return TEST_REPORT();
}
