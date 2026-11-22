/* The Track light window's comparison pictures at the App level (dep98 step 2 part D): Before (the files as they are on disk) beside the working copy's photo, and the four
 * times of day at one node. A fake game records which light files it was given at which time (FAKE_RECORD.<time>) and paints a grey by time, so the test sees what each
 * picture was made from: the working copy for the time being edited, the folder's files for the others, the working places file in all, the saved files for Before; a
 * change makes only the pictures it touches again; one game at a time. Temp copy of repo files, no CD data; with a game and the CD data given, the same loop runs against the real
 * game too (a tone tints the photo, the four times differ, Before after Save is the window's own picture). usage: test_editor_light_compare REPO [GAME CD] */
#include "app.h"
#include "test.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static char root[EXPORT_PATH], light[EXPORT_PATH], fake[EXPORT_PATH + 16], record[EXPORT_PATH + 16];

static char *read_file(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    const long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *text = malloc((size_t)n + 1);
    if (text != NULL && fread(text, 1, (size_t)n, f) != (size_t)n) { free(text), text = NULL; }
    fclose(f);
    if (text != NULL) text[n] = '\0', *size = (size_t)n;
    return text;
}

static void write_file(const char *path, const char *text, size_t size, int exec)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL) return;
    fwrite(text, 1, size, f);
    fclose(f);
    if (exec) chmod(path, 0755);
}

static void copy_repo_file(const char *repo, const char *dir, const char *name)
{
    char from[EXPORT_PATH + 64], to[EXPORT_PATH + 64];
    snprintf(from, sizeof from, "%s/data/%s/%s", repo, dir, name);
    snprintf(to, sizeof to, "%s/%s/%s", light, dir, name);
    size_t n = 0;
    char *text = read_file(from, &n);
    CHECK(text != NULL);
    if (text != NULL) write_file(to, text, n, 0);
    free(text);
}

/* each run writes what it was given as FAKE_RECORD.<time>: its light file, its places file and the node; and the picture's grey tells the time */
static const char FAKE[] =
    "#!/bin/sh\n"
    "shot= mods=\n"
    "while [ $# -gt 0 ]; do case $1 in --screenshot) shot=$2; shift;; --mods) mods=$2; shift;; esac; shift; done\n"
    "{ echo \"node=$TNFS_START_NODE\"; cat \"$mods\"/track_light/*.txt; echo ---; cat \"$mods\"/track/*.txt 2>/dev/null; } > \"$FAKE_RECORD.$TNFS_LIGHT_TIME\"\n"
    "echo run >> \"$FAKE_RECORD.runs\"\n"
    "case $TNFS_LIGHT_TIME in dawn) v=30;; day) v=200;; dusk) v=90;; night) v=5;; esac\n"
    "o=$(printf '%03o' $v)\n"
    "printf 'P6\\n640 480\\n255\\n' > \"$shot\"\n"
    "head -c 921600 /dev/zero | LC_ALL=C tr '\\000' \"\\\\$o\" >> \"$shot\"\n";

static const char *const WORD[4] = { "dawn", "day", "dusk", "night" };

static int track_index(const char *name)
{
    for (int i = 0; light_ui_track(i) != NULL; ++i)
        if (strcmp(light_ui_track(i), name) == 0) return i;
    return -1;
}

/* polls until nothing is due or running (the clock runs on, the wall clock bounds it: a loaded machine is slow); the number of polls it took, 0 when it never settled */
static int settle(App *a, double *now)
{
    const time_t deadline = time(NULL) + 120;
    for (int i = 1; time(NULL) < deadline; ++i) {
        *now += 0.01;
        const int busy = app_light_poll(a, *now);
        if (!busy && a->light.photo.state != LIGHT_PHOTO_RUNNING && a->light.edit_serial == a->light.photo_serial) return i;
        usleep(5000);
    }
    return 0;
}

static char *recorded(const char *time, size_t *size)
{
    char path[EXPORT_PATH + 32];
    snprintf(path, sizeof path, "%s.%s", record, time);
    return read_file(path, size);
}

static int runs(void)
{
    char path[EXPORT_PATH + 32];
    snprintf(path, sizeof path, "%s.runs", record);
    size_t n = 0;
    char *text = read_file(path, &n);
    int count = 0;
    for (size_t i = 0; text != NULL && i < n; ++i) count += text[i] == '\n';
    free(text);
    return count;
}

static void forget(void)
{
    char path[EXPORT_PATH + 32];
    for (int i = 0; i < 4; ++i) {
        snprintf(path, sizeof path, "%s.%s", record, WORD[i]);
        unlink(path);
    }
    snprintf(path, sizeof path, "%s.runs", record);
    unlink(path);
}

static int grey(const LightPhotoJob *j)
{
    return j->rgba != NULL && j->state == LIGHT_PHOTO_DONE ? j->rgba[0] : -1;
}

