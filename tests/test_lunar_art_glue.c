/* dep59 (Lunar Springs' entry, L4): Lunar Springs' bank made from the CD and kept (game/frontend/lunar_art.h, lunar_art_cached): a bank with
 * its picture and palette, written to the player's cache under its language's name; an entry with the same key is read back, not made
 * again; the German art makes its own; with no CD there is no bank (the caller shows Rusty's). Skipped without original/. */
#include "game/frontend/lunar_art.h"
#include "game/sys/art_cache.h"
#include "game/sys/resource.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char dir[512];

static long file_size(const char *name)
{
    char path[640];
    struct stat s;
    snprintf(path, sizeof path, "%s%s", dir, name);
    return stat(path, &s) == 0 ? (long)s.st_size : -1;
}

/* bgnd's place in a bank (its directory's), 0 for none */
static size_t bgnd_at(const uint8_t *bank, size_t size)
{
    for (size_t i = 16; bank != NULL && i + 8 <= size && i < 16 + 8 * 64; i += 8)
        if (memcmp(bank + i, "bgnd", 4) == 0) return (size_t)bank[i + 4] | (size_t)bank[i + 5] << 8 | (size_t)bank[i + 6] << 16;
    return 0;
}

/* the bank's picture's first row, all `v` */
static int bgnd_is(const uint8_t *bank, size_t size, uint8_t v)
{
    const size_t at = bgnd_at(bank, size);
    if (at == 0 || at + 16 + 640 > size) return 0;
    for (int i = 0; i < 640; i++)
        if (bank[at + 16 + i] != v) return 0;
    return 1;
}

static void test_made_and_kept(const char *cd)
{
    char name[32];
    size_t size = 0;
    resource_init(cd, 0);
    uint8_t *made = lunar_art_cached(LUNAR_ART_SELECT, &size, name, sizeof name);
    CHECK(made != NULL && strcmp(name, "lunar_sel_en.bin") == 0 && bgnd_at(made, size) > 0);
    size_t rn = 0, ln = 0;
    uint8_t *rust = resource_load_art("tracksel/rustsel.qfs", &rn), *lost = resource_load_art("tracksel/lostsel.qfs", &ln);
    CHECK(size == rn && file_size("lunar_sel_en.bin") > (long)rn);   /* written: its bytes and the cache's header */
    /* the same key, the picture's first row 0s: read back as it is, not made again */
    ArtCacheKey key;
    const uint8_t how = LUNAR_ART_SELECT;
    art_cache_key_init(&key, "lunar_art", LUNAR_ART_VERSION);
    art_cache_key_add(&key, &how, 1), art_cache_key_add(&key, rust, rn), art_cache_key_add(&key, lost, ln);
    const size_t at = bgnd_at(made, size);
    if (made != NULL && at > 0) {
        memset(made + at + 16, 0, 640);
        CHECK(art_cache_write("lunar_sel_en.bin", &key, made, size));
        size_t n = 0;
        uint8_t *again = lunar_art_cached(LUNAR_ART_SELECT, &n, name, sizeof name);
        CHECK(again != NULL && n == size && bgnd_is(again, n, 0));   /* the cache's, as it was written */
        free(again);
    }
    free(made), resource_free(rust), resource_free(lost);
    resource_init(cd, 1);   /* German: its own entry */
    uint8_t *de = lunar_art_cached(LUNAR_ART_BEST, &size, name, sizeof name);
    CHECK(de != NULL && strcmp(name, "lunar_bst_de.bin") == 0 && file_size("lunar_bst_de.bin") > 0 && !bgnd_is(de, size, 0));
    free(de);
}

static void test_no_cd(void)
{
    char name[32];
    size_t size = 1;
    resource_init("/nonexistent-cd", 0);
    CHECK(lunar_art_cached(LUNAR_ART_SELECT, &size, name, sizeof name) == NULL && size == 0);
}

int main(void)
{
    char cd[512], probe[600];
    snprintf(cd, sizeof cd, "%s/original", TNFS_SOURCE_DIR);
    snprintf(probe, sizeof probe, "%s/FRONTEND/ART/TRACKSEL/RUSTSEL.QFS", cd);
    snprintf(dir, sizeof dir, "%s/lunar_art_glue_XXXXXX", TNFS_BINARY_DIR);
    if (mkdtemp(dir) == NULL) return fprintf(stderr, "no work folder\n"), 1;
    strcat(dir, "/");
    art_cache_set_dir(dir);
    if (access(probe, R_OK) != 0) printf("skip the CD's (no original/)\n");
    else test_made_and_kept(cd);
    test_no_cd();
    return TEST_REPORT();
}
