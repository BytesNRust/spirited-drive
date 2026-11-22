/* The Track light session's places file (editor/light_session.h, dep98 part D): the track's data/track/<STEM>.txt opens with the time's file, its tunnel, canyon and car_dim
 * ranges are edited through the same working copy, the photo gets both, and Save writes each only when it changed, only in a folder with track_light/ and track/, never
 * under original/. A copy of a repo light file and places file in a temp folder, so no CD data. usage: test_editor_light_places REPO */
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

typedef struct {
    char root[EXPORT_PATH], light[EXPORT_PATH], file[EXPORT_PATH], places[EXPORT_PATH];
    char *source, *places_source;
    size_t source_size, places_size;
} Fixture;

static int make_fixture(Fixture *x, const char *repo, int with_track_dir)
{
    char user[EXPORT_PATH], why[200], path[EXPORT_PATH + 64];
    if (!car_race_temp(x->root, sizeof x->root, user, sizeof user, why, sizeof why)) return 0;
    snprintf(x->light, sizeof x->light, "%s/light", x->root);
    mkdir(x->light, 0777);
    snprintf(path, sizeof path, "%s/track_light", x->light);
    mkdir(path, 0777);
    snprintf(path, sizeof path, "%s/data/track_light/CY1N.txt", repo);
    x->source = read_file(path, &x->source_size);
    snprintf(path, sizeof path, "%s/data/track/CY1.txt", repo);
    x->places_source = read_file(path, &x->places_size);
    if (x->source == NULL || x->places_source == NULL) return 0;
    snprintf(x->file, sizeof x->file, "%s/track_light/CY1N.txt", x->light);
    write_file(x->file, x->source, x->source_size);
    if (with_track_dir) {
        snprintf(path, sizeof path, "%s/track", x->light);
        mkdir(path, 0777);
        snprintf(x->places, sizeof x->places, "%s/track/CY1.txt", x->light);
        write_file(x->places, x->places_source, x->places_size);
    }
    return 1;
}

static void free_fixture(Fixture *x)
{
    free(x->source);
    free(x->places_source);
    car_race_temp_remove(x->root);
}

static int same_file(const char *path, const char *bytes, size_t size)
{
    size_t n = 0;
    char *now = read_file(path, &n);
    const int same = now != NULL && n == size && memcmp(now, bytes, size) == 0;
    free(now);
    return same;
}

static void test_open_edit_save(const char *repo)
{
    Fixture x;
    CHECK(make_fixture(&x, repo, 1));
    LightSession s;
    char why[LIGHT_WHY + 200];
    CHECK(light_session_open(&s, x.light, "CY1", 'N', why, sizeof why) && s.has_file && s.has_places);
    CHECK(strcmp(s.places.track, "CY1") == 0 && s.places.letter == 0);
    CHECK(!light_session_dirty(&s));
    CHECK(light_file_count(&s.places, LIGHT_KIND_PLACE) >= 1);
    LightPlace p;
    CHECK(light_file_get_place(&s.places, 0, &p) && strcmp(p.what, "tunnel") == 0);
    size_t before_size = 0;
    char *before = light_session_places_text(&s, &before_size);
    CHECK(before != NULL && before_size == x.places_size && memcmp(before, x.places_source, x.places_size) == 0);
    free(before);
    // an edit of a place: dirty, the photo's text has it, the time's file does not
    CHECK(light_file_set_place(&s.places, -1, &(LightPlace){ "canyon", 10, 20 }, s.file.nodes, why, sizeof why));
    CHECK(light_session_dirty(&s));
    size_t n = 0;
    char *text = light_session_places_text(&s, &n);
    CHECK(text != NULL && strstr(text, "canyon 10 20") != NULL);
    free(text);
    CHECK(light_session_save(&s, why, sizeof why));
    CHECK(!light_session_dirty(&s));
    size_t on_disk = 0;
    char *disk = read_file(x.places, &on_disk);
    CHECK(disk != NULL && strstr(disk, "canyon 10 20") != NULL);
    free(disk);
    CHECK(same_file(x.file, x.source, x.source_size));   // the time's file did not change: not written
    // revert brings both back
    CHECK(light_file_set_place(&s.places, 0, &(LightPlace){ "car_dim", 1, 2 }, s.file.nodes, why, sizeof why));
    CHECK(light_file_set_brightness(&s.file, -1, &(LightBrightness){ 1, 2, 50, LIGHT_SIDE_BOTH, 0 }, why, sizeof why));
    CHECK(light_session_dirty(&s));
    light_session_revert(&s);
    CHECK(!light_session_dirty(&s));
    char *back = light_session_places_text(&s, &n);
    CHECK(back != NULL && strstr(back, "canyon 10 20") != NULL && strstr(back, "car_dim 1 2") == NULL);
    free(back);
    light_session_close(&s);
    free_fixture(&x);
}

