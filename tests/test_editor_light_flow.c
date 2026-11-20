/* The Track light window's flow at the App level (dep98 step 2 part C, the owner's rule: a step without a flow test is a gap): open the mode on a light folder, pick CY1
 * at night, add a brightness line, see the unsaved mark, the photo of the WORKING copy (a fake game records what it was given), Revert, Save into the folder, and the
 * refusals (a folder that is no light folder, a track or time change over unsaved edits, TR5 with no file, no game, a node out of the road). The folder is a temp copy of
 * repo light files, so it needs no CD data. usage: test_editor_light_flow REPO */
#include "app.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char root[EXPORT_PATH], light[EXPORT_PATH], fake[EXPORT_PATH + 16], record[EXPORT_PATH + 16], disk_file[EXPORT_PATH + 32];

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

static void copy_repo_file(const char *repo, const char *name)
{
    char from[EXPORT_PATH + 64], to[EXPORT_PATH + 64];
    snprintf(from, sizeof from, "%s/data/track_light/%s", repo, name);
    snprintf(to, sizeof to, "%s/track_light/%s", light, name);
    size_t n = 0;
    char *text = read_file(from, &n);
    CHECK(text != NULL);
    if (text != NULL) write_file(to, text, n, 0);
    free(text);
}

static const char FAKE[] =
    "#!/bin/sh\n"
    "shot= mods=\n"
    "while [ $# -gt 0 ]; do case $1 in --screenshot) shot=$2; shift;; --mods) mods=$2; shift;; esac; shift; done\n"
    "cat \"$mods\"/track_light/*.txt > \"$FAKE_RECORD\"\n"
    "case $TNFS_LIGHT_TIME in dawn) v=30;; day) v=200;; dusk) v=90;; night) v=5;; esac\n"
    "o=$(printf '%03o' $v)\n"
    "printf 'P6\\n1280 960\\n255\\n' > \"$shot\"\n"
    "head -c 3686400 /dev/zero | LC_ALL=C tr '\\000' \"\\\\$o\" >> \"$shot\"\n";

static int track_index(const char *name)
{
    for (int i = 0; light_ui_track(i) != NULL; ++i)
        if (strcmp(light_ui_track(i), name) == 0) return i;
    return -1;
}

/* polls until the photo job has ended (the clock runs on): 1 when it did */
static int run_photo(App *a, double *now)
{
    for (int i = 0; i < 2000; ++i) {
        *now += 0.01;
        if (!app_light_poll(a, *now)) return 1;
        usleep(10000);
    }
    return 0;
}

