/* The manifest of generated masks (editor/psd_manifest.h): a hash a PNG, saved and read back, an edited PNG told from an untouched one, one the manifest does not know. */
#include "psd_manifest.h"
#include "test.h"

#define _XOPEN_SOURCE 700
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char root[512];

static int drop(const char *p, const struct stat *st, int flag, struct FTW *w)
{
    (void)st, (void)flag, (void)w;
    return remove(p);
}

static void put(const char *name, const char *text)
{
    char p[1024];
    snprintf(p, sizeof p, "%s/%s", root, name);
    FILE *f = fopen(p, "wb");
    if (f != NULL) fputs(text, f), fclose(f);
}

static void test_hash(void)
{
    char sha[MANIFEST_SHA];
    put("abc.png", "abc");
    char p[1024];
    snprintf(p, sizeof p, "%s/abc.png", root);
    CHECK(manifest_file_sha(p, sha) && strcmp(sha, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") == 0);   /* FIPS 180-4's "abc" */
    snprintf(p, sizeof p, "%s/nothing.png", root);
    CHECK(!manifest_file_sha(p, sha));
}

static void test_state(void)
{
    Manifest m = { 0 };
    put("frnt_mask.png", "one");
    put("rear_mask.png", "two");
    CHECK_EQ_INT(manifest_state(&m, root, "frnt_mask.png"), GEN_UNKNOWN);   /* no line: nothing is claimed */
    CHECK_EQ_INT(manifest_state(&m, root, "gone_mask.png"), GEN_MISSING);
    CHECK(manifest_record(&m, root, "frnt_mask.png") && manifest_record(&m, root, "rear_mask.png"));
    CHECK_EQ_INT(manifest_state(&m, root, "frnt_mask.png"), GEN_OK);
    put("frnt_mask.png", "one, touched up in GIMP");
    CHECK_EQ_INT(manifest_state(&m, root, "frnt_mask.png"), GEN_EDITED);
    CHECK_EQ_INT(manifest_state(&m, root, "rear_mask.png"), GEN_OK);
    CHECK(manifest_record(&m, root, "frnt_mask.png"));   /* a regeneration writes the new hash over the old */
    CHECK_EQ_INT(manifest_state(&m, root, "frnt_mask.png"), GEN_OK);
    CHECK_EQ_INT(m.count, 2);
    CHECK(!manifest_record(&m, root, "gone_mask.png"));
    CHECK(manifest_forget(&m, "frnt_mask.png") && !manifest_forget(&m, "frnt_mask.png") && m.count == 1);   /* a file no longer made is no longer written down */
    CHECK_EQ_INT(manifest_state(&m, root, "frnt_mask.png"), GEN_UNKNOWN);
    CHECK_EQ_INT(manifest_state(&m, root, "rear_mask.png"), GEN_OK);
}

/* 64 hex digits of one digit: a hash that reads back */
static const char *hex(char digit)
{
    static char text[4][MANIFEST_SHA];
    static int next;
    char *out = text[next++ % 4];
    memset(out, digit, MANIFEST_SHA - 1), out[MANIFEST_SHA - 1] = '\0';
    return out;
}

static void test_save_load(void)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/.generated.tsv", root);
    Manifest m = { 0 }, back = { 0 };
    CHECK(manifest_load(path, &back) && back.count == 0);   /* no file: an empty manifest */
    CHECK(manifest_set(&m, "rear_mask.png", hex('a')) && manifest_set(&m, "dash_mask.png", hex('b')) && manifest_set(&m, "frnt_mask.png", hex('c')) && manifest_set(&m, "dash_mask.png", hex('d')));
    CHECK_EQ_INT(m.count, 3);
    CHECK(manifest_save(path, &m) && manifest_load(path, &back) && back.count == 3);
    CHECK(manifest_sha(&back, "dash_mask.png") != NULL && strcmp(manifest_sha(&back, "dash_mask.png"), hex('d')) == 0);
    CHECK(manifest_sha(&back, "frnt_mask.png") != NULL && strcmp(manifest_sha(&back, "frnt_mask.png"), hex('c')) == 0 && manifest_sha(&back, "nope.png") == NULL);
    FILE *f = fopen(path, "rb");
    char text[256] = "";
    if (f != NULL) text[fread(text, 1, sizeof text - 1, f)] = '\0', fclose(f);
    char expect[512];
    snprintf(expect, sizeof expect, "dash_mask.png\t%s\nfrnt_mask.png\t%s\nrear_mask.png\t%s\n", hex('d'), hex('c'), hex('a'));
    CHECK(strcmp(text, expect) == 0);   /* sorted by name: a stable file */
    put(".generated.tsv", "garbage\nok_mask.png\t0123456789012345678901234567890123456789012345678901234567890123\n\t\nno tab here\n");
    Manifest rough = { 0 };
    CHECK(manifest_load(path, &rough) && rough.count == 1 && manifest_sha(&rough, "ok_mask.png") != NULL);   /* a line that is not a line is skipped */
    char long_name[MANIFEST_NAME + 8];
    memset(long_name, 'x', sizeof long_name - 1), long_name[sizeof long_name - 1] = '\0';
    CHECK(!manifest_set(&m, long_name, hex('e')));
    for (int i = m.count; i < MANIFEST_MAX; ++i) {
        char name[32];
        snprintf(name, sizeof name, "n%d_mask.png", i);
        CHECK(manifest_set(&m, name, hex('f')));
    }
    CHECK(!manifest_set(&m, "one_more_mask.png", hex('f')));   /* full */
    snprintf(path, sizeof path, "%s/no-such-folder/.generated.tsv", root);
    CHECK(!manifest_save(path, &m));
}

int main(void)
{
    const char *tmp = getenv("TMPDIR") != NULL ? getenv("TMPDIR") : "/tmp";
    snprintf(root, sizeof root, "%s/psd-manifest-test-%d", tmp, (int)getpid());
    mkdir(root, 0777);
    test_hash();
    test_state();
    test_save_load();
    nftw(root, drop, 8, FTW_DEPTH | FTW_PHYS);
    return TEST_REPORT();
}
