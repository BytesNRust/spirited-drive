/* The Track light window's flow at the App level (dep98 step 2 part C, the owner's rule: a step without a flow test is a gap): open the mode on a light folder, pick CY1
 * at night, add a brightness line, see the unsaved mark, the photo of the WORKING copy (a fake game records what it was given), Revert, Save into the folder, and the
 * refusals (a folder that is no light folder, a track or time change over unsaved edits, TR5 with no file, no game, a node out of the road). The folder is a temp copy of
 * repo light files, so it needs no CD data. With a game and the CD data given, the same loop runs against the real game too.
 * usage: test_editor_light_flow [REPO GAME CD] (REPO is not read any more: the light files and the TRIs are made here) */
#include "app.h"
#include "fake_tri.h"
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

/* the game's files: a TRI for each track the test opens (the Studio reads the node count from it) */
static void write_cd(const char *cd)
{
    char dir[EXPORT_PATH + 32], file[EXPORT_PATH + 64];
    snprintf(dir, sizeof dir, "%s/SIMDATA", cd);
    mkdir(dir, 0777);
    snprintf(dir, sizeof dir, "%s/SIMDATA/MISC", cd);
    mkdir(dir, 0777);
    static const char *const TRACKS[] = { "AL1", "CY1", "CY2", "TR5" };
    for (size_t i = 0; i < sizeof TRACKS / sizeof *TRACKS; ++i) {
        snprintf(file, sizeof file, "%s/%s.TRI", dir, TRACKS[i]);
        CHECK(fake_tri_write(file, 30));
    }
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

static const char LIVE[] =
    "#!/bin/sh\n"
    "mods=\n"
    "while [ $# -gt 0 ]; do case $1 in --mods) mods=$2; shift;; esac; shift; done\n"
    "echo \"edit=$TNFS_LIGHT_EDIT time=$TNFS_LIGHT_TIME start=$TNFS_START_NODE weather=$TNFS_WEATHER\" > \"$FAKE_RECORD\"\n"
    "sleep 30\n";

static int track_index(const char *name)
{
    for (int i = 0; light_ui_track(i) != NULL; ++i)
        if (strcmp(light_ui_track(i), name) == 0) return i;
    return -1;
}

/* polls until the photo job has ended (the clock runs on): 1 when it did */
static int run_photo_for(App *a, double *now, int polls)
{
    for (int i = 0; i < polls; ++i) {
        *now += 0.01;
        if (!app_light_poll(a, *now)) return 1;
        usleep(10000);
    }
    return 0;
}

static int run_photo(App *a, double *now)
{
    return run_photo_for(a, now, 2000);
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
    CHECK(cy1 >= 0 && app_light_select(a, cy1, night) && a->light.session.has_file && a->light.time == night);
    app_light_set_node(a, 70);
    CHECK(a->light.node == 70);
    CHECK(light_track_count(&a->light.session.file, LT_TIME_N, LT_LAMP) > 0);
    double now = 1;
    CHECK(run_photo(a, &now));                                         // the photo that opening and selecting made due starts once the settle time passed, and ends
    CHECK(a->light.photo.state == LIGHT_PHOTO_DONE && a->light.photo.w == 1280 && a->light.photo.rgba[0] == 5);   // night: the fake's grey 5
    const int first_serial = a->light.photo.serial;
    // an edit: the unsaved mark; the photo is due but waits for the settle time; it is of the working copy, not the file on disk
    size_t before_size = 0;
    snprintf(disk_file, sizeof disk_file, "%s/track_light/CY1.txt", light);
    char *before = read_file(disk_file, &before_size);
    CHECK(before != NULL);
    char why[LT_WHY];
    const LightTrackBrightness b = { 100, 110, 70, LT_ALL_POINTS };
    const int brightness = light_track_count(&a->light.session.file, LT_TIME_N, LT_BRIGHTNESS);
    CHECK(light_track_set_brightness(&a->light.session.file, LT_TIME_N, -1, &b, why, sizeof why));
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
    CHECK(rec != NULL && strstr(rec, "brightness 100 110 70") != NULL);   // the game was given the working copy
    free(rec);
    size_t disk_size = 0;
    char *disk = read_file(disk_file, &disk_size);
    CHECK(disk != NULL && disk_size == before_size && memcmp(disk, before, disk_size) == 0);   // and the file on disk is as it was
    free(disk);
    // another time of the same track needs no read (the file holds them all); another track over unsaved edits is refused, with the reason; so is leaving the mode
    CHECK(app_light_select(a, cy1, 1) && a->light.time == 1 && app_modified(a) && app_light_select(a, cy1, night));
    CHECK(!app_light_select(a, track_index("CY2"), night) && strstr(a->light.error, "unsaved") != NULL && a->light.track == cy1);
    app_ask(a, APP_ACT_QUIT, "");
    CHECK(a->confirm == APP_ACT_QUIT);
    app_answer(a, APP_ANSWER_CANCEL);
    // Revert: the file as loaded, no mark
    app_light_revert(a);
    CHECK(!app_modified(a) && light_track_count(&a->light.session.file, LT_TIME_N, LT_BRIGHTNESS) == brightness);
    // edit again, Save: the file on disk has the line after every line the owner wrote, no other file changed
    CHECK(light_track_set_brightness(&a->light.session.file, LT_TIME_N, -1, &b, why, sizeof why));
    app_light_touch(a);
    CHECK(app_light_save(a) && !app_modified(a));
    disk = read_file(disk_file, &disk_size);
    CHECK(disk != NULL && disk_size > before_size && memcmp(disk, before, before_size) == 0 && strstr(disk + before_size, "brightness 100 110 70") != NULL);   // the new line after every line the owner wrote
    free(disk);
    // the same line given a strip (as the panel's bar does), saved: the word is in the file, and a window opened on it again reads it
    const LightTrackBrightness safe = { 100, 110, 70, LT_STRIP_RIGHT };
    CHECK(light_track_set_brightness(&a->light.session.file, LT_TIME_N, brightness, &safe, why, sizeof why));
    app_light_touch(a);
    CHECK(app_light_save(a) && !app_modified(a));
    disk = read_file(disk_file, &disk_size);
    CHECK(disk != NULL && disk_size > before_size && memcmp(disk, before, before_size) == 0 && strstr(disk + before_size, "brightness 100 110 70 right") != NULL);
    free(disk);
    CHECK(app_light_select(a, track_index("CY2"), night) && app_light_select(a, cy1, night));   // away and back: read again from the disk
    LightTrackBrightness again;
    CHECK(light_track_get_brightness(&a->light.session.file, LT_TIME_N, brightness, &again) && again.from == 100 && again.to == 110 && again.percent == 70 && again.mask == LT_STRIP_RIGHT);
    free(before);
    // a node out of the road is clamped
    app_light_set_node(a, 999999);
    CHECK(a->light.node == (int)a->light.session.nodes - 1 && a->light.session.nodes == 120);
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


/* the mean of a photo's R, G and B over its whole frame */
static double mean_of(const uint8_t *rgba, int w, int h)
{
    double sum = 0;
    for (size_t i = 0; i < (size_t)w * h; ++i) sum += rgba[i * 4] + rgba[i * 4 + 1] + rgba[i * 4 + 2];
    return sum / ((double)w * h * 3);
}

/* the real game: CY1 at night, node 70: a photo is taken; a brightness line makes the next one darker; after Save, a fresh window on the saved folder gives the same picture */
static void test_real_game(const char *game, const char *cd)
{
    char user[EXPORT_PATH], why[200], dir[EXPORT_PATH], sub[EXPORT_PATH + 16];
    char tmp[EXPORT_PATH];
    CHECK(car_race_temp(tmp, sizeof tmp, user, sizeof user, why, sizeof why));
    snprintf(dir, sizeof dir, "%s/light", tmp);
    mkdir(dir, 0777);
    snprintf(sub, sizeof sub, "%s/track_light", dir);
    mkdir(sub, 0777);
    snprintf(light, sizeof light, "%s", dir);
    fake_light_write(light, "CY1");
    App *a = calloc(1, sizeof *a);
    CHECK(a != NULL);
    if (a == NULL) return;
    app_begin(a, game, "", NULL);
    app_set_light_dir(a, dir);
    snprintf(a->export_src.cd, sizeof a->export_src.cd, "%s", cd);
    CHECK(app_open_light(a) && app_light_select(a, track_index("CY1"), 3));
    app_light_set_node(a, 70);
    double now = 1;
    CHECK(run_photo_for(a, &now, 6000) && a->light.photo.state == LIGHT_PHOTO_DONE && a->light.photo.w == 1280);
    if (a->light.photo.state != LIGHT_PHOTO_DONE) { app_free(a), free(a); return; }
    const double before = mean_of(a->light.photo.rgba, a->light.photo.w, a->light.photo.h);
    uint8_t *first = malloc((size_t)a->light.photo.w * a->light.photo.h * 4);
    memcpy(first, a->light.photo.rgba, (size_t)a->light.photo.w * a->light.photo.h * 4);
    const LightTrackBrightness dark = { 0, a->light.session.nodes - 1, 30, LT_ALL_POINTS };
    CHECK(light_track_set_brightness(&a->light.session.file, LT_TIME_N, -1, &dark, why, sizeof why));
    app_light_touch(a);
    now += LIGHT_UI_SETTLE + 0.1;
    CHECK(app_light_poll(a, now));
    CHECK(run_photo_for(a, &now, 6000) && a->light.photo.state == LIGHT_PHOTO_DONE);
    const double after = mean_of(a->light.photo.rgba, a->light.photo.w, a->light.photo.h);
    CHECK(before > 1.0 && after < before);                    // the road is darker with 30 percent of its light
    uint8_t *second = malloc((size_t)a->light.photo.w * a->light.photo.h * 4);
    memcpy(second, a->light.photo.rgba, (size_t)a->light.photo.w * a->light.photo.h * 4);
    CHECK(app_light_save(a));
    app_free(a);
    // a fresh window on the saved folder: the same picture as the working copy gave
    app_begin(a, game, "", NULL);
    app_set_light_dir(a, dir);
    snprintf(a->export_src.cd, sizeof a->export_src.cd, "%s", cd);
    CHECK(app_open_light(a) && app_light_select(a, track_index("CY1"), 3));
    app_light_set_node(a, 70);
    now = 1;
    CHECK(run_photo_for(a, &now, 6000) && a->light.photo.state == LIGHT_PHOTO_DONE);
    CHECK(memcmp(a->light.photo.rgba, second, (size_t)a->light.photo.w * a->light.photo.h * 4) == 0);
    CHECK(memcmp(first, second, (size_t)a->light.photo.w * a->light.photo.h * 4) != 0);
    free(first);
    free(second);
    app_free(a);
    free(a);
    car_race_temp_remove(tmp);
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
    app_free(a);
    free(a);
}

/* the packaged app has no source tree: the window opens saying no folder is set, Choose... (app_light_set_folder) refuses what is no light folder and what would drop
 * unsaved edits, then takes the folder and keeps it; the game's own data folder is the default when it has track_light/ */
static void test_choose_folder(void)
{
    char pack[EXPORT_PATH + 64], dir[EXPORT_PATH + 64], found[EXPORT_PATH + 128];
    snprintf(pack, sizeof pack, "%s/pack", root);
    mkdir(pack, 0777);
    snprintf(dir, sizeof dir, "%s/Contents", pack);
    mkdir(dir, 0777);
    snprintf(dir, sizeof dir, "%s/Contents/MacOS", pack);
    mkdir(dir, 0777);
    snprintf(dir, sizeof dir, "%s/Contents/Resources", pack);
    mkdir(dir, 0777);
    snprintf(dir, sizeof dir, "%s/Contents/Resources/data", pack);
    mkdir(dir, 0777);
    char program[EXPORT_PATH + 64];
    snprintf(program, sizeof program, "%s/Contents/MacOS/tnfs", pack);
    CHECK(!app_light_default_dir(program, found, sizeof found));   // a data folder with no track_light/ is not a light folder
    snprintf(dir, sizeof dir, "%s/Contents/Resources/data/track_light", pack);
    mkdir(dir, 0777);
    CHECK(app_light_default_dir(program, found, sizeof found) && strstr(found, "Contents/MacOS/../Resources/data") != NULL);
    CHECK(!app_light_default_dir("", found, sizeof found) && !app_light_default_dir(NULL, found, sizeof found) && !app_light_default_dir("tnfs", found, sizeof found));

    // inside an app bundle: no default, and Save refuses
    CHECK(app_light_in_bundle("/Applications/X.app/Contents/Resources/data") && app_light_in_bundle("/a/X.app") && !app_light_in_bundle("/a/X.apple/data") && !app_light_in_bundle("/a/app/data") && !app_light_in_bundle(NULL));
    char bundle[EXPORT_PATH + 64], saved[EXPORT_PATH + 64];
    snprintf(bundle, sizeof bundle, "%s/Game.app", root);
    mkdir(bundle, 0777);
    snprintf(dir, sizeof dir, "%s/Contents", bundle);
    mkdir(dir, 0777);
    snprintf(dir, sizeof dir, "%s/Contents/MacOS", bundle);
    mkdir(dir, 0777);
    snprintf(dir, sizeof dir, "%s/Contents/Resources", bundle);
    mkdir(dir, 0777);
    snprintf(dir, sizeof dir, "%s/Contents/Resources/data", bundle);
    mkdir(dir, 0777);
    snprintf(dir, sizeof dir, "%s/Contents/Resources/data/track_light", bundle);
    mkdir(dir, 0777);
    snprintf(program, sizeof program, "%s/Contents/MacOS/tnfs", bundle);
    CHECK(!app_light_default_dir(program, found, sizeof found));   // the data has track_light/, but it is in a bundle
    char keep[EXPORT_PATH + 64];
    snprintf(keep, sizeof keep, "%s", light);
    snprintf(light, sizeof light, "%s/Contents/Resources/data", bundle);
    fake_light_write(light, "AL1");
    snprintf(light, sizeof light, "%s", keep);
    App *b = calloc(1, sizeof *b);
    CHECK(b != NULL);
    if (b == NULL) return;
    app_begin(b, program, "", NULL);
    snprintf(b->export_src.cd, sizeof b->export_src.cd, "%s", root);
    CHECK(app_open_light(b) && !b->light.open && strstr(b->light.error, "Choose a light folder") != NULL && b->light_dir[0] == '\0');
    snprintf(dir, sizeof dir, "%s/Contents/Resources/data", bundle);
    CHECK(app_light_set_folder(b, dir) && b->light.open);   // looking at it is allowed
    snprintf(saved, sizeof saved, "%s/Contents/Resources/data/track_light/AL1.txt", bundle);
    size_t n0 = 0;
    char *before = read_file(saved, &n0);
    CHECK(before != NULL);
    const LightTrackBrightness e = { 0, b->light.session.nodes - 1, 40, LT_ALL_POINTS };
    char reason[200];
    CHECK(light_track_set_brightness(&b->light.session.file, LT_TIME_B, -1, &e, reason, sizeof reason));
    app_light_touch(b);
    CHECK(!app_light_save(b) && strstr(b->light.error, "app bundle") != NULL);
    size_t n1 = 0;
    char *after = read_file(saved, &n1);
    CHECK(after != NULL && n1 == n0 && before != NULL && memcmp(before, after, n0) == 0);   // the file was not touched
    free(before);
    free(after);
    app_free(b);
    free(b);

    App *a = calloc(1, sizeof *a);
    CHECK(a != NULL);
    if (a == NULL) return;
    app_begin(a, fake, "", NULL);   // a game with no data folder beside it, and no folder set
    snprintf(a->export_src.cd, sizeof a->export_src.cd, "%s", root);
    CHECK(a->light_dir[0] == '\0');
    CHECK(app_open_light(a) && a->kind == APP_KIND_LIGHT && !a->light.open && strstr(a->light.error, "Choose a light folder") != NULL && a->light_dir[0] == '\0');
    CHECK(!app_modified(a) && !app_light_select(a, 1, 1) && !app_light_photo(a) && !app_light_busy(a));
    app_light_revert(a);   // nothing open: harmless
    CHECK(!app_light_set_folder(a, root) && !a->light.open && a->light.error[0] != '\0' && a->light_dir[0] == '\0');   // root has no track_light/
    CHECK(app_light_set_folder(a, light) && a->light.open && strcmp(a->light_dir, light) == 0 && a->light.error[0] == '\0');
    CHECK(strcmp(light_ui_track(a->light.track), "AL1") == 0 && a->light.time == 1);
    // unsaved edits: another folder is refused until they are saved or reverted
    const LightTrackBrightness br = { 0, a->light.session.nodes - 1, 40, LT_ALL_POINTS };
    char why[200];
    CHECK(light_track_set_brightness(&a->light.session.file, LT_TIME_B, -1, &br, why, sizeof why));
    app_light_touch(a);
    CHECK(app_modified(a) && !app_light_set_folder(a, light) && strstr(a->light.error, "unsaved") != NULL);
    app_light_revert(a);
    CHECK(!app_modified(a) && app_light_set_folder(a, light));
    CHECK(!app_light_set_folder(a, root) && a->light.open && strcmp(a->light_dir, light) == 0);   // a refused folder leaves the open one
    app_free(a);
    free(a);
}

/* the test drive (light_drive.h) through the App: it starts on the open track, time, node and weather with the light editor on; an edit reaches its copy at once, without Save and
 * without a photo (one game at a time); another node or weather restarts it once the change settles; Stop frees the game and the photo is made again */
static void test_drive_flow(void)
{
    App *a = calloc(1, sizeof *a);
    CHECK(a != NULL);
    if (a == NULL) return;
    char live[EXPORT_PATH + 16];
    snprintf(live, sizeof live, "%s/live-game", root);
    write_file(live, LIVE, strlen(LIVE), 1);
    app_begin(a, live, "", NULL);
    app_set_light_dir(a, light);
    snprintf(a->export_src.cd, sizeof a->export_src.cd, "%s", root);
    setenv("FAKE_RECORD", record, 1);
    CHECK(app_open_light(a));
    const int cy1 = track_index("CY1");
    CHECK(app_light_select(a, cy1, 3));
    app_light_set_node(a, 70);
    CHECK(!app_light_drive_running(a));
    unlink(record);   // the photo flow's record is still there
    CHECK(app_light_drive(a) && app_light_drive_running(a));
    char scratch_file[EXPORT_PATH + 120];
    for (int i = 0; i < 300 && access(record, F_OK) != 0; ++i) usleep(10000);
    usleep(100000);
    size_t n = 0;
    char *rec = read_file(record, &n);
    CHECK(rec != NULL && strstr(rec, "edit=1") != NULL && strstr(rec, "time=night") != NULL && strstr(rec, "start=70") != NULL && strstr(rec, "weather=clear") != NULL);
    free(rec);
    double now = 1;
    // an edit reaches the drive's copy at once, with no Save and no photo
    char why[LT_WHY];
    const LightTrackBrightness b = { 100, 110, 70, LT_ALL_POINTS };
    CHECK(light_track_set_brightness(&a->light.session.file, LT_TIME_N, -1, &b, why, sizeof why));
    app_light_touch(a);
    now += 0.05;
    app_light_poll(a, now);
    snprintf(scratch_file, sizeof scratch_file, "%s/track_light/CY1.txt", a->light.drive.mods);
    char *copy = read_file(scratch_file, &n);
    CHECK(copy != NULL && strstr(copy, "brightness 100 110 70") != NULL);
    free(copy);
    CHECK(a->light.photo.state != LIGHT_PHOTO_RUNNING);
    now += LIGHT_UI_SETTLE + 1;
    CHECK(app_light_poll(a, now) == 0 && a->light.photo.state != LIGHT_PHOTO_RUNNING && app_light_drive_running(a));   // the photo waits for the game to be free
    // another node: the drive keeps going until the change settles, then it restarts there
    app_light_set_node(a, 90);
    now += 0.05;
    CHECK(app_light_poll(a, now) == 0 && a->light.drive.node == 70);
    now += LIGHT_UI_SETTLE;
    CHECK(app_light_poll(a, now) == 0 && app_light_drive_running(a) && a->light.drive.node == 90);
    // the weather restarts it too
    unlink(record);
    app_light_set_weather(a, 2);
    now += LIGHT_UI_SETTLE;
    CHECK(app_light_poll(a, now) == 0 && a->light.drive.weather == 2);
    for (int i = 0; i < 300 && access(record, F_OK) != 0; ++i) usleep(10000);
    usleep(100000);
    rec = read_file(record, &n);
    CHECK(rec != NULL && strstr(rec, "weather=rain") != NULL && strstr(rec, "start=90") != NULL);
    free(rec);
    // Stop: the game is gone, nothing is reported as a failure, and the photo is made again
    app_light_drive_stop(a);
    CHECK(!app_light_drive_running(a) && a->light.drive.why[0] == '\0' && a->light.drive.temp[0] == '\0');
    now += 0.1;
    CHECK(app_light_poll(a, now) == 1 && a->light.photo.state == LIGHT_PHOTO_RUNNING);
    light_photo_stop(&a->light.photo);
    // nothing to drive with: refused in words, nothing runs
    a->game[0] = '\0';
    CHECK(!app_light_drive(a) && a->light.error[0] != '\0' && !app_light_drive_running(a));
    app_free(a);
    free(a);
}

int main(int argc, char **argv)
{
    char user[EXPORT_PATH], why[200];
    CHECK(car_race_temp(root, sizeof root, user, sizeof user, why, sizeof why));
    snprintf(light, sizeof light, "%s/light", root);
    mkdir(light, 0777);
    char sub[EXPORT_PATH + 16];
    snprintf(sub, sizeof sub, "%s/track_light", light);
    mkdir(sub, 0777);
    fake_light_write(light, "AL1");
    fake_light_write(light, "CY1");
    fake_light_write(light, "CY2");
    write_cd(root);
    snprintf(fake, sizeof fake, "%s/fake-game", root);
    write_file(fake, FAKE, strlen(FAKE), 1);
    snprintf(record, sizeof record, "%s/record", root);
    test_flow();
    test_drive_flow();
    test_not_a_light_folder();
    test_choose_folder();
    car_race_temp_remove(root);
    if (argc > 3) test_real_game(argv[2], argv[3]);
    return TEST_REPORT();
}