static void test_before(App *a, double *now)
{
    char why[LIGHT_WHY];
    forget();
    app_light_set_compare(a, LIGHT_COMPARE_BEFORE);
    CHECK(settle(a, now) > 0);
    LightCompare *c = &a->light.compare;
    CHECK(grey(&a->light.photo) == 5 && grey(&c->job[0]) == 5);                 // the working copy's photo and Before are both of night
    // an edit: the working copy's photo is made again, Before is not: it shows the file on disk
    const int before_runs = runs();
    const int before_serial = c->job[0].serial;
    CHECK(light_file_set_brightness(&a->light.session.file, -1, &(LightBrightness){ 120, 180, 70, LIGHT_SIDE_BOTH, 0 }, why, sizeof why));
    app_light_touch(a);
    CHECK(settle(a, now) > 0);
    CHECK_EQ_INT(runs(), before_runs + 1);                                       // one game run: the photo of the working copy
    CHECK_EQ_INT(c->job[0].serial, before_serial);
    size_t n = 0;
    char *rec = recorded("night", &n);
    CHECK(rec != NULL && strstr(rec, "brightness 120 180 70") != NULL);          // the last run was the working copy's
    free(rec);
    // Save: Before is now the saved file: it is made again, with the edit in it
    CHECK(app_light_save(a));
    CHECK(settle(a, now) > 0);
    CHECK(c->job[0].serial == before_serial + 1);
    rec = recorded("night", &n);
    CHECK(rec != NULL && strstr(rec, "brightness 120 180 70") != NULL);
    free(rec);
    // a node change makes both again
    app_light_set_compare(a, LIGHT_COMPARE_OFF);
    forget();
    app_light_set_compare(a, LIGHT_COMPARE_BEFORE);
    app_light_set_node(a, 90);
    CHECK(settle(a, now) > 0);
    CHECK_EQ_INT(runs(), 2);
    rec = recorded("night", &n);
    CHECK(rec != NULL && strstr(rec, "node=90\n") != NULL);
    free(rec);
}

static void test_four(App *a, double *now)
{
    char why[LIGHT_WHY];
    app_light_set_compare(a, LIGHT_COMPARE_OFF);
    forget();
    app_light_set_compare(a, LIGHT_COMPARE_FOUR);
    CHECK(settle(a, now) > 0);
    LightCompare *c = &a->light.compare;
    static const int GREY[4] = { 30, 200, 90, 5 };
    for (int t = 0; t < 4; ++t) CHECK_EQ_INT(grey(&c->job[t]), GREY[t]);        // dawn, day, dusk, night in that order
    CHECK_EQ_INT(runs(), 4);                                                      // the four times, one game at a time (the window's own photo was current)
    // the time being edited is the working copy; the others are the folder's files
    CHECK(light_file_set_brightness(&a->light.session.file, -1, &(LightBrightness){ 10, 20, 33, LIGHT_SIDE_BOTH, 0 }, why, sizeof why));
    CHECK(light_file_set_place(&a->light.session.places, -1, &(LightPlace){ "canyon", 5, 9 }, a->light.session.file.nodes, why, sizeof why));
    app_light_touch(a);
    forget();
    int serial[4];
    for (int t = 0; t < 4; ++t) serial[t] = c->job[t].serial;
    CHECK(settle(a, now) > 0);
    size_t n = 0;
    for (int t = 0; t < 4; ++t) {
        char *rec = recorded(WORD[t], &n);
        CHECK(rec != NULL);
        if (rec == NULL) continue;
        CHECK((strstr(rec, "brightness 10 20 33") != NULL) == (t == 3));         // only night has the working copy
        CHECK(strstr(rec, "canyon 5 9") != NULL);                                // the working places are in every picture
        free(rec);
    }
    for (int t = 0; t < 4; ++t) CHECK(c->job[t].serial == serial[t] + 1);        // the places file changed: all four again
    // an edit to the night file only: night again, the other three stay
    forget();
    for (int t = 0; t < 4; ++t) serial[t] = c->job[t].serial;
    CHECK(light_file_set_brightness(&a->light.session.file, -1, &(LightBrightness){ 30, 40, 44, LIGHT_SIDE_BOTH, 0 }, why, sizeof why));
    app_light_touch(a);
    CHECK(settle(a, now) > 0);
    CHECK_EQ_INT(c->job[0].serial, serial[0]);
    CHECK_EQ_INT(c->job[1].serial, serial[1]);
    CHECK_EQ_INT(c->job[2].serial, serial[2]);
    CHECK_EQ_INT(c->job[3].serial, serial[3] + 1);
    CHECK_EQ_INT(runs(), 2);                                                      // the window's photo and night
    // off: nothing more is made
    app_light_set_compare(a, LIGHT_COMPARE_OFF);
    forget();
    app_light_set_node(a, 50);
    CHECK(settle(a, now) > 0);
    CHECK_EQ_INT(runs(), 1);
}

