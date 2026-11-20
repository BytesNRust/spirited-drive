/* The Track light window's file model (editor/light_file.h, dep98 step 2 part A): a light file is held as its lines, only brightness, lamp and tunnel_tone are the window's,
 * and a file that was not changed is written back as the same bytes. The round trip runs on every light file and places file of the repo (data/track_light, data/track):
 * they are in the repo, so this test needs no CD data. usage: test_editor_light_file REPO */
#include "light_file.h"
#include "test.h"

#ifndef _WIN32
#include <dirent.h>   // the folder walk is POSIX; a Windows build runs the rest of the test
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

#ifndef _WIN32
/* every .txt of a folder through the model: parsed, formatted, the same bytes. Returns how many files */
static int round_trip_folder(const char *dir, int places)
{
    DIR *d = opendir(dir);
    CHECK(d != NULL);
    if (d == NULL) return 0;
    int files = 0;
    for (struct dirent *e; (e = readdir(d)) != NULL;) {
        const size_t len = strlen(e->d_name);
        if (len < 5 || strcmp(e->d_name + len - 4, ".txt") != 0) continue;
        char path[1024];
        snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
        size_t size = 0;
        char *text = read_file(path, &size);
        CHECK(text != NULL);
        if (text == NULL) continue;
        LightFile f;
        char why[LIGHT_WHY];
        const int ok = light_file_parse(text, size, &f, why, sizeof why);
        if (!ok) fprintf(stderr, "%s: %s\n", path, why);
        CHECK(ok);
        if (ok) {
            size_t out_size = 0;
            char *out = light_file_format(&f, &out_size);
            CHECK(out != NULL && out_size == size && memcmp(out, text, size) == 0);
            CHECK(places ? f.letter == 0 : f.letter != 0);
            CHECK(light_file_first_bad(&f) == 0);   // the files the game reads have no malformed owned line
            free(out);
            light_file_free(&f);
        }
        free(text);
        ++files;
    }
    closedir(d);
    return files;
}

static void test_repo_files(const char *repo)
{
    char dir[1024];
    snprintf(dir, sizeof dir, "%s/data/track_light", repo);
    CHECK(round_trip_folder(dir, 0) >= 60);   // 15 tracks x 4 times
    snprintf(dir, sizeof dir, "%s/data/track", repo);
    CHECK(round_trip_folder(dir, 1) >= 15);
}
#else
static void test_repo_files(const char *repo) { (void)repo; }
#endif

static void test_real_night_file(const char *repo)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/data/track_light/CY1N.txt", repo);
    size_t size = 0;
    char *text = read_file(path, &size);
    CHECK(text != NULL);
    if (text == NULL) return;
    LightFile f;
    char why[LIGHT_WHY];
    CHECK(light_file_parse(text, size, &f, why, sizeof why));
    CHECK(strcmp(f.track, "CY1") == 0);
    CHECK(f.letter == 'N');
    CHECK_EQ_INT(f.nodes, 1920);
    CHECK_EQ_INT(light_file_count(&f, LIGHT_KIND_LAMP), 170);
    CHECK_EQ_INT(light_file_count(&f, LIGHT_KIND_TUNNEL_TONE), 1);
    CHECK_EQ_INT(light_file_count(&f, LIGHT_KIND_BRIGHTNESS), 0);
    LightTunnelTone tone;
    CHECK(light_file_get_tunnel_tone(&f, 0, &tone));
    CHECK(tone.light > 104.39 && tone.light < 104.41);
    CHECK(tone.has_colour && strcmp(tone.colour, "1/0.35/0.05") == 0);
    LightLamp lamp;
    CHECK(light_file_get_lamp(&f, 0, &lamp));
    CHECK(!lamp.has_colour && !lamp.has_radius && lamp.percent == 100);
    CHECK(!light_file_get_lamp(&f, 170, &lamp));
    light_file_free(&f);
    free(text);
    snprintf(path, sizeof path, "%s/data/track/CY1.txt", repo);
    text = read_file(path, &size);
    CHECK(text != NULL);
    if (text == NULL) return;
    CHECK(light_file_parse(text, size, &f, why, sizeof why));
    CHECK(strcmp(f.track, "CY1") == 0 && f.letter == 0 && f.nodes == 0);
    CHECK_EQ_INT(light_file_count(&f, LIGHT_KIND_LAMP), 0);   // a places file has nothing the window edits
    light_file_free(&f);
    free(text);
}

