/* The platform layer's cache of the .cfg files (a read does not go to the disk again within a few ms): a write through
 * the layer is read back at once, a removed file is gone at once, a file edited from outside is seen within the
 * interval, and other files are never cached. */
#include "platform/platform.h"
#include "test.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int reads_as(const char *name, const char *want)
{
    size_t n = 0;
    uint8_t *d = platform_read_user_file(name, &n);
    const int ok = d != NULL && n == strlen(want) && memcmp(d, want, n) == 0 && d[n] == 0;
    if (d != NULL) platform_free_file(d);
    return ok;
}

static void poke(const char *dir, const char *name, const char *text)   /* an edit from outside the layer */
{
    char path[1200];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *f = fopen(path, "wb");
    CHECK(f != NULL);
    if (f == NULL) return;
    fputs(text, f);
    fclose(f);
}

int main(void)
{
    char dir[] = "/tmp/tnfs_cfgcache_XXXXXX";
    CHECK(mkdtemp(dir) != NULL);
    setenv("TNFS_PREF_DIR", dir, 1);

    CHECK(platform_write_user_file("port.cfg", (const uint8_t *)"a=1\n", 4));
    CHECK(reads_as("port.cfg", "a=1\n"));
    CHECK(reads_as("port.cfg", "a=1\n"));                       /* from the copy */
    CHECK(platform_write_user_file("port.cfg", (const uint8_t *)"a=22\n", 5));
    CHECK(reads_as("port.cfg", "a=22\n"));                      /* write then read: the new value, at once */

    poke(dir, "port.cfg", "a=3\n");                             /* edited from outside: seen within the interval */
    SDL_Delay(120);
    CHECK(reads_as("port.cfg", "a=3\n"));

    CHECK(platform_remove_user_file("port.cfg"));
    size_t n = 0;
    CHECK(platform_read_user_file("port.cfg", &n) == NULL);     /* removed: gone at once */

    poke(dir, "other.dat", "x");                                /* not a .cfg: always read from the disk */
    CHECK(reads_as("other.dat", "x"));
    poke(dir, "other.dat", "y");
    CHECK(reads_as("other.dat", "y"));

    char path[1200];
    snprintf(path, sizeof path, "%s/other.dat", dir);
    unlink(path);
    return TEST_REPORT();
}
