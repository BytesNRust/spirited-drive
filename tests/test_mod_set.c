/* dep59 n4 phase 2: the mods in effect (game/sys/mod_set.h), on a scratch player folder: Restomod's list in its order, `--mods` above it
 * (Restomod's only: Stock and Restored load no mods at all); a change of list or of mode is in effect at once (the settings' write), the generation
 * moving on only when the folders do; a listed mod with no folder left out. The overlay (platform_read_file) and the port's data (data_path) follow the same folders. */
#include "formats/mod_list.h"
#include "game/sys/data_path.h"
#include "game/sys/mod_set.h"
#include "game/sys/port_profile.h"
#include "game/sys/port_settings.h"
#include "platform/platform.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char base[256], cd[300], pref[300];

static void make_dirs(const char *path)
{
    char buf[1024];
    snprintf(buf, sizeof buf, "%s", path);
    for (char *p = buf + 1; *p; ++p)
        if (*p == '/') { *p = '\0'; mkdir(buf, 0777); *p = '/'; }
    mkdir(buf, 0777);
}

static void write_file(const char *dir, const char *name, const char *text)
{
    char path[1100];
    make_dirs(dir);
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *f = fopen(path, "wb");
    CHECK(f != NULL);
    if (f != NULL) fputs(text, f), fclose(f);
}

/* the game's NOTE.TXT as the overlay gives it, compared with `text` */
static int note_is(const char *text)
{
    char asked[400];
    size_t size = 0;
    snprintf(asked, sizeof asked, "%s/SIMDATA/MISC/NOTE.TXT", cd);
    uint8_t *d = platform_read_file(asked, &size);
    const int same = d != NULL && size == strlen(text) && memcmp(d, text, size) == 0;
    platform_free_file(d);
    return same;
}

static void mod_note(const char *id, const char *text)
{
    char dir[400];
    snprintf(dir, sizeof dir, "%s/mods/%s/SIMDATA/MISC", pref, id);
    write_file(dir, "NOTE.TXT", text);
}

static void save_list(const char *text)
{
    ModList l;
    mod_list_parse(text, &l);
    CHECK(mod_set_save_list(&l));
}

static void test_the_list_in_its_order(void)
{
    mod_note("a", "from a");
    mod_note("b", "from b");
    CHECK(port_profile_select(PROFILE_RESTOMOD));
    CHECK(mod_set_start(cd, NULL));
    CHECK(note_is("from the cd"));   /* no list yet */
    const uint32_t g0 = mod_set_generation();
    save_list("b,a");
    CHECK(note_is("from b"));
    const uint32_t g1 = mod_set_generation();
    CHECK(g1 != g0);
    save_list("a,b");
    CHECK(note_is("from a"));
    const char *dirs[4];
    CHECK_EQ_INT(mod_set_dirs(dirs, 4), 2);   /* the folders in effect, the first wins */
    CHECK(strstr(dirs[0], "/mods/a") != NULL && strstr(dirs[1], "/mods/b") != NULL);
    CHECK_EQ_INT(mod_set_dirs(dirs, 1), 1);   /* no more than asked for */
    CHECK(strstr(dirs[0], "/mods/a") != NULL);
    CHECK(mod_set_generation() != g1);
    const uint32_t g2 = mod_set_generation();
    PortSettings s = port_settings_load();   /* another row saved: the same folders, the same generation */
    s.fov_adjust = 2;
    CHECK(port_settings_save(&s));
    CHECK_EQ_INT((int)mod_set_generation(), (int)g2);
    char dir[400], found[DATA_PATH_CANDIDATES][DATA_PATH_MAX];   /* the port's data follows the same folders */
    snprintf(dir, sizeof dir, "%s/mods/b", pref);
    write_file(dir, "car_envmap.png", "png");
    data_path_candidates("car_envmap.png", found);
    CHECK(strstr(found[0], "/mods/b/car_envmap.png") != NULL);
    save_list("ghost,b");   /* a listed mod with no folder is left out */
    CHECK(note_is("from b"));
}

static void test_stock_and_restored_read_no_list(void)
{
    save_list("a");
    CHECK(note_is("from a"));
    const uint32_t g = mod_set_generation();
    CHECK(port_profile_select(PROFILE_STOCK));
    CHECK(note_is("from the cd"));
    CHECK(mod_set_generation() != g);
    CHECK(port_profile_select(PROFILE_RESTORED));
    CHECK(note_is("from the cd"));
    CHECK(port_profile_select(PROFILE_RESTOMOD));
    CHECK(note_is("from a"));   /* the list was kept: back in Restomod, in effect again */
}

static void test_the_command_line_folder_comes_first(void)
{
    char cli[400];
    snprintf(cli, sizeof cli, "%s/cli/SIMDATA/MISC", base);
    write_file(cli, "NOTE.TXT", "from the command line");
    snprintf(cli, sizeof cli, "%s/cli", base);
    char missing[400];
    snprintf(missing, sizeof missing, "%s/nope", base);
    CHECK(!mod_set_start(cd, missing));   /* a --mods folder that is not there stops the start, in Restomod */
    CHECK(port_profile_select(PROFILE_STOCK));
    CHECK(mod_set_start(cd, missing));   /* elsewhere it is ignored */
    CHECK(port_profile_select(PROFILE_RESTOMOD));
    CHECK(mod_set_start(cd, cli));
    CHECK(note_is("from the command line"));
    CHECK(port_profile_select(PROFILE_STOCK));
    CHECK(note_is("from the cd"));   /* Stock and Restored load no mods, --mods neither */
    CHECK(port_profile_select(PROFILE_RESTORED));
    CHECK(note_is("from the cd"));
    CHECK(port_profile_select(PROFILE_RESTOMOD));
    CHECK(note_is("from the command line"));
    CHECK(mod_set_start(cd, NULL));
    CHECK(note_is("from a"));
}

int main(void)
{
    snprintf(base, sizeof base, "%s/tnfs_mod_set_test_%d", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp", (int)getpid());
    snprintf(cd, sizeof cd, "%s/cd", base);
    snprintf(pref, sizeof pref, "%s/pref", base);
    char cd_misc[400];
    snprintf(cd_misc, sizeof cd_misc, "%s/SIMDATA/MISC", cd);
    write_file(cd_misc, "NOTE.TXT", "from the cd");
    make_dirs(pref);
    setenv("TNFS_PREF_DIR", pref, 1);
    test_the_list_in_its_order();
    test_stock_and_restored_read_no_list();
    test_the_command_line_folder_comes_first();
    mod_set_stop();
    char cmd[400];
    snprintf(cmd, sizeof cmd, "rm -rf '%s'", base);
    if (system(cmd) != 0) fprintf(stderr, "could not remove %s\n", base);
    return TEST_REPORT();
}