static void test_no_places_file(const char *repo)
{
    Fixture x;
    CHECK(make_fixture(&x, repo, 0));   // no track/ folder at all
    LightSession s;
    char why[LIGHT_WHY + 200];
    CHECK(light_session_open(&s, x.light, "CY1", 'N', why, sizeof why) && s.has_file && !s.has_places);
    CHECK(light_session_places_text(&s, &(size_t){ 0 }) == NULL);
    CHECK(light_file_set_brightness(&s.file, -1, &(LightBrightness){ 1, 2, 50, LIGHT_SIDE_BOTH, 0 }, why, sizeof why));
    CHECK(light_session_save(&s, why, sizeof why));   // the time's file saves as ever
    light_session_close(&s);
    free_fixture(&x);
}

static void test_refusals(const char *repo)
{
    Fixture x;
    CHECK(make_fixture(&x, repo, 1));
    LightSession s;
    char why[LIGHT_WHY + 200];
    CHECK(light_session_open(&s, x.light, "CY1", 'N', why, sizeof why) && s.has_places);
    CHECK(light_file_set_place(&s.places, -1, &(LightPlace){ "tunnel", 3, 4 }, s.file.nodes, why, sizeof why));
    // the track/ folder goes away: the places cannot be saved, and then nothing is written (the time's file included)
    CHECK(light_file_set_brightness(&s.file, -1, &(LightBrightness){ 1, 2, 50, LIGHT_SIDE_BOTH, 0 }, why, sizeof why));
    unlink(x.places);
    char dir[EXPORT_PATH + 16];
    snprintf(dir, sizeof dir, "%s/track", x.light);
    rmdir(dir);
    CHECK(!light_session_save(&s, why, sizeof why) && strstr(why, "track") != NULL);
    CHECK(same_file(x.file, x.source, x.source_size));   // all or nothing: the time's file is as it was
    light_session_close(&s);
    free_fixture(&x);
    // a folder under original/: refused at open, as before
    Fixture y;
    CHECK(make_fixture(&y, repo, 1));
    char under[EXPORT_PATH + 32];
    snprintf(under, sizeof under, "%s/original", y.root);
    mkdir(under, 0777);
    char sub[EXPORT_PATH + 64];
    snprintf(sub, sizeof sub, "%s/original/light", y.root);
    mkdir(sub, 0777);
    snprintf(sub, sizeof sub, "%s/original/light/track_light", y.root);
    mkdir(sub, 0777);
    snprintf(sub, sizeof sub, "%s/original/light", y.root);
    CHECK(!light_session_open(&s, sub, "CY1", 'N', why, sizeof why) && strstr(why, "original") != NULL);
    free_fixture(&y);
}

int main(int argc, char **argv)
{
    const char *repo = argc > 1 ? argv[1] : ".";
    test_open_edit_save(repo);
    test_no_places_file(repo);
    test_refusals(repo);
    return TEST_REPORT();
}
