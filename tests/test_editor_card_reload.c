/* The card and the car's files changed on disk while the car is open (W4, editor/app_card_watch.c): mod.ini with no unsaved edits is read again; with
 * edits it is kept, the state says «Changed on disk» (Reload or Keep mine) and Save asks first; the car file and the textures of modelo/c1 changing
 * reach the lists, the Model and the Preview (car_serial, mask.generation). A kit made of plain files (no game data). */
#include "app.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char root[400];

static void path_of(char *out, size_t cap, const char *rel)
{
    snprintf(out, cap, "%s/%s", root, rel);
}

static int put(const char *rel, const char *text)
{
    char p[700];
    path_of(p, sizeof p, rel);
    FILE *f = fopen(p, "wb");
    if (f == NULL) return 0;
    const size_t n = strlen(text);
    const int ok = fwrite(text, 1, n, f) == n;
    return fclose(f) == 0 && ok;
}

static int file_has(const char *rel, const char *text)
{
    char p[700], bytes[600] = "";
    path_of(p, sizeof p, rel);
    FILE *f = fopen(p, "rb");
    if (f == NULL) return 0;
    const size_t n = fread(bytes, 1, sizeof bytes - 1, f);
    fclose(f);
    bytes[n] = '\0';
    return strstr(bytes, text) != NULL;
}

static void dir_of(const char *rel)
{
    char p[700];
    path_of(p, sizeof p, rel);
    mkdir(p, 0755);
}

static const char *CARD(int year)
{
    static char text[300];
    static int pad;   /* every card is longer than the last: the size alone tells two writes apart on a clock as coarse as a kernel tick */
    const int n = snprintf(text, sizeof text, "id=boxcar\nname=Box\nversion=1\ntype=car\nstem=C84\nsound_from=911\nyear=%d\n#", year);
    for (int i = 0; i < ++pad && n + i + 2 < (int)sizeof text; i++) text[n + i] = '#', text[n + i + 1] = '\n', text[n + i + 2] = '\0';
    return text;
}

static App *open_kit(void)
{
    const char *dirs[] = { "coche", "coche/SIMDATA", "coche/SIMDATA/CARFAMS", "modelo", "modelo/c1" };
    for (size_t i = 0; i < sizeof dirs / sizeof dirs[0]; i++) dir_of(dirs[i]);
    if (!put("coche/mod.ini", CARD(1984)) || !put("coche/SIMDATA/CARFAMS/C84.CFM", "cfm one") || !put("modelo/c1/0010_rsid.png", "x")) return NULL;
    App *a = calloc(1, sizeof *a);
    if (a == NULL) return NULL;
    app_begin(a, "./tnfs", "", NULL);
    if (!app_open_car(a, root)) return free(a), NULL;
    return a;
}

static const char *year(const App *a)
{
    return a->edit.now.value[CE_YEAR];
}

static void test_the_card(void)
{
    App *a = open_kit();
    CHECK(a != NULL);
    if (a == NULL) return;
    CHECK_EQ_INT(app_card_watch(a, 10.0), 0);   /* the first look only records the files */
    CHECK(strcmp(year(a), "1984") == 0);

    /* no unsaved edits: read again at the next look a second on */
    CHECK(put("coche/mod.ini", CARD(1985)));
    CHECK_EQ_INT(app_card_watch(a, 10.4), 0);   /* not yet a second */
    CHECK(strcmp(year(a), "1984") == 0);
    CHECK_EQ_INT(app_card_watch(a, 11.1), 1);
    CHECK(strcmp(year(a), "1985") == 0 && !a->cdisk.card_changed && !car_edit_modified(&a->edit));
    CHECK(strstr(a->status, "mod.ini changed on disk") != NULL);
    CHECK_EQ_INT(app_card_watch(a, 12.2), 0);   /* once */

    /* our own Save is not a change on disk */
    CHECK(car_edit_set(&a->edit, CE_YEAR, "1990"));
    CHECK(app_save(a));
    CHECK_EQ_INT(app_card_watch(a, 13.3), 0);
    CHECK(strcmp(year(a), "1990") == 0 && car_edit_can_undo(&a->edit));

    /* unsaved edits: kept, the state says so, Save does not write over the file */
    CHECK(car_edit_set(&a->edit, CE_YEAR, "1991"));
    CHECK(put("coche/mod.ini", CARD(1970)));
    CHECK_EQ_INT(app_card_watch(a, 14.4), 1);
    CHECK(a->cdisk.card_changed && car_edit_modified(&a->edit));
    CHECK(strcmp(year(a), "1991") == 0);
    CHECK(!app_save(a));
    CHECK(strstr(a->status, "mod.ini changed on disk") != NULL);
    CHECK(file_has("coche/mod.ini", "year=1970"));   /* the file is the other program's still */
    CHECK_EQ_INT(app_card_watch(a, 15.5), 0);               /* said once */
    app_card_keep_mine(a);
    CHECK(!a->cdisk.card_changed);
    CHECK(app_save(a));
    CHECK(file_has("coche/mod.ini", "year=1991"));   /* mine, written over */

    /* Reload: the file's card, the edits gone */
    CHECK(car_edit_set(&a->edit, CE_YEAR, "1992"));
    CHECK(put("coche/mod.ini", CARD(1960)));
    CHECK_EQ_INT(app_card_watch(a, 16.6), 1);
    CHECK(a->cdisk.card_changed);
    CHECK(app_card_reload(a));
    CHECK(!a->cdisk.card_changed && !car_edit_modified(&a->edit) && strcmp(year(a), "1960") == 0);

    /* changed between two looks, then Save at once: asked all the same */
    CHECK(car_edit_set(&a->edit, CE_YEAR, "1993"));
    CHECK(put("coche/mod.ini", CARD(1950)));
    CHECK(!app_save(a));
    CHECK(a->cdisk.card_changed);
    CHECK(app_card_reload(a));
    CHECK(strcmp(year(a), "1950") == 0);

    /* a card another program is still writing (not a car's card yet): the open one stays, read at a later look */
    CHECK(put("coche/mod.ini", "id=bo"));
    CHECK_EQ_INT(app_card_watch(a, 17.7), 0);
    CHECK(strcmp(year(a), "1950") == 0 && !a->cdisk.card_changed);
    CHECK(put("coche/mod.ini", CARD(1940)));
    CHECK_EQ_INT(app_card_watch(a, 18.8), 1);
    CHECK(strcmp(year(a), "1940") == 0);
    app_free(a);
    free(a);
}