static const char SAMPLE[] =
    "# tnfs track light 1\n"
    "track CY1 time N nodes 100 flags fff\n"
    "rows 0 99 255 255 61 61 62 63 255 60 61 62 63\n"
    "# the owner's note, kept\n"
    "brightness 10 20 70\n"
    "brillo 30 40 50\n"                // a Spanish word: kept as it is, not the window's
    "frobnicate 1 2 3\n"               // a word nobody knows: kept
    "lamp 5 100\n"
    "lamp 50 150 ff8020:60 8\n"
    "tunnel_tone 104.4 1/0.35/0.05\n";

static LightFile sample(const char *text)
{
    LightFile f;
    char why[LIGHT_WHY];
    const int ok = light_file_parse(text, strlen(text), &f, why, sizeof why);
    if (!ok) fprintf(stderr, "parse: %s\n", why);
    CHECK(ok);
    return f;
}

static char *formatted(const LightFile *f)
{
    size_t size = 0;
    char *text = light_file_format(f, &size);
    CHECK(text != NULL);
    return text;
}

static void test_classification(void)
{
    LightFile f = sample(SAMPLE);
    CHECK_EQ_INT(light_file_count(&f, LIGHT_KIND_BRIGHTNESS), 1);
    CHECK_EQ_INT(light_file_count(&f, LIGHT_KIND_LAMP), 2);
    CHECK_EQ_INT(light_file_count(&f, LIGHT_KIND_TUNNEL_TONE), 1);
    CHECK_EQ_INT(light_file_count(&f, LIGHT_KIND_BAD), 0);
    CHECK_EQ_INT(light_file_count(&f, LIGHT_KIND_OTHER), 6);   // 2 comments, the header, rows, brillo, frobnicate
    LightBrightness b;
    CHECK(light_file_get_brightness(&f, 0, &b) && b.from == 10 && b.to == 20 && b.percent == 70);
    LightLamp lamp;
    CHECK(light_file_get_lamp(&f, 1, &lamp) && lamp.node == 50 && lamp.percent == 150 && lamp.has_colour && strcmp(lamp.colour, "ff8020:60") == 0 &&
          lamp.has_radius && lamp.radius == 8);
    char *text = formatted(&f);
    CHECK(text != NULL && strcmp(text, SAMPLE) == 0);
    free(text);
    light_file_free(&f);
}

static void test_edits_touch_only_their_line(void)
{
    LightFile f = sample(SAMPLE);
    char why[LIGHT_WHY];
    const LightBrightness set = { 12, 24, 30 };
    CHECK(light_file_set_brightness(&f, 0, &set, why, sizeof why));
    char *text = formatted(&f);
    CHECK(text != NULL && strstr(text, "brightness 12 24 30\n") != NULL && strstr(text, "brightness 10 20 70") == NULL);
    // everything else is the same lines in the same order: the sample with that one line swapped
    char want[sizeof SAMPLE + 8];
    const char *old = strstr(SAMPLE, "brightness 10 20 70\n");
    snprintf(want, sizeof want, "%.*sbrightness 12 24 30\n%s", (int)(old - SAMPLE), SAMPLE, old + strlen("brightness 10 20 70\n"));
    CHECK(text != NULL && strcmp(text, want) == 0);
    free(text);
    // a new line goes after the last line
    const LightBrightness added = { 60, 70, 40 };
    CHECK(light_file_set_brightness(&f, -1, &added, why, sizeof why));
    text = formatted(&f);
    const char *tail = "tunnel_tone 104.4 1/0.35/0.05\nbrightness 60 70 40\n";
    CHECK(text != NULL && strlen(text) > strlen(tail) && strcmp(text + strlen(text) - strlen(tail), tail) == 0);
    free(text);
    CHECK_EQ_INT(light_file_count(&f, LIGHT_KIND_BRIGHTNESS), 2);
    // removing drops that line and nothing else
    CHECK(light_file_remove(&f, LIGHT_KIND_BRIGHTNESS, 1));
    CHECK(light_file_remove(&f, LIGHT_KIND_BRIGHTNESS, 0));
    CHECK(!light_file_remove(&f, LIGHT_KIND_BRIGHTNESS, 0));
    text = formatted(&f);
    CHECK(text != NULL && strstr(text, "brightness") == NULL && strstr(text, "brillo 30 40 50\n") != NULL && strstr(text, "# the owner's note, kept\n") != NULL);
    free(text);
    light_file_free(&f);
}

