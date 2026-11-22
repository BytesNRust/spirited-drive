/* Export original track (editor/original_track_trace.c): a CD track's road as a trace, and as a new track project. argv[1] is the game's files (original/); without
 * SIMDATA/MISC/TR1.TRI the CD blocks are skipped (CI has no original/). */
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

static int exists(const char *rel)
{
    char p[800];
    struct stat st;
    snprintf(p, sizeof p, "%s/%s", root, rel);
    return lstat(p, &st) == 0;
}

static uint8_t *load(const char *cd, const char *stem, size_t *size)
{
    char path[900];
    snprintf(path, sizeof path, "%s/SIMDATA/MISC/%s.TRI", cd, stem);
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    *size = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *d = malloc(*size);
    if (fread(d, 1, *size, f) != *size) free(d), d = NULL;
    fclose(f);
    return d;
}

static void road(const char *cd, const char *stem)
{
    size_t size;
    uint8_t *d = load(cd, stem, &size);
    Trace t = {0};
    char why[300] = "";
    double mx = 99, mn = 99, dy = 99;
    CHECK(d != NULL && original_track_trace(d, size, 4, &t, why, sizeof why));
    CHECK(original_track_trace_error(d, size, &t, &mx, &mn, &dy, why, sizeof why));
    if (mx >= 1.5 || dy >= 3.0) fprintf(stderr, "%s: max %.2f m, dy %.2f m\n", stem, mx, dy);
    CHECK(mx < 1.5);   // the built road is within a metre and a half of the CD's nodes on the plan, even at every fourth node
    CHECK(dy < 3.0);
    free(t.points), free(t.keys), free(d);
}

static void export_flow(const char *cd)
{
    char tracks[800], dir[900], why[300], p[900];
    snprintf(tracks, sizeof tracks, "%s/tracks", root);
    mkdir(tracks, 0755);
    CHECK(!original_track_export(cd, "AL1", tracks, "al-copy", "AL copy", NULL, dir, sizeof dir, why, sizeof why) && strstr(why, "segments") != NULL);
    CHECK(!original_track_export(cd, "TR1", tracks, "Bad_Id", "x", NULL, dir, sizeof dir, why, sizeof why) && strstr(why, "a-z") != NULL);
    CHECK(!original_track_export("/no/such", "TR1", tracks, "tr-copy", "x", NULL, dir, sizeof dir, why, sizeof why));
    CHECK(!exists("tracks/tr-copy") && !exists("tracks/tr-copy.making") && !exists("tracks/al-copy.making"));
    CHECK(original_track_export(cd, "TR1", tracks, "tr-copy", "TR copy", NULL, dir, sizeof dir, why, sizeof why));
    CHECK(strstr(dir, "tracks/tr-copy") != NULL && !exists("tracks/tr-copy.making"));
    Project pr;
    Trace tr = {0};
    CHECK(project_open(dir, &pr, &tr, why, sizeof why));
    CHECK(strcmp(pr.id, "tr-copy") == 0 && strcmp(pr.name, "TR copy") == 0 && tr.count > 100);
    free(tr.points), free(tr.keys);
    snprintf(p, sizeof p, "%s/project.ini", dir);
    CHECK(access(p, R_OK) == 0);
    CHECK(!original_track_export(cd, "TR1", tracks, "tr-copy", "x", NULL, dir, sizeof dir, why, sizeof why) && strstr(why, "already") != NULL);
}

int main(int argc, char **argv)
{
    const char *cd = argc > 1 ? argv[1] : "";
    char probe[900], why[300];
    CHECK(!original_track_trace(NULL, 0, 4, &(Trace){0}, why, sizeof why));
    snprintf(probe, sizeof probe, "%s/SIMDATA/MISC/TR1.TRI", cd);
    if (access(probe, R_OK) != 0) {
        printf("skip: no TR1.TRI in %s\n", cd);
        return TEST_REPORT();
    }
    snprintf(root, sizeof root, "%s/otexport_XXXXXX", getenv("TMPDIR") != NULL ? getenv("TMPDIR") : "/tmp");
    if (mkdtemp(root) == NULL) return perror("mkdtemp"), 1;
    static const char *const TR[] = { "TR1", "TR2", "TR3", "TR4", "TR5", "TR6", "TR7" };
    for (int i = 0; i < 7; ++i) road(cd, TR[i]);
    CHECK(original_track_exportable(cd, "TR2", why, sizeof why));
    CHECK(!original_track_exportable(cd, "CY3", why, sizeof why));
    export_flow(cd);
    nftw(root, rm_entry, 16, FTW_DEPTH | FTW_PHYS);
    return TEST_REPORT();
}
