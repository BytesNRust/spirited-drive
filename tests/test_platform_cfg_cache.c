/* The platform layer's cache of the .cfg files (a read does not go to the disk again within a few ms): a write through
 * the layer is read back at once, a removed file is gone at once, a file edited from outside is seen within the
 * interval, and other files are never cached. */
#include "game/sys/port_log.h"
#include "platform/platform.h"
#include "test.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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

static SDL_AtomicInt reader_stop;

static int SDLCALL read_all_the_time(void *unused)
{
    (void)unused;
    while (SDL_GetAtomicInt(&reader_stop) == 0) {
        size_t n = 0;
        uint8_t *d = platform_read_user_file("race.cfg", &n);
        if (d != NULL) platform_free_file(d);
    }
    return 0;
}

/* a read that began before a write must not leave the old bytes for the read after it */
static void test_read_during_write(void)
{
    SDL_Thread *reader = SDL_CreateThread(read_all_the_time, "reader", NULL);
    CHECK(reader != NULL);
    int stale = 0;
    for (int i = 0; i < 400; i++) {
        char want[16];
        const int len = snprintf(want, sizeof want, "v=%d\n", i);
        CHECK(platform_write_user_file("race.cfg", (const uint8_t *)want, (size_t)len));
        if (!reads_as("race.cfg", want)) stale++;
    }
    SDL_SetAtomicInt(&reader_stop, 1);
    SDL_WaitThread(reader, NULL);
    CHECK_EQ_INT(stale, 0);
    CHECK(platform_remove_user_file("race.cfg"));
}

/* more files than the old four slots: each reads as itself */
static void test_all_the_mode_files(void)
{
    static const char *const NAMES[] = { "port.cfg", "stock.cfg", "restored.cfg", "restomod.cfg", "controls.cfg", "extras.cfg" };
    enum { COUNT = sizeof NAMES / sizeof NAMES[0] };
    for (int i = 0; i < COUNT; i++) CHECK(platform_write_user_file(NAMES[i], (const uint8_t *)NAMES[i], strlen(NAMES[i])));
    for (int round = 0; round < 3; round++)
        for (int i = 0; i < COUNT; i++) CHECK(reads_as(NAMES[i], NAMES[i]));
    for (int i = 0; i < COUNT; i++) CHECK(platform_remove_user_file(NAMES[i]));
}

static int temp_files_left(void)
{
    int count = 0;
    char **names = platform_list_user_files("*.tmp", &count);
    if (names != NULL) platform_free_names(names);
    return count;
}

/* a write leaves no temporary file behind, also when it cannot end: here the target is a folder with something in it */
static void test_no_temp_left(const char *dir)
{
    CHECK(platform_write_user_file("kept.cfg", (const uint8_t *)"a=1\n", 4));
    CHECK_EQ_INT(temp_files_left(), 0);
    CHECK(platform_remove_user_file("kept.cfg"));
    char path[1200];
    snprintf(path, sizeof path, "%s/blocked.cfg", dir);
    CHECK(mkdir(path, 0700) == 0);
    poke(path, "inside", "x");
    port_log_watch("the target is a folder (0 retries)");   /* it stays in the way: retries only wasted 500 ms and more */
    CHECK(!platform_write_user_file("blocked.cfg", (const uint8_t *)"a=1\n", 4));
    CHECK(port_log_watch_hit());
    CHECK_EQ_INT(temp_files_left(), 0);
    snprintf(path, sizeof path, "%s/blocked.cfg/inside", dir);
    unlink(path);
    snprintf(path, sizeof path, "%s/blocked.cfg", dir);
    rmdir(path);
}

/* a file saved with a UTF-8 byte order mark (Notepad, VS Code, PowerShell) reads without it, cached or not; a file that is not text is left as it is */
static void test_bom(const char *dir)
{
    poke(dir, "bom.cfg", "\xEF\xBB\xBF" "a=1\n");
    CHECK(reads_as("bom.cfg", "a=1\n"));
    CHECK(reads_as("bom.cfg", "a=1\n"));   /* from the copy: without it as well */
    poke(dir, "bom.ini", "\xEF\xBB\xBF" "id=x\n");
    CHECK(reads_as("bom.ini", "id=x\n"));
    poke(dir, "bom.txt", "\xEF\xBB\xBF");
    CHECK(reads_as("bom.txt", ""));
    poke(dir, "bom.dat", "\xEF\xBB\xBF" "z");
    CHECK(reads_as("bom.dat", "\xEF\xBB\xBF" "z"));
    poke(dir, "short.ini", "\xEF\xBB");
    CHECK(reads_as("short.ini", "\xEF\xBB"));
    CHECK(platform_remove_user_file("bom.cfg") && platform_remove_user_file("bom.ini") && platform_remove_user_file("bom.txt"));
    CHECK(platform_remove_user_file("bom.dat") && platform_remove_user_file("short.ini"));
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
    test_all_the_mode_files();
    test_no_temp_left(dir);
    test_bom(dir);
    test_read_during_write();
    return TEST_REPORT();
}