static void test_lamps_and_tunnel_tone(void)
{
    LightFile f = sample(SAMPLE);
    char why[LIGHT_WHY];
    const LightLamp lamp = { 70, 200, 1, "ff2000", 1, 12 };
    CHECK(light_file_set_lamp(&f, 0, &lamp, why, sizeof why));
    LightLamp got;
    CHECK(light_file_get_lamp(&f, 0, &got) && got.node == 70 && got.percent == 200 && strcmp(got.colour, "ff2000") == 0 && got.radius == 12);
    char *text = formatted(&f);
    CHECK(text != NULL && strstr(text, "lamp 70 200 ff2000 12\n") != NULL);
    free(text);
    // the tunnel look is replaced where it is, never added twice
    const LightTunnelTone tone = { 60, 1, "ff5a0d" };
    CHECK(light_file_set_tunnel_tone(&f, 0, &tone, why, sizeof why));
    CHECK(!light_file_set_tunnel_tone(&f, -1, &tone, why, sizeof why) && why[0] != '\0');   // a second one: refused with the reason
    text = formatted(&f);
    CHECK(text != NULL && strstr(text, "tunnel_tone 60 ff5a0d\n") != NULL && strstr(text, "1/0.35/0.05") == NULL);
    free(text);
    CHECK(light_file_remove(&f, LIGHT_KIND_TUNNEL_TONE, 0));
    CHECK_EQ_INT(light_file_count(&f, LIGHT_KIND_TUNNEL_TONE), 0);
    const LightTunnelTone plain = { 100, 0, "" };
    CHECK(light_file_set_tunnel_tone(&f, -1, &plain, why, sizeof why));
    text = formatted(&f);
    CHECK(text != NULL && strstr(text, "tunnel_tone 100\n") != NULL);
    free(text);
    light_file_free(&f);
}

static void test_rules_of_the_game(void)
{
    LightFile f = sample(SAMPLE);
    char why[LIGHT_WHY];
    char *before = formatted(&f);
    LightBrightness b = { 20, 10, 50 };   // unordered
    CHECK(!light_file_set_brightness(&f, -1, &b, why, sizeof why) && why[0] != '\0');
    b = (LightBrightness){ 10, 100, 50 };   // the last node is 99
    CHECK(!light_file_set_brightness(&f, -1, &b, why, sizeof why));
    b = (LightBrightness){ 10, 20, 1001 };
    CHECK(!light_file_set_brightness(&f, -1, &b, why, sizeof why));
    b = (LightBrightness){ 10, 20, 50 };
    CHECK(!light_file_set_brightness(&f, 5, &b, why, sizeof why));   // no such line to set
    LightLamp l = { 100, 100, 0, "", 0, 0 };
    CHECK(!light_file_set_lamp(&f, -1, &l, why, sizeof why));   // node 100 of 100
    l = (LightLamp){ 5, 0, 0, "", 0, 0 };
    CHECK(!light_file_set_lamp(&f, -1, &l, why, sizeof why));   // 0 percent
    l = (LightLamp){ 5, 1001, 0, "", 0, 0 };
    CHECK(!light_file_set_lamp(&f, -1, &l, why, sizeof why));
    l = (LightLamp){ 5, 100, 1, "zzzzzz", 0, 0 };
    CHECK(!light_file_set_lamp(&f, -1, &l, why, sizeof why));
    l = (LightLamp){ 5, 100, 1, "ff8020:101", 0, 0 };
    CHECK(!light_file_set_lamp(&f, -1, &l, why, sizeof why));
    l = (LightLamp){ 5, 100, 0, "", 1, 65 };
    CHECK(!light_file_set_lamp(&f, -1, &l, why, sizeof why));
    l = (LightLamp){ 5, 100, 0, "", 1, 0 };
    CHECK(!light_file_set_lamp(&f, -1, &l, why, sizeof why));
    LightTunnelTone t = { 10001, 0, "" };
    CHECK(!light_file_set_tunnel_tone(&f, 0, &t, why, sizeof why));
    t = (LightTunnelTone){ 50, 1, "red" };
    CHECK(!light_file_set_tunnel_tone(&f, 0, &t, why, sizeof why));
    t = (LightTunnelTone){ -1, 0, "" };
    CHECK(!light_file_set_tunnel_tone(&f, 0, &t, why, sizeof why));
    char *after = formatted(&f);
    CHECK(before != NULL && after != NULL && strcmp(before, after) == 0);   // every refusal left the file as it was
    free(before);
    free(after);
    // the colours the game takes: hex, hex:strength, r/g/b, r/g/b:strength
    static const char *const OK[] = { "ff8020", "ff8020:50", "1/0.5/0.2", "1/0.5/0.2:80" };
    for (size_t i = 0; i < sizeof OK / sizeof OK[0]; ++i) {
        LightTunnelTone ok = { 50, 1, "" };
        snprintf(ok.colour, sizeof ok.colour, "%s", OK[i]);
        CHECK(light_file_set_tunnel_tone(&f, 0, &ok, why, sizeof why));
    }
    light_file_free(&f);
}

