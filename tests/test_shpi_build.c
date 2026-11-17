/* dep59 tools B1: a texture bank built from its entries (formats/shpi_build.h). Each bank of the CD's 39 CFMs comes back byte for
 * byte; a new bank of three entries reads back with fsh; what the builder refuses it refuses naming the entry. The tests that read
 * the CD are skipped without it. */
#include "formats/chunk.h"
#include "formats/fsh.h"
#include "formats/shpi_build.h"
#include "test.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

enum { MAX_ENTRIES = 64 };

static uint8_t *read_all(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    const long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *d = n > 0 ? malloc((size_t)n) : NULL;
    if (d != NULL && fread(d, 1, (size_t)n, f) != (size_t)n) free(d), d = NULL;
    fclose(f);
    *size = (size_t)n;
    return d;
}

static uint8_t *read_cd(const char *file, size_t *size)
{
    char path[512];
    snprintf(path, sizeof path, "%s/original/SIMDATA/CARFAMS/%s", TNFS_SOURCE_DIR, file);
    return read_all(path, size);
}

static int round_trip(const char *file, int child, const Chunk *c)
{
    ShpiDescEntry e[MAX_ENTRIES];
    size_t n = 0, len = 0;
    uint8_t *built = NULL;
    ExchangeError err = { { 0 } };
    if (!shpi_describe(c->data, c->size, e, MAX_ENTRIES, &n, &err)) {
        fprintf(stderr, "shpi_build: %s child %d: describe: %s\n", file, child, err.message);
        return 0;
    }
    int ok = shpi_build(e, n, &built, &len, &err);
    if (!ok) fprintf(stderr, "shpi_build: %s child %d: build: %s\n", file, child, err.message);
    if (ok && (len != c->size || memcmp(built, c->data, len) != 0)) {
        size_t at = 0;
        while (at < len && at < c->size && built[at] == c->data[at]) at++;
        fprintf(stderr, "shpi_build: %s child %d: %zu bytes for %zu, first difference at 0x%zx\n", file, child, len, c->size, at);
        ok = 0;
    }
    free(built);
    return ok;
}

static int test_round_trip_cd(void)
{
    char dir[512];
    snprintf(dir, sizeof dir, "%s/original/SIMDATA/CARFAMS", TNFS_SOURCE_DIR);
    DIR *h = opendir(dir);
    if (h == NULL) return 0;
    int banks = 0, same = 0;
    struct dirent *de;
    while ((de = readdir(h)) != NULL) {
        const size_t len = strlen(de->d_name);
        if (len < 5 || strcasecmp(de->d_name + len - 4, ".CFM") != 0) continue;
        size_t n = 0;
        uint8_t *cfm = read_cd(de->d_name, &n);
        Chunk root, c;
        for (int child = CFM_CHILD_BITMAPS_HIGH; cfm != NULL && child <= CFM_CHILD_BITMAPS_LOW; child += 2) {
            CHECK(chunk_open(&root, cfm, n) == CHUNK_OK && chunk_child(&root, (uint32_t)child, &c) == CHUNK_OK);
            banks++;
            same += round_trip(de->d_name, child, &c);
        }
        free(cfm);
    }
    closedir(h);
    CHECK_EQ_INT(banks, 78);
    CHECK_EQ_INT(same, banks);
    return 1;
}

static ShpiDescEntry entry(const char *name, uint8_t type, uint16_t w, uint16_t h, const uint8_t *data, uint32_t dir_index)
{
    ShpiDescEntry e;
    memset(&e, 0, sizeof e);
    memcpy(e.name, name, 4);
    e.type = type, e.width = w, e.height = h, e.data = data, e.block_size = SHPI_BLOCK_AUTO, e.dir_index = dir_index;
    if (type != 0x7b) e.misc[0] = 256;
    return e;
}

