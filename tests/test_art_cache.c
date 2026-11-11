/* The art cache: an entry written is read back with its key, a different key or input misses, a damaged or short file misses, no folder or a
 * folder that cannot be written leaves every read a miss and nothing breaks. */
#include "game/sys/art_cache.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char dir[512];

static void key_for(ArtCacheKey *k, uint32_t version, const char *input)
{
    art_cache_key_init(k, "test", version);
    art_cache_key_add(k, input, strlen(input));
}

static void a_written_entry_reads_back(void)
{
    ArtCacheKey k;
    key_for(&k, 1, "the CD's bytes");
    uint8_t data[1000], back[1000];
    for (int i = 0; i < 1000; i++) data[i] = (uint8_t)(i * 7);
    CHECK(art_cache_write("a.bin", &k, data, sizeof data));
    memset(back, 0, sizeof back);
    CHECK(art_cache_read("a.bin", &k, back, sizeof back));
    CHECK(memcmp(back, data, sizeof data) == 0);
}

static void another_key_misses(void)
{
    ArtCacheKey k, version, input, missing;
    key_for(&k, 1, "the CD's bytes");
    key_for(&version, 2, "the CD's bytes");
    key_for(&input, 1, "another CD");
    art_cache_key_init(&missing, "test", 1);
    art_cache_key_add(&missing, NULL, 0);
    uint8_t back[1000];
    CHECK(!art_cache_read("a.bin", &version, back, sizeof back));
    CHECK(!art_cache_read("a.bin", &input, back, sizeof back));
    CHECK(!art_cache_read("a.bin", &missing, back, sizeof back));
    CHECK(!art_cache_read("a.bin", &k, back, 999));   /* another size */
    CHECK(!art_cache_read("none.bin", &k, back, sizeof back));
}

static void a_damaged_or_short_file_misses(void)
{
    ArtCacheKey k;
    key_for(&k, 1, "the CD's bytes");
    char path[600];
    snprintf(path, sizeof path, "%sa.bin", dir);
    FILE *f = fopen(path, "r+b");
    CHECK(f != NULL);
    if (f == NULL) return;
    fseek(f, 500, SEEK_SET);
    fputc(0x5A, f);   /* a byte of the picture flipped */
    fclose(f);
    uint8_t back[1000];
    CHECK(!art_cache_read("a.bin", &k, back, sizeof back));
    uint8_t data[1000] = { 0 };
    CHECK(art_cache_write("a.bin", &k, data, sizeof data));   /* made again */
    CHECK(art_cache_read("a.bin", &k, back, sizeof back));
    CHECK(truncate(path, 100) == 0);
    CHECK(!art_cache_read("a.bin", &k, back, sizeof back));
    f = fopen(path, "wb");
    if (f != NULL) fputs("not ours", f), fclose(f);
    CHECK(!art_cache_read("a.bin", &k, back, sizeof back));
}

static void no_folder_no_cache(void)
{
    ArtCacheKey k;
    key_for(&k, 1, "x");
    uint8_t data[16] = { 1 }, back[16];
    art_cache_set_dir(NULL);
    CHECK(!art_cache_write("a.bin", &k, data, sizeof data));
    CHECK(!art_cache_read("a.bin", &k, back, sizeof back));
    art_cache_set_dir("/nonexistent-root-folder/cannot/be/made/");
    CHECK(!art_cache_write("a.bin", &k, data, sizeof data));
    CHECK(!art_cache_read("a.bin", &k, back, sizeof back));
}

int main(void)
{
    const char *tmp = getenv("TMPDIR");
    char base[400];
    snprintf(base, sizeof base, "%s/art_cache_XXXXXX", tmp != NULL && tmp[0] != '\0' ? tmp : "/tmp");
    if (mkdtemp(base) == NULL) return 1;
    snprintf(dir, sizeof dir, "%s/cache/", base);   /* not made yet: the first write makes it */
    art_cache_set_dir(dir);
    a_written_entry_reads_back();
    another_key_misses();
    a_damaged_or_short_file_misses();
    no_folder_no_cache();
    char path[600];
    snprintf(path, sizeof path, "%sa.bin", dir), remove(path);
    rmdir(dir), rmdir(base);
    return TEST_REPORT();
}