static void test_the_car_files(void)
{
    App *a = open_kit();
    CHECK(a != NULL);
    if (a == NULL) return;
    CHECK_EQ_INT(app_card_watch(a, 1.0), 0);
    const unsigned serial = a->cdisk.serial, generation = a->mask.generation;
    CHECK_EQ_INT(a->car.texture_count, 1);

    /* a texture added: the list, and the Model and the Preview are told */
    CHECK(put("modelo/c1/0020_bonn.png", "y"));
    CHECK_EQ_INT(app_card_watch(a, 2.1), 1);
    CHECK_EQ_INT(a->car.texture_count, 2);
    CHECK(a->cdisk.serial == serial && strstr(a->status, "textures changed on disk") != NULL);   /* the list only: the car file is as it was */
    CHECK_EQ_INT(app_card_watch(a, 3.2), 0);

    /* a texture rewritten, the car file rewritten (Rehacer, a new bank) */
    unsigned s = a->cdisk.serial;
    CHECK(put("modelo/c1/0020_bonn.png", "yy"));
    CHECK_EQ_INT(app_card_watch(a, 4.3), 1);
    CHECK(a->cdisk.serial == s);
    CHECK(put("coche/SIMDATA/CARFAMS/C84.CFM", "cfm two, longer"));
    CHECK_EQ_INT(app_card_watch(a, 5.4), 1);
    CHECK(a->cdisk.serial != s && a->mask.generation != generation && strstr(a->status, "car file changed on disk") != NULL);

    /* a texture gone */
    char p[700];
    path_of(p, sizeof p, "modelo/c1/0020_bonn.png");
    CHECK(unlink(p) == 0);
    CHECK_EQ_INT(app_card_watch(a, 6.5), 1);
    CHECK_EQ_INT(a->car.texture_count, 1);

    /* a mask with unsaved edits is never dropped: the files are read when it is saved or closed */
    CHECK_EQ_INT(a->mask.sel, -1);
    app_free(a);
    free(a);
}

int main(void)
{
    const char *tmp = getenv("TMPDIR");
    snprintf(root, sizeof root, "%s/tnfs-card-reload-XXXXXX", tmp != NULL && tmp[0] != '\0' ? tmp : "/tmp");
    if (mkdtemp(root) == NULL) {
        fprintf(stderr, "no scratch folder\n");
        return 1;
    }
    test_the_card();
    test_the_car_files();
    char cmd[600];
    const char *files[] = { "coche/mod.ini", "coche/mod.ini.tmp", "coche/SIMDATA/CARFAMS/C84.CFM", "modelo/c1/0010_rsid.png", "modelo/c1/0020_bonn.png" };
    for (size_t i = 0; i < sizeof files / sizeof files[0]; i++) {
        path_of(cmd, sizeof cmd, files[i]);
        unlink(cmd);
    }
    const char *dirs[] = { "modelo/c1", "modelo", "coche/SIMDATA/CARFAMS", "coche/SIMDATA", "coche" };
    for (size_t i = 0; i < sizeof dirs / sizeof dirs[0]; i++) {
        path_of(cmd, sizeof cmd, dirs[i]);
        rmdir(cmd);
    }
    rmdir(root);
    return TEST_REPORT();
}