/* !PAL from the Viper, topv 96 x 40 and side 7 x 3 */
static void test_new_bank(const uint8_t *viper_pal)
{
    static uint8_t topv[96 * 40], side[7 * 3];
    for (size_t i = 0; i < sizeof topv; i++) topv[i] = (uint8_t)(i % 250);
    for (size_t i = 0; i < sizeof side; i++) side[i] = (uint8_t)(0xf0 + i % 16);   /* 0xFA..0xFF too */
    const ShpiDescEntry e[3] = { entry("!PAL", 0x24, 256, 1, viper_pal, 0), entry("topv", 0x7b, 96, 40, topv, 1), entry("side", 0x7b, 7, 3, side, 2) };
    uint8_t *built = NULL;
    size_t len = 0;
    ExchangeError err = { { 0 } };
    CHECK(shpi_build(e, 3, &built, &len, &err));
    Fsh f;
    CHECK_EQ_INT(built != NULL ? fsh_open(&f, built, len) : FSH_ERR_HEADER, FSH_OK);
    if (built != NULL) {
        CHECK_EQ_INT((int)f.count, 3);
        const struct { const char *name; uint16_t w, h; const uint8_t *px; size_t n; } want[] = {
            { "!PAL", 256, 1, viper_pal, 768 }, { "topv", 96, 40, topv, sizeof topv }, { "side", 7, 3, side, sizeof side } };
        for (int i = 0; i < 3; i++) {
            uint32_t index;
            FshEntry fe;
            CHECK(fsh_find(&f, want[i].name, &index) == FSH_OK && fsh_entry(&f, index, &fe) == FSH_OK);
            CHECK_EQ_INT(fe.width, want[i].w);
            CHECK_EQ_INT(fe.height, want[i].h);
            CHECK(fe.payload_size >= want[i].n && memcmp(fe.payload, want[i].px, want[i].n) == 0);
        }
    }
    free(built);
}

static void refused(const ShpiDescEntry *e, size_t n, const char *word, const char *what)
{
    uint8_t *built = NULL;
    size_t len = 0;
    ExchangeError err = { { 0 } };
    const int ok = shpi_build(e, n, &built, &len, &err);
    if (ok || strstr(err.message, word) == NULL) fprintf(stderr, "shpi_build: %s: %s (%s)\n", what, ok ? "built" : err.message, word);
    CHECK_EQ_INT(ok, 0);
    CHECK(strstr(err.message, word) != NULL);
    CHECK(built == NULL);
    free(built);
}

static void test_refusals(const uint8_t *pal)
{
    static uint8_t px[16 * 16];
    ShpiDescEntry e[3] = { entry("!PAL", 0x24, 256, 1, pal, 0), entry("topv", 0x7b, 16, 16, px, 1), entry("side", 0x7b, 16, 16, px, 2) };
    e[1].type = 0x7c;
    refused(e, 3, "topv", "a bitmap of type 0x7c");
    e[1].type = 0x7b, e[1].width = 0;
    refused(e, 3, "topv", "a side of 0");
    e[1].width = 32768;
    refused(e, 3, "topv", "a side of 32768");
    e[1].width = 16;
    memcpy(e[2].name, "topv", 4);
    refused(e, 3, "topv", "two entries of one name");
    memcpy(e[2].name, "side", 4);
    const ShpiDescEntry no_pal[2] = { entry("topv", 0x7b, 16, 16, px, 0), entry("side", 0x7b, 16, 16, px, 1) };
    refused(no_pal, 2, "!PAL", "no !PAL");
    /* the menus' banks name it !pal (RUSTSEL.QFS, MOUSE.FSH): a palette too (dep59 L4 rebuilds one) */
    ShpiDescEntry menu[2] = { entry("!pal", 0x22, 256, 1, pal, 0), entry("bgnd", 0x7b, 16, 16, px, 1) };
    uint8_t *built = NULL;
    size_t built_len = 0;
    ExchangeError err;
    CHECK(shpi_build(menu, 2, &built, &built_len, &err) && built != NULL && memcmp(built + 16, "!pal", 4) == 0);
    free(built);
    memcpy(menu[0].name, "!pak", 4);
    refused(menu, 2, "!PAL", "!pak is no palette's name");
    e[2].dir_index = 1;
    refused(e, 3, "side", "two entries at one place of the directory");
    e[2].dir_index = 2, e[2].block_size = 1u << 24;
    refused(e, 3, "side", "a block size past 24 bits");
}

int main(void)
{
    size_t n = 0;
    uint8_t *viper = read_cd("DVIPER.CFM", &n);
    static uint8_t grey[768];
    for (int i = 0; i < 768; i++) grey[i] = (uint8_t)(i / 3 / 4);
    test_refusals(grey);
    if (viper == NULL) {
        fprintf(stderr, "shpi_build: no CD in original/, the CD's tests are skipped\n");
        test_new_bank(grey);
        return TEST_REPORT();
    }
    test_round_trip_cd();
    Chunk root, c;
    Fsh f;
    FshEntry pal;
    uint32_t index;
    CHECK(chunk_open(&root, viper, n) == CHUNK_OK && chunk_child(&root, CFM_CHILD_BITMAPS_HIGH, &c) == CHUNK_OK);
    CHECK(fsh_open(&f, c.data, c.size) == FSH_OK && fsh_find(&f, "!PAL", &index) == FSH_OK && fsh_entry(&f, index, &pal) == FSH_OK);
    test_new_bank(pal.payload);
    free(viper);
    return TEST_REPORT();
}