static void test_limits(void)
{
    char text[8192];
    snprintf(text, sizeof text, "# tnfs track light 1\ntrack CY1 time N nodes 3000 flags fff\n");
    LightFile f = sample(text);
    char why[LIGHT_WHY];
    for (int i = 0; i < LIGHT_MAX_BRIGHTNESS; ++i) {
        const LightBrightness b = { (unsigned)i, (unsigned)i + 1, 50 };
        CHECK(light_file_set_brightness(&f, -1, &b, why, sizeof why));
    }
    const LightBrightness more = { 1, 2, 50 };
    CHECK(!light_file_set_brightness(&f, -1, &more, why, sizeof why));   // the 65th
    for (int i = 0; i < LIGHT_MAX_LAMPS; ++i) {
        const LightLamp l = { (unsigned)i, 100, 0, "", 0, 0 };
        if (!light_file_set_lamp(&f, -1, &l, why, sizeof why)) { CHECK(0); break; }
    }
    const LightLamp l = { 5, 100, 0, "", 0, 0 };
    CHECK(!light_file_set_lamp(&f, -1, &l, why, sizeof why));   // the 2049th
    light_file_free(&f);
}

static void test_lamp_only_at_night(void)
{
    LightFile f = sample("# tnfs track light 1\ntrack CY1 time B nodes 100 flags fff\n");
    char why[LIGHT_WHY];
    const LightLamp l = { 5, 100, 0, "", 0, 0 };
    CHECK(!light_file_set_lamp(&f, -1, &l, why, sizeof why) && strstr(why, "night") != NULL);   // the game ignores a lamp by day: the reason says so
    const LightBrightness b = { 1, 2, 50 };
    CHECK(light_file_set_brightness(&f, -1, &b, why, sizeof why));   // a brightness is for any time
    light_file_free(&f);
}