static void test_flow(void)
{
    App *a = calloc(1, sizeof *a);
    CHECK(a != NULL);
    if (a == NULL) return;
    app_begin(a, fake, "", NULL);
    app_set_light_dir(a, light);
    snprintf(a->export_src.cd, sizeof a->export_src.cd, "%s", root);   // the game's files: the fake does not read them, the job only wants a folder
    setenv("FAKE_RECORD", record, 1);
    CHECK(app_open_light(a) && a->kind == APP_KIND_LIGHT && a->light.open);
    CHECK(strcmp(light_ui_track(a->light.track), "AL1") == 0 && a->light.time == 1 && !app_modified(a));   // the first track at day
    char title[256];
    app_title(a, title, sizeof title);
    CHECK(strstr(title, "Track light") != NULL && strstr(title, " *") == NULL);
    // CY1 at night: its lamps are listed; the photo is of node 70
    const int cy1 = track_index("CY1"), night = 3;
    CHECK(cy1 >= 0 && app_light_select(a, cy1, night) && a->light.session.has_file && a->light.session.letter == 'N');
    app_light_set_node(a, 70);
    CHECK(a->light.node == 70);
    CHECK(light_file_count(&a->light.session.file, LIGHT_KIND_LAMP) > 0);
    double now = 1;
    CHECK(run_photo(a, &now));                                         // the photo that opening and selecting made due starts once the settle time passed, and ends
    CHECK(a->light.photo.state == LIGHT_PHOTO_DONE && a->light.photo.w == 1280 && a->light.photo.rgba[0] == 5);   // night: the fake's grey 5
    const int first_serial = a->light.photo.serial;
    // an edit: the unsaved mark; the photo is due but waits for the settle time; it is of the working copy, not the file on disk
    size_t before_size = 0;
    snprintf(disk_file, sizeof disk_file, "%s/track_light/CY1N.txt", light);
    char *before = read_file(disk_file, &before_size);
    CHECK(before != NULL);
    char why[LIGHT_WHY];
    const LightBrightness b = { 120, 180, 70 };
    const int brightness = light_file_count(&a->light.session.file, LIGHT_KIND_BRIGHTNESS);
    CHECK(light_file_set_brightness(&a->light.session.file, -1, &b, why, sizeof why));
    app_light_touch(a);
    CHECK(app_modified(a));
    app_title(a, title, sizeof title);
    CHECK(strstr(title, " *") != NULL);
    now += 0.1;
    CHECK(app_light_poll(a, now) == 0);                                // 0.1 s after the edit: not yet
    CHECK(a->light.photo.serial == first_serial);
    now += LIGHT_UI_SETTLE;
    CHECK(app_light_poll(a, now) == 1);                                // the settle time passed: the photo starts
    CHECK(run_photo(a, &now) && a->light.photo.state == LIGHT_PHOTO_DONE && a->light.photo.serial == first_serial + 1);
    size_t rec_size = 0;
    char *rec = read_file(record, &rec_size);
    CHECK(rec != NULL && strstr(rec, "brightness 120 180 70") != NULL);   // the game was given the working copy
    free(rec);
    size_t disk_size = 0;
    char *disk = read_file(disk_file, &disk_size);
    CHECK(disk != NULL && disk_size == before_size && memcmp(disk, before, disk_size) == 0);   // and the file on disk is as it was
    free(disk);
    // a track or time change over unsaved edits is refused, with the reason; so is leaving the mode
    CHECK(!app_light_select(a, track_index("CY2"), night) && strstr(a->light.error, "unsaved") != NULL && a->light.track == cy1);
    app_ask(a, APP_ACT_QUIT, "");
    CHECK(a->confirm == APP_ACT_QUIT);
    app_answer(a, APP_ANSWER_CANCEL);
    // Revert: the file as loaded, no mark
    app_light_revert(a);
    CHECK(!app_modified(a) && light_file_count(&a->light.session.file, LIGHT_KIND_BRIGHTNESS) == brightness);
    // edit again, Save: the file on disk has the line after every line the owner wrote, no other file changed
    CHECK(light_file_set_brightness(&a->light.session.file, -1, &b, why, sizeof why));
    app_light_touch(a);
    CHECK(app_light_save(a) && !app_modified(a));
    disk = read_file(disk_file, &disk_size);
    CHECK(disk != NULL && disk_size > before_size && memcmp(disk, before, before_size) == 0 && strcmp(disk + before_size, "brightness 120 180 70\n") == 0);
    free(disk);
    free(before);
    // a node out of the road is clamped
    app_light_set_node(a, 999999);
    CHECK(a->light.node == (int)a->light.session.file.nodes - 1);
    app_light_set_node(a, -4);
    CHECK(a->light.node == 0);
    // TR5 has no file: it opens with the reason, nothing to Save or photograph
    CHECK(app_light_select(a, track_index("TR5"), 1) && !a->light.session.has_file);
    CHECK(!app_light_save(a) && a->light.error[0] != '\0');
    CHECK(!app_light_photo(a) && a->light.error[0] != '\0');
    // no game: the photo is refused with the reason, nothing runs
    CHECK(app_light_select(a, cy1, night) && a->game[0] != '\0');
    a->game[0] = '\0';
    CHECK(!app_light_photo(a) && a->light.error[0] != '\0' && !app_light_busy(a));
    app_free(a);
    free(a);
}

static void test_not_a_light_folder(void)
{
    App *a = calloc(1, sizeof *a);
    CHECK(a != NULL);
    if (a == NULL) return;
    app_begin(a, fake, "", NULL);
    char bare[EXPORT_PATH + 16];
    snprintf(bare, sizeof bare, "%s/bare", root);
    mkdir(bare, 0777);
    app_set_light_dir(a, bare);
    CHECK(!app_open_light(a) && a->kind == APP_KIND_NONE && strstr(a->open_error, "track_light") != NULL);   // the window says why, the start screen stays
    app_set_light_dir(a, "");
    CHECK(!app_open_light(a) && a->kind == APP_KIND_NONE && a->open_error[0] != '\0');
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
    copy_repo_file(repo, "AL1B.txt");
    copy_repo_file(repo, "CY1N.txt");
    copy_repo_file(repo, "CY2N.txt");
    snprintf(fake, sizeof fake, "%s/fake-game", root);
    write_file(fake, FAKE, strlen(FAKE), 1);
    snprintf(record, sizeof record, "%s/record", root);
    test_flow();
    test_not_a_light_folder();
    car_race_temp_remove(root);
    return TEST_REPORT();
}
