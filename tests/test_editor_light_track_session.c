/* The Track light window's session on the new light file (editor/light_track_session.h): open, edit, dirty, revert, save, the folder rules, and the TRI's node count against the
 * `nodes` of the old light files. usage: test_editor_light_track_session REPO [CD] (the CD part runs only with the game's files). */
#include "light_track_session.h"
#include "original_track_trace.h"
#include "test.h"

#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char root[400];

static int rm_entry(const char *path, const struct stat *st, int flag, struct FTW *ftw)
{
    (void)st, (void)flag, (void)ftw;
    return remove(path);
}

static void write_file(const char *path, const char *text)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL) return;
    fputs(text, f);
    fclose(f);
}

static char *slurp(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    char *text = calloc(1, 1 << 16);
    *size = text != NULL ? fread(text, 1, (1 << 16) - 1, f) : 0;
    fclose(f);
    return text;
}

static const char *const FILE_TEXT = "# tnfs track light 2\ntrack CY1\ntunnel 10 20\nsnow 0\n\ntime A\n  sun 150 20 55\n  tunnel_look 15 20 90\n";

static void folder_rules(void)
{
    char dir[600], why[300];
    LightTrackSession s;
    snprintf(dir, sizeof dir, "%s/plain", root);
    mkdir(dir, 0755);
    CHECK(!light_track_session_open(&s, dir, "CY1", 100, why, sizeof why) && strstr(why, "track_light") != NULL);   // not a light folder
    snprintf(dir, sizeof dir, "%s/light", root);
    mkdir(dir, 0755);
    snprintf(dir, sizeof dir, "%s/light/track_light", root);
    mkdir(dir, 0755);
    snprintf(dir, sizeof dir, "%s/light", root);
    CHECK(!light_track_session_open(&s, dir, "cy1", 100, why, sizeof why) && strstr(why, "stem") != NULL);
    CHECK(!light_track_session_open(&s, dir, "CY1", 0, why, sizeof why) && strstr(why, "node count") != NULL);
    CHECK(light_track_session_open(&s, dir, "CY1", 100, why, sizeof why) && !s.has_file && strstr(why, "nothing to edit") != NULL);   // no file: opens, empty
    CHECK(!light_track_session_dirty(&s) && light_track_session_text(&s, &(size_t){0}) == NULL);
    char none[8] = "";
    light_track_session_path(&s, none, sizeof none);
    CHECK(none[0] == '\0');
    light_track_session_close(&s);
}

static void edit_save(void)
{
    char dir[600], path[700], why[300];
    LightTrackSession s;
    snprintf(dir, sizeof dir, "%s/light", root);
    snprintf(path, sizeof path, "%s/track_light/CY1.txt", dir);
    write_file(path, FILE_TEXT);
    CHECK(light_track_session_open(&s, dir, "CY1", 100, why, sizeof why) && s.has_file);
    CHECK(!light_track_session_dirty(&s));
    CHECK(light_track_session_clamp_node(&s, -5) == 0 && light_track_session_clamp_node(&s, 40) == 40 && light_track_session_clamp_node(&s, 4000) == 99);
    LightTrackSun sun = { 1, 1, 1, 100.0, 30.0, 80.0 };
    CHECK(light_track_set_sun(&s.file, LT_TIME_A, &sun, why, sizeof why));
    CHECK(light_track_session_dirty(&s));
    light_track_session_revert(&s);
    CHECK(!light_track_session_dirty(&s));
    CHECK(light_track_set_sun(&s.file, LT_TIME_A, &sun, why, sizeof why) && light_track_session_save(&s, why, sizeof why) && !light_track_session_dirty(&s));
    size_t size = 0;
    char *on_disk = slurp(path, &size);
    CHECK(on_disk != NULL && strstr(on_disk, "sun 100 30 80") != NULL && strstr(on_disk, "tunnel 10 20") != NULL);   // only the line changed
    free(on_disk);
    char tmp[720];
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    CHECK(access(tmp, F_OK) != 0);   // no temporary file left
    light_track_session_close(&s);
    char other[700];   // the file of another track under this name is refused
    snprintf(other, sizeof other, "%s/track_light/CY2.txt", dir);
    write_file(other, FILE_TEXT);
    CHECK(!light_track_session_open(&s, dir, "CY2", 100, why, sizeof why) && strstr(why, "not of CY2") != NULL);
}

static void under_original_folder(void)
{
    char dir[700], sub[800], why[300];
    LightTrackSession s;
    snprintf(dir, sizeof dir, "%s/Original", root);
    mkdir(dir, 0755);
    snprintf(sub, sizeof sub, "%s/track_light", dir);
    mkdir(sub, 0755);
    CHECK(!light_track_session_open(&s, dir, "CY1", 100, why, sizeof why) && strstr(why, "original") != NULL);
}

/* the TRI's node count is the `nodes` of the old light files (data/track_light/<TRACK>A.txt): `track CY1 time B nodes 1920 flags 3ff` */
static void nodes_of_the_tri(const char *repo, const char *cd)
{
    static const char *const TRACKS[] = { "AL1", "AL2", "AL3", "CL1", "CL2", "CL3", "CY1", "CY2", "CY3", "TR1", "TR2", "TR3", "TR4", "TR6", "TR7" };
    char probe[800], why[200];
    snprintf(probe, sizeof probe, "%s/SIMDATA/MISC/AL1.TRI", cd);
    if (access(probe, R_OK) != 0) {
        printf("skip: no AL1.TRI in %s\n", cd);
        return;
    }
    for (size_t i = 0; i < sizeof TRACKS / sizeof *TRACKS; i++) {
        char path[900];
        snprintf(path, sizeof path, "%s/data/track_light/%sA.txt", repo, TRACKS[i]);
        size_t size = 0;
        char *text = slurp(path, &size);
        const char *at = text != NULL ? strstr(text, " nodes ") : NULL;
        unsigned want = at != NULL ? (unsigned)strtoul(at + 7, NULL, 10) : 0, got = 0;
        CHECK(want > 0 && original_track_nodes(cd, TRACKS[i], &got, why, sizeof why));
        if (got != want) fprintf(stderr, "%s: the TRI says %u nodes, the file %u\n", TRACKS[i], got, want);
        CHECK(got == want);
        free(text);
    }
    unsigned n = 0;
    CHECK(!original_track_nodes(cd, "ZZ9", &n, why, sizeof why) && strstr(why, "game's folder") != NULL);
}

int main(int argc, char **argv)
{
    snprintf(root, sizeof root, "%s/light_track_session_XXXXXX", getenv("TMPDIR") != NULL ? getenv("TMPDIR") : "/tmp");
    if (mkdtemp(root) == NULL) return perror("mkdtemp"), 1;
    folder_rules();
    edit_save();
    under_original_folder();
    if (argc > 2) nodes_of_the_tri(argv[1], argv[2]);
    nftw(root, rm_entry, 16, FTW_DEPTH | FTW_PHYS);
    return TEST_REPORT();
}
