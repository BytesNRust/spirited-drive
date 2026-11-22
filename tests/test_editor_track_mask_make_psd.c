/* Create PSD (editor/track_mask_make_psd.c): the texture goes to a scratch folder as <STEM>/<name>.rgb.png, the script is run, its PSD ends up in art-source/masks, never over
 * one that is there, and a failing script is said in words with nothing left behind. The interpreter is /bin/sh and the "script" a shell script: no Pillow needed. */
#include "track_mask_make_psd.h"
#include "test.h"

#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char root[400], tools[500], proj[500], scratch_parent[500];

static int rm_entry(const char *path, const struct stat *st, int flag, struct FTW *ftw)
{
    (void)st, (void)flag, (void)ftw;
    return remove(path);
}

static void put(const char *path, const char *text)
{
    FILE *f = fopen(path, "w");
    if (f != NULL) fputs(text, f), fclose(f);
}

static int count_entries(const char *dir)
{
    char cmd[700];
    snprintf(cmd, sizeof cmd, "ls -A '%s' | wc -l", dir);
    FILE *p = popen(cmd, "r");
    int n = -1;
    if (p != NULL && fscanf(p, "%d", &n) != 1) n = -1;
    if (p != NULL) pclose(p);
    return n;
}

int main(void)
{
    const uint8_t px[4 * 4 * 4] = { 0 };
    char why[300], path[800];
    snprintf(root, sizeof root, "%s/mkpsd_XXXXXX", getenv("TMPDIR") != NULL ? getenv("TMPDIR") : "/tmp");
    if (mkdtemp(root) == NULL) return perror("mkdtemp"), 1;
    snprintf(tools, sizeof tools, "%s/tools", root), mkdir(tools, 0755);
    snprintf(proj, sizeof proj, "%s/proj", root), mkdir(proj, 0755);
    snprintf(scratch_parent, sizeof scratch_parent, "%s/tmp", root), mkdir(scratch_parent, 0755);
    setenv("TMPDIR", scratch_parent, 1);
    // the fake script: $1 the drafts folder, $2 the output; makes <out>/<STEM>/<name>.psd from every <drafts>/<STEM>/<name>.rgb.png
    snprintf(path, sizeof path, "%s/track_mask_psd.py", tools);
    put(path, "for f in \"$1\"/*/*.rgb.png; do s=$(basename \"$(dirname \"$f\")\"); n=$(basename \"$f\" .rgb.png); mkdir -p \"$2/$s\"; echo psd > \"$2/$s/$n.psd\"; done\n");

    CHECK(track_mask_make_psd("/bin/sh", tools, proj, "AL1", "slot_00", px, 4, 4, why, sizeof why));
    snprintf(path, sizeof path, "%s/art-source/masks/slot_00.psd", proj);
    CHECK(access(path, R_OK) == 0);
    CHECK(count_entries(scratch_parent) == 0);   // the scratch folder is gone
    CHECK(!track_mask_make_psd("/bin/sh", tools, proj, "AL1", "slot_00", px, 4, 4, why, sizeof why) && strstr(why, "already") != NULL);   // never over a PSD
    CHECK(!track_mask_make_psd("/bin/sh", "", proj, "AL1", "slot_01", px, 4, 4, why, sizeof why) && strstr(why, "tools folder") != NULL);
    CHECK(!track_mask_make_psd("/bin/sh", tools, proj, "AL1", "slot_01", NULL, 4, 4, why, sizeof why) && strstr(why, "no pixels") != NULL);
    snprintf(path, sizeof path, "%s/track_mask_psd.py", tools);
    put(path, "echo 'ModuleNotFoundError: No module named PIL' >&2; exit 1\n");
    CHECK(!track_mask_make_psd("/bin/sh", tools, proj, "AL1", "slot_02", px, 4, 4, why, sizeof why) && strstr(why, "Pillow") != NULL);
    put(path, "echo 'something broke' >&2; exit 3\n");
    CHECK(!track_mask_make_psd("/bin/sh", tools, proj, "AL1", "slot_02", px, 4, 4, why, sizeof why) && strstr(why, "something broke") != NULL);
    snprintf(path, sizeof path, "%s/art-source/masks/slot_02.psd", proj);
    CHECK(access(path, F_OK) != 0);   // nothing left behind
    CHECK(count_entries(scratch_parent) == 0);
    nftw(root, rm_entry, 16, FTW_DEPTH | FTW_PHYS);
    return TEST_REPORT();
}
