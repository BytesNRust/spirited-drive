/* The Track light window's session (editor/light_session.h, dep98 step 2 part C): open a track's light file in a light folder, edit it, Save, Revert, and the rules of Save
 * (only the one file, only into a light folder, never under original/). The flow runs on a copy of a repo light file in a temp folder, so it needs no CD data.
 * usage: test_editor_light_session REPO */
#include "car_race.h"
#include "light_session.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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

static void write_file(const char *path, const char *text, size_t size)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL) return;
    fwrite(text, 1, size, f);
    fclose(f);
}

/* a light folder in a temp tree: <root>/light/track_light/CY1N.txt (a copy of the repo's), <root>/light/track/CY1.txt (a places file) and a sibling file outside it */
typedef struct {
    char root[EXPORT_PATH], light[EXPORT_PATH], file[EXPORT_PATH], original[EXPORT_PATH];
    char *source;
    size_t source_size;
} Fixture;

static int make_fixture(Fixture *x, const char *repo)
{
    char user[EXPORT_PATH], why[200], path[EXPORT_PATH + 64];
    if (!car_race_temp(x->root, sizeof x->root, user, sizeof user, why, sizeof why)) return 0;
    snprintf(x->light, sizeof x->light, "%s/light", x->root);
    mkdir(x->light, 0777);
    snprintf(path, sizeof path, "%s/track_light", x->light);
    mkdir(path, 0777);
    snprintf(path, sizeof path, "%s/track", x->light);
    mkdir(path, 0777);
    snprintf(path, sizeof path, "%s/data/track_light/CY1N.txt", repo);
    x->source = read_file(path, &x->source_size);
    if (x->source == NULL) return 0;
    snprintf(x->file, sizeof x->file, "%s/track_light/CY1N.txt", x->light);
    write_file(x->file, x->source, x->source_size);
    snprintf(path, sizeof path, "%s/track/CY1.txt", x->light);
    write_file(path, "# tnfs track places 1\ntrack CY1\n", 33);
    snprintf(path, sizeof path, "%s/sibling.txt", x->root);
    write_file(path, "keep", 4);
    snprintf(x->original, sizeof x->original, "%s/original", x->root);
    return 1;
}

static void free_fixture(Fixture *x)
{
    free(x->source);
    car_race_temp_remove(x->root);
}

static void test_open_edit_save_reload(const char *repo)
{
    Fixture x;
    CHECK(make_fixture(&x, repo));
    LightSession s;
    char why[LIGHT_WHY + 200];
    CHECK(light_session_open(&s, x.light, "CY1", 'N', why, sizeof why) && s.has_file);
    CHECK(strcmp(s.track, "CY1") == 0 && s.letter == 'N' && s.file.nodes == 1920);
    CHECK(!light_session_dirty(&s));                                 // opened: the working copy is the file
    char path[EXPORT_PATH];
    light_session_path(&s, path, sizeof path);
    CHECK(strcmp(path, x.file) == 0);
    const int lamps = light_file_count(&s.file, LIGHT_KIND_LAMP), brightness = light_file_count(&s.file, LIGHT_KIND_BRIGHTNESS);
    CHECK(lamps > 0);                                                // the night file's lamps are listed
    // an edit: dirty; the photo's text carries it, the file on disk does not
    const LightBrightness b = { 120, 180, 70 };
    CHECK(light_file_set_brightness(&s.file, -1, &b, why, sizeof why));
    CHECK(light_session_dirty(&s));
    size_t text_size = 0;
    char *text = light_session_text(&s, &text_size);
    CHECK(text != NULL && strstr(text, "brightness 120 180 70") != NULL);
    size_t disk_size = 0;
    char *disk = read_file(x.file, &disk_size);
    CHECK(disk != NULL && disk_size == x.source_size && memcmp(disk, x.source, disk_size) == 0);   // nothing is written before Save
    free(disk);
    free(text);
    // Revert: back to the file as loaded
    light_session_revert(&s);
    CHECK(!light_session_dirty(&s) && light_file_count(&s.file, LIGHT_KIND_BRIGHTNESS) == brightness);
    // edit again and Save: the file has the line, and every other byte is the owner's
    CHECK(light_file_set_brightness(&s.file, -1, &b, why, sizeof why));
    CHECK(light_session_save(&s, why, sizeof why));
    CHECK(!light_session_dirty(&s));
    disk = read_file(x.file, &disk_size);
    CHECK(disk != NULL && disk_size > x.source_size && memcmp(disk, x.source, x.source_size) == 0 && strcmp(disk + x.source_size, "brightness 120 180 70\n") == 0);   // appended after the last line
    free(disk);
    // reload from disk: the same entries
    LightSession again;
    CHECK(light_session_open(&again, x.light, "CY1", 'N', why, sizeof why) && !light_session_dirty(&again));
    CHECK(light_file_count(&again.file, LIGHT_KIND_BRIGHTNESS) == brightness + 1 && light_file_count(&again.file, LIGHT_KIND_LAMP) == lamps);
    LightBrightness got;
    CHECK(light_file_get_brightness(&again.file, brightness, &got) && got.from == 120 && got.to == 180 && got.percent == 70);
    light_session_close(&again);
    // Save wrote the one file: the sibling and the places file are as they were, and no temporary file is left
    char name[EXPORT_PATH + 64];
    snprintf(name, sizeof name, "%s/sibling.txt", x.root);
    size_t n = 0;
    char *sib = read_file(name, &n);
    CHECK(sib != NULL && n == 4 && memcmp(sib, "keep", 4) == 0);
    free(sib);
    snprintf(name, sizeof name, "%s/track/CY1.txt", x.light);
    sib = read_file(name, &n);
    CHECK(sib != NULL && n == 33);
    free(sib);
    snprintf(name, sizeof name, "%s/track_light/CY1N.txt.tmp", x.light);
    CHECK(access(name, F_OK) != 0);                                   // the temporary file is gone
    light_session_close(&s);
    free_fixture(&x);
}