/* the mean of one channel of a photo (0 R, 1 G, 2 B) */
static double channel(const LightPhotoJob *j, int c)
{
    double sum = 0;
    for (size_t i = 0; i < (size_t)j->w * j->h; ++i) sum += j->rgba[i * 4 + c];
    return sum / ((double)j->w * j->h);
}

/* the real game, CY1 at night, node 70: a tone line over the nodes of the photo tints it (red up), a car tone changes the car; the four times show day brighter than night;
 * after Save, Before is the same picture as the window's own */
static void test_real_game(const char *game, const char *cd)
{
    App *a = calloc(1, sizeof *a);
    CHECK(a != NULL);
    if (a == NULL) return;
    app_begin(a, game, "", NULL);
    app_set_light_dir(a, light);
    snprintf(a->export_src.cd, sizeof a->export_src.cd, "%s", cd);
    CHECK(app_open_light(a) && app_light_select(a, track_index("CY1"), 3));
    app_light_set_node(a, 70);
    double now = 1;
    CHECK(settle(a, &now) > 0 && a->light.photo.state == LIGHT_PHOTO_DONE && a->light.photo.w == 1280);
    if (a->light.photo.state != LIGHT_PHOTO_DONE) { app_free(a), free(a); return; }
    const double red_before = channel(&a->light.photo, 0), blue_before = channel(&a->light.photo, 2);
    char why[LIGHT_WHY];
    CHECK(light_file_set_tone(&a->light.session.file, -1, &(LightTone){ 40, 100, 0, "ff0000", 80 }, why, sizeof why));
    app_light_touch(a);
    app_light_set_compare(a, LIGHT_COMPARE_BEFORE);
    CHECK(settle(a, &now) > 0 && a->light.photo.state == LIGHT_PHOTO_DONE);
    const double red_after = channel(&a->light.photo, 0), blue_after = channel(&a->light.photo, 2);
    CHECK(red_after - blue_after > red_before - blue_before + 1.0);                       // a red tone: red up against blue
    LightCompare *c = &a->light.compare;
    CHECK(c->job[0].state == LIGHT_PHOTO_DONE && memcmp(c->job[0].rgba, a->light.photo.rgba, (size_t)a->light.photo.w * a->light.photo.h * 4) != 0);   // Before is not the changed photo
    CHECK(fabs(channel(&c->job[0], 0) - red_before) < 0.5);                                // ... it is the file as it was
    CHECK(app_light_save(a));
    CHECK(settle(a, &now) > 0);
    CHECK(memcmp(c->job[0].rgba, a->light.photo.rgba, (size_t)a->light.photo.w * a->light.photo.h * 4) == 0);   // saved: Before is the same picture as the window's own
    app_light_set_compare(a, LIGHT_COMPARE_FOUR);
    CHECK(settle(a, &now) > 0);
    for (int t = 0; t < 4; ++t) CHECK(c->job[t].state == LIGHT_PHOTO_DONE);
    CHECK(channel(&c->job[1], 1) > channel(&c->job[3], 1));                                // day is brighter than night
    app_free(a);
    free(a);
}

static void test_flow(void)
{
    App *a = calloc(1, sizeof *a);
    CHECK(a != NULL);
    if (a == NULL) return;
    app_begin(a, fake, "", NULL);
    app_set_light_dir(a, light);
    snprintf(a->export_src.cd, sizeof a->export_src.cd, "%s", root);
    setenv("FAKE_RECORD", record, 1);
    CHECK(app_open_light(a) && a->light.compare.mode == LIGHT_COMPARE_OFF);
    CHECK(app_light_select(a, track_index("CY1"), 3));
    app_light_set_node(a, 70);
    double now = 1;
    CHECK(settle(a, &now) > 0);
    test_before(a, &now);
    test_four(a, &now);
    app_free(a);
    free(a);
}

int main(int argc, char **argv)
{
    const char *repo = argc > 1 ? argv[1] : ".";
    char user[EXPORT_PATH], why[200];
    CHECK(car_race_temp(root, sizeof root, user, sizeof user, why, sizeof why));
    snprintf(light, sizeof light, "%s/light", root);
    mkdir(light, 0777);
    char sub[EXPORT_PATH + 16];
    snprintf(sub, sizeof sub, "%s/track_light", light);
    mkdir(sub, 0777);
    snprintf(sub, sizeof sub, "%s/track", light);
    mkdir(sub, 0777);
    for (const char *letter = "ABCN"; *letter != '\0'; ++letter) {
        char name[16];
        snprintf(name, sizeof name, "CY1%c.txt", *letter);
        copy_repo_file(repo, "track_light", name);
    }
    copy_repo_file(repo, "track", "CY1.txt");
    snprintf(fake, sizeof fake, "%s/fake-game", root);
    write_file(fake, FAKE, strlen(FAKE), 1);
    snprintf(record, sizeof record, "%s/record", root);
    test_flow();
    if (argc > 3) test_real_game(argv[2], argv[3]);
    car_race_temp_remove(root);
    return TEST_REPORT();
}