static void test_odd_files(void)
{
    // no final newline: kept, and a new line still starts on its own
    LightFile f = sample("# tnfs track light 1\ntrack CY1 time N nodes 100 flags fff\nlamp 5 100");
    char *text = formatted(&f);
    CHECK(text != NULL && strcmp(text, "# tnfs track light 1\ntrack CY1 time N nodes 100 flags fff\nlamp 5 100") == 0);
    free(text);
    char why[LIGHT_WHY];
    const LightBrightness b = { 1, 2, 50 };
    CHECK(light_file_set_brightness(&f, -1, &b, why, sizeof why));
    text = formatted(&f);
    CHECK(text != NULL && strcmp(text, "# tnfs track light 1\ntrack CY1 time N nodes 100 flags fff\nlamp 5 100\nbrightness 1 2 50") == 0);
    free(text);
    light_file_free(&f);
    // CRLF: kept, new lines written the same way
    f = sample("# tnfs track light 1\r\ntrack CY1 time N nodes 100 flags fff\r\nlamp 5 100\r\n");
    text = formatted(&f);
    CHECK(text != NULL && strcmp(text, "# tnfs track light 1\r\ntrack CY1 time N nodes 100 flags fff\r\nlamp 5 100\r\n") == 0);
    free(text);
    CHECK_EQ_INT(light_file_count(&f, LIGHT_KIND_LAMP), 1);   // the "\r" is no part of the value
    CHECK(light_file_set_brightness(&f, -1, &b, why, sizeof why));
    text = formatted(&f);
    CHECK(text != NULL && strcmp(text, "# tnfs track light 1\r\ntrack CY1 time N nodes 100 flags fff\r\nlamp 5 100\r\nbrightness 1 2 50\r\n") == 0);
    free(text);
    light_file_free(&f);
    // blank lines and an empty tail are kept
    f = sample("# tnfs track light 1\n\ntrack CY1 time N nodes 100 flags fff\n\n\n");
    text = formatted(&f);
    CHECK(text != NULL && strcmp(text, "# tnfs track light 1\n\ntrack CY1 time N nodes 100 flags fff\n\n\n") == 0);
    free(text);
    light_file_free(&f);
}

static void test_malformed_owned_lines(void)
{
    static const char TEXT[] = "# tnfs track light 1\ntrack CY1 time N nodes 100 flags fff\nbrightness 20 10 50\nlamp 5 100\nlamp 500 100\nbrightness 1 2\ntunnel_tone\nbrightness 1 2 3 4\n";
    LightFile f = sample(TEXT);
    CHECK_EQ_INT(light_file_count(&f, LIGHT_KIND_BAD), 5);   // unordered, node out of range, two numbers, no light, four numbers
    CHECK_EQ_INT(light_file_count(&f, LIGHT_KIND_LAMP), 1);
    CHECK_EQ_INT(light_file_first_bad(&f), 3);
    char *text = formatted(&f);
    CHECK(text != NULL && strcmp(text, TEXT) == 0);   // kept as written
    free(text);
    CHECK(!light_file_remove(&f, LIGHT_KIND_BAD, 0));   // a bad line is not the window's to remove
    CHECK_EQ_INT(light_file_count(&f, LIGHT_KIND_BAD), 5);
    light_file_free(&f);
}

static void test_refused_headers(void)
{
    LightFile f;
    char why[LIGHT_WHY];
    static const char *const BAD[] = {
        "", "rows 0 0 1 2 3\n", "# tnfs track light 1\ntrack CY1 time X nodes 100 flags fff\n", "# tnfs track light 1\ntrack CY1 time N nodes 0 flags fff\n",
        "# tnfs track light 1\ntrack CY1 time N flags fff\n", "# tnfs track light 1\ntrack TOOLONGNAME time N nodes 100\n",
    };
    for (size_t i = 0; i < sizeof BAD / sizeof BAD[0]; ++i) {
        why[0] = '\0';
        CHECK(!light_file_parse(BAD[i], strlen(BAD[i]), &f, why, sizeof why) && why[0] != '\0');
    }
    // the Spanish header words of older files read; a places file is a header with no time
    static const char SPANISH[] = "pista TR2 hora C nodos 50 banderas 1\n", PLACES[] = "# tnfs track places 1\ntrack TR2\n";
    CHECK(light_file_parse(SPANISH, strlen(SPANISH), &f, why, sizeof why) && strcmp(f.track, "TR2") == 0 && f.letter == 'C' && f.nodes == 50);
    light_file_free(&f);
    CHECK(light_file_parse(PLACES, strlen(PLACES), &f, why, sizeof why) && f.letter == 0);
    light_file_free(&f);
}

int main(int argc, char **argv)
{
    const char *repo = argc > 1 ? argv[1] : ".";
    test_classification();
    test_edits_touch_only_their_line();
    test_lamps_and_tunnel_tone();
    test_rules_of_the_game();
    test_limits();
    test_lamp_only_at_night();
    test_odd_files();
    test_malformed_owned_lines();
    test_refused_headers();
    test_real_night_file(repo);
    test_repo_files(repo);
    return TEST_REPORT();
}