static void test_refusals(const char *repo)
{
    Fixture x;
    CHECK(make_fixture(&x, repo));
    LightSession s;
    char why[LIGHT_WHY + 200];
    // a folder with no track_light/ is no light folder
    char bare[EXPORT_PATH + 16];
    snprintf(bare, sizeof bare, "%s/bare", x.root);
    mkdir(bare, 0777);
    CHECK(!light_session_open(&s, bare, "CY1", 'N', why, sizeof why) && strstr(why, "track_light") != NULL);
    CHECK(!light_session_open(&s, "/no/such/folder", "CY1", 'N', why, sizeof why));
    // the CD's own data is never written: any folder under original/ is refused at open
    char under[EXPORT_PATH + 64];
    snprintf(under, sizeof under, "%s/original", x.root);
    mkdir(under, 0777);
    snprintf(under, sizeof under, "%s/original/GAMEDATA", x.root);
    mkdir(under, 0777);
    snprintf(under, sizeof under, "%s/original/GAMEDATA/track_light", x.root);
    mkdir(under, 0777);
    snprintf(under, sizeof under, "%s/original/GAMEDATA", x.root);
    CHECK(!light_session_open(&s, under, "CY1", 'N', why, sizeof why) && strstr(why, "original") != NULL);
    // bad track, bad time
    CHECK(!light_session_open(&s, x.light, "cy1", 'N', why, sizeof why));
    CHECK(!light_session_open(&s, x.light, "CY1", 'X', why, sizeof why));
    // a file that is not this track's: a CY2 header in CY1N.txt
    write_file(x.file, "# tnfs track light 1\ntrack CY2 time N nodes 10 flags fff\n", 52);
    CHECK(!light_session_open(&s, x.light, "CY1", 'N', why, sizeof why) && why[0] != '\0');
    // a track with no file (TR5's light is made in code): opens, read-only, with the reason
    CHECK(light_session_open(&s, x.light, "TR5", 'B', why, sizeof why) && !s.has_file && why[0] != '\0');
    CHECK(!light_session_dirty(&s) && light_session_text(&s, &(size_t){ 0 }) == NULL && light_session_clamp_node(&s, 50) == 0);
    CHECK(!light_session_save(&s, why, sizeof why) && why[0] != '\0');
    light_session_close(&s);
    // a folder that stopped being a light folder after the open: Save refuses and writes nothing
    write_file(x.file, x.source, x.source_size);
    CHECK(light_session_open(&s, x.light, "CY1", 'N', why, sizeof why));
    const LightBrightness b = { 1, 2, 50 };
    CHECK(light_file_set_brightness(&s.file, -1, &b, why, sizeof why));
    char gone[EXPORT_PATH + 64];
    snprintf(gone, sizeof gone, "%s/track_light/CY1N.txt", x.light);
    unlink(gone);
    snprintf(gone, sizeof gone, "%s/track_light", x.light);
    rmdir(gone);
    CHECK(!light_session_save(&s, why, sizeof why) && why[0] != '\0');
    CHECK(access(gone, F_OK) != 0);                                   // Save did not make the folder
    CHECK(light_session_dirty(&s));                                   // and the edit is still there to save elsewhere
    light_session_close(&s);
    free_fixture(&x);
}

static void test_clamp_and_rules(const char *repo)
{
    Fixture x;
    CHECK(make_fixture(&x, repo));
    LightSession s;
    char why[LIGHT_WHY + 200];
    CHECK(light_session_open(&s, x.light, "CY1", 'N', why, sizeof why));
    CHECK(light_session_clamp_node(&s, -5) == 0 && light_session_clamp_node(&s, 70) == 70 && light_session_clamp_node(&s, 1919) == 1919);
    CHECK(light_session_clamp_node(&s, 1920) == 1919 && light_session_clamp_node(&s, 1000000) == 1919);
    // a lamp in a day file is refused with the game's rule, through the model
    LightSession day;
    char path[EXPORT_PATH + 64];
    snprintf(path, sizeof path, "%s/data/track_light/CY1B.txt", repo);
    size_t n = 0;
    char *text = read_file(path, &n);
    CHECK(text != NULL);
    snprintf(path, sizeof path, "%s/track_light/CY1B.txt", x.light);
    write_file(path, text, n);
    free(text);
    CHECK(light_session_open(&day, x.light, "CY1", 'B', why, sizeof why) && day.has_file);
    const LightLamp lamp = { 10, 100, 0, "", 0, 0 };
    CHECK(!light_file_set_lamp(&day.file, -1, &lamp, why, sizeof why) && why[0] != '\0' && !light_session_dirty(&day));
    light_session_close(&day);
    light_session_close(&s);
    free_fixture(&x);
}

int main(int argc, char **argv)
{
    const char *repo = argc > 1 ? argv[1] : ".";
    test_open_edit_save_reload(repo);
    test_refusals(repo);
    test_clamp_and_rules(repo);
    return TEST_REPORT();
}
