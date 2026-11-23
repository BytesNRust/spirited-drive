/* Convert PSD (editor/psd_job.c): small PSDs written here; the flows are the real ones: found without case, converted in a thread into the project, a failure
 * leaving the project as it was, the dash too, an older dash colour going, a PSD folder left untouched. */
#include "psd_job.h"

#include "formats/car_mask_codec.h"
#include "formats/png.h"
#include "psd_fixture.h"
#include "psd_manifest.h"
#include "test.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

enum { W = 4 };

static char root[400];

static void path_of(char *out, size_t cap, const char *rel)
{
    snprintf(out, cap, "%s/%s", root, rel);
}

static int put_bytes(const char *rel, const uint8_t *bytes, size_t n)
{
    char p[700];
    path_of(p, sizeof p, rel);
    FILE *f = fopen(p, "wb");
    if (f == NULL) return 0;
    const int ok = fwrite(bytes, 1, n, f) == n;
    return fclose(f) == 0 && ok;
}

static int put(const char *rel, const char *text)
{
    return put_bytes(rel, (const uint8_t *)text, strlen(text));
}

static void dir_of(const char *rel)
{
    char p[700];
    path_of(p, sizeof p, rel);
    mkdir(p, 0755);
}

static int exists(const char *rel)
{
    char p[700];
    path_of(p, sizeof p, rel);
    return access(p, F_OK) == 0;
}

static int content_is(const char *rel, const char *text)
{
    char p[700], bytes[200] = "";
    path_of(p, sizeof p, rel);
    FILE *f = fopen(p, "rb");
    if (f == NULL) return 0;
    const size_t n = fread(bytes, 1, sizeof bytes - 1, f);
    fclose(f);
    bytes[n] = '\0';
    return strcmp(bytes, text) == 0;
}

/* the grey of texel x of mask PNG `rel`, -1 when it is not a PNG */
static int grey_at(const char *rel, int x)
{
    char p[700];
    path_of(p, sizeof p, rel);
    FILE *f = fopen(p, "rb");
    if (f == NULL) return -1;
    uint8_t bytes[4096];
    const size_t n = fread(bytes, 1, sizeof bytes, f);
    fclose(f);
    PngImage img;
    if (png_read(bytes, n, &img) != PNG_OK) return -1;
    const int v = img.indexed ? img.palette[img.pixels[x] * 3] : img.pixels[x * 4];
    png_free(img.pixels);
    return v;
}

static const uint8_t SIDE[W][4] = { { 255, 255, 255, 0 }, { 20, 40, 220, 255 }, { 20, 220, 40, 255 }, { 255, 0, 255, 255 } };   /* paint, shine, body, black */

static void put_side_psd(const char *rel)
{
    FxLayer l[1] = { { "mask", NULL, 0, 0, 0, W, 1, &SIDE[0][0], 0, 1, 0 } };
    FxBuf f = fx_psd(W, 1, l, 1, 8, 3, 1);
    CHECK(put_bytes(rel, f.p, f.n));
    free(f.p);
}

static void put_dash_psd(const char *rel, int with_colour)
{
    static const uint8_t M[W][4] = { { 0, 0, 255, 255 }, { 0, 255, 0, 255 }, { 255, 255, 255, 255 }, { 0, 0, 0, 255 } };
    static const uint8_t C[W][4] = { { 0 }, { 0 }, { 10, 200, 30, 255 }, { 0 } };
    FxLayer l[2] = { { "mask", NULL, 0, 0, 0, W, 1, &M[0][0], 0, 1, 0 }, { "color", NULL, 0, 0, 0, W, 1, &C[0][0], 0, 1, 0 } };
    FxBuf f = fx_psd(W, 1, l, with_colour ? 2 : 1, 8, 3, 1);
    CHECK(put_bytes(rel, f.p, f.n));
    free(f.p);
}

static void run_to_end(PsdJob *job)
{
    for (int i = 0; i < 40000 && psd_job_poll(job); i++) usleep(5000);
}

static char psd_dir[500], piece[500];

/* the legacy project: <piece>/car_masks/<folder>/ and <piece>/dash_masks/<folder>/, no manifest */
static int start(PsdJob *job, const char *folder, char *why, size_t why_cap)
{
    char masks[700], dash[700];
    snprintf(masks, sizeof masks, "%s/car_masks/%s", piece, folder);
    snprintf(dash, sizeof dash, "%s/dash_masks/%s", piece, folder);
    return psd_job_start(job, psd_dir, folder, masks, dash, "", 0, why, why_cap);
}

static void setup(void)
{
    const char *dirs[] = { "psd", "psd/TSupra", "proj", "proj/car_masks", "proj/car_masks/TSUPRA" };
    for (size_t i = 0; i < sizeof dirs / sizeof dirs[0]; i++) dir_of(dirs[i]);
    put_side_psd("psd/TSupra/side.psd");
    put_side_psd("psd/TSupra/frnt.psd");
    put_side_psd("psd/TSupra/side.before-1603.psd");
    path_of(psd_dir, sizeof psd_dir, "psd/TSupra"), path_of(piece, sizeof piece, "proj");
}

static void test_find(void)
{
    char out[600], psd[600];
    path_of(psd, sizeof psd, "psd");
    CHECK(psd_job_find(psd, "TSUPRA", out, sizeof out) && strstr(out, "/psd/TSupra") != NULL);   /* the folder says TSupra, the project TSUPRA */
    CHECK(!psd_job_find(psd, "NOPE", out, sizeof out));
    CHECK(!psd_job_find("/no/such/root", "TSUPRA", out, sizeof out));
}

static void test_body_masks(void)
{
    PsdJob job;
    char why[300];
    psd_job_init(&job);
    put("proj/car_masks/TSUPRA/side_mask.png", "the old side");
    put("proj/car_masks/TSUPRA/mine_mask.png", "not in any PSD");
    CHECK(start(&job, "TSUPRA", why, sizeof why));
    CHECK(psd_job_busy(&job));
    run_to_end(&job);
    CHECK(job.finished && job.ok);
    CHECK_EQ_INT(job.masks, 2);   /* side and frnt: a backup is not a texture */
    CHECK_EQ_INT(grey_at("proj/car_masks/TSUPRA/side_mask.png", 1), CAR_MASK_SHINE);   /* the old file is replaced by the PSD's mask */
    CHECK_EQ_INT(grey_at("proj/car_masks/TSUPRA/frnt_mask.png", 3), CAR_MASK_BLACK);
    CHECK(content_is("proj/car_masks/TSUPRA/mine_mask.png", "not in any PSD"));   /* a mask no PSD makes stays */
    CHECK(!exists("proj/car_masks/TSUPRA/side.before-1603_mask.png"));
    CHECK(!exists("proj/dash_masks"));                                           /* no dash.psd: no dash folder */
    CHECK(strstr(job.text, "2 masks") != NULL);
    CHECK(job.scratch[0] == '\0');                                               /* the scratch folder is gone */
    CHECK(exists("psd/TSupra/side.psd"));                                        /* the PSDs untouched */
    psd_job_free(&job);
}

static void test_failure_leaves_the_project(void)
{
    PsdJob job;
    char why[300];
    psd_job_init(&job);
    put("proj/car_masks/TSUPRA/side_mask.png", "the old side");
    char old[700];
    path_of(old, sizeof old, "proj/car_masks/TSUPRA/frnt_mask.png");
    unlink(old);
    put("psd/TSupra/broken.psd", "this is not a PSD");
    CHECK(start(&job, "TSUPRA", why, sizeof why));
    run_to_end(&job);
    CHECK(job.finished && !job.ok);
    CHECK(strstr(job.text, "broken.psd") != NULL);                                /* the file that could not be read is named */
    CHECK(content_is("proj/car_masks/TSUPRA/side_mask.png", "the old side"));     /* the project as it was */
    CHECK(!exists("proj/car_masks/TSUPRA/frnt_mask.png"));
    CHECK(job.scratch[0] == '\0');
    path_of(old, sizeof old, "psd/TSupra/broken.psd");
    unlink(old);
    psd_job_free(&job);
}

static void test_dash(void)
{
    PsdJob job;
    char why[300];
    psd_job_init(&job);
    put_dash_psd("psd/TSupra/dash.psd", 1);
    CHECK(start(&job, "TSUPRA", why, sizeof why));
    run_to_end(&job);
    CHECK(job.finished && job.ok);
    CHECK_EQ_INT(job.dash_files, 2);
    CHECK(exists("proj/dash_masks/TSUPRA/dash_mask.png") && exists("proj/dash_masks/TSUPRA/dash_color.png"));
    CHECK(!exists("proj/car_masks/TSUPRA/dash_mask.png"));                       /* the dash goes to its own folder */
    CHECK(strstr(job.text, "dash") != NULL);
    /* a dash.psd with no colour layer: the colour of the older conversion goes */
    put_dash_psd("psd/TSupra/dash.psd", 0);
    CHECK(start(&job, "TSUPRA", why, sizeof why));
    CHECK(!start(&job, "TSUPRA", why, sizeof why) && strstr(why, "already") != NULL);   /* busy: a second start is refused */
    run_to_end(&job);
    CHECK(job.ok && job.dash_files == 1);
    CHECK(exists("proj/dash_masks/TSUPRA/dash_mask.png") && !exists("proj/dash_masks/TSUPRA/dash_color.png"));
    psd_job_free(&job);
}

static void test_stop(void)
{
    PsdJob job;
    char why[300];
    psd_job_init(&job);
    CHECK(start(&job, "TSUPRA", why, sizeof why));
    psd_job_stop(&job);   /* ends at once, the project untouched, and says so */
    CHECK(!psd_job_busy(&job) && job.finished && !job.ok && strstr(job.text, "stopped") != NULL);
    psd_job_free(&job);
}

static void test_half_written_never(void)
{
    PsdJob job;
    char why[300];
    psd_job_init(&job);
    put("proj/car_masks/TSUPRA/side_mask.png", "the old side");
    char p[700];
    path_of(p, sizeof p, "proj/dash_masks/TSUPRA/dash_mask.png"), unlink(p);
    path_of(p, sizeof p, "proj/dash_masks/TSUPRA"), rmdir(p);
    path_of(p, sizeof p, "proj/dash_masks"), rmdir(p);
    CHECK(put("proj/dash_masks", "a file in the way"));
    CHECK(start(&job, "TSUPRA", why, sizeof why));
    run_to_end(&job);
    CHECK(job.finished && !job.ok && strstr(job.text, "nothing was changed") != NULL);
    CHECK(content_is("proj/car_masks/TSUPRA/side_mask.png", "the old side"));
    CHECK(!exists("proj/car_masks/TSUPRA/frnt_mask.png.psdtmp") && !exists("proj/car_masks/TSUPRA/side_mask.png.psdtmp"));
    path_of(p, sizeof p, "proj/dash_masks"), unlink(p);
    psd_job_free(&job);
    CHECK(!start(&job, "../x", why, sizeof why) && strstr(why, "too long") != NULL);   /* a folder name that is not one */
    psd_job_free(&job);
}

static void test_kit(void)
{
    PsdJob job;
    char why[300], masks[700], dash[700], manifest[700], sha[MANIFEST_SHA];
    path_of(masks, sizeof masks, "kit/art-generated/car_masks"), path_of(dash, sizeof dash, "kit/art-generated/dash_masks"), path_of(manifest, sizeof manifest, "kit/art-generated/.generated.tsv");
    dir_of("kit");   /* nothing of art-generated/ is there yet: the job makes the folders */
    psd_job_init(&job);
    put_dash_psd("psd/TSupra/dash.psd", 1);
    CHECK(psd_job_start(&job, psd_dir, "TSUPRA", masks, dash, manifest, 0, why, sizeof why));
    run_to_end(&job);
    CHECK(job.finished && job.ok);
    CHECK(exists("kit/art-generated/car_masks/side_mask.png") && exists("kit/art-generated/car_masks/frnt_mask.png"));   /* flat: no folder of the car's name */
    CHECK(exists("kit/art-generated/dash_masks/dash_mask.png") && exists("kit/art-generated/dash_masks/dash_color.png"));
    Manifest m;
    CHECK(manifest_load(manifest, &m) && m.count == 4);   /* a line a PNG written */
    path_of(masks, sizeof masks, "kit/art-generated/car_masks/side_mask.png");
    CHECK(manifest_file_sha(masks, sha) && manifest_sha(&m, "side_mask.png") != NULL && strcmp(manifest_sha(&m, "side_mask.png"), sha) == 0);
    CHECK(manifest_sha(&m, "dash_mask.png") != NULL && manifest_sha(&m, "dash_color.png") != NULL);
    /* a mask somebody else put there, hashed by an earlier run, keeps its line; the colour of the dash goes with a dash.psd that has none */
    CHECK(manifest_set(&m, "other_mask.png", "0123456789012345678901234567890123456789012345678901234567890123") && manifest_save(manifest, &m));
    put_dash_psd("psd/TSupra/dash.psd", 0);
    path_of(masks, sizeof masks, "kit/art-generated/car_masks"), path_of(dash, sizeof dash, "kit/art-generated/dash_masks");
    CHECK(psd_job_start(&job, psd_dir, "TSUPRA", masks, dash, manifest, 0, why, sizeof why));
    run_to_end(&job);
    CHECK(job.ok && !exists("kit/art-generated/dash_masks/dash_color.png"));
    CHECK(manifest_load(manifest, &m) && manifest_sha(&m, "other_mask.png") != NULL && manifest_sha(&m, "dash_color.png") == NULL && manifest_sha(&m, "dash_mask.png") != NULL);
    psd_job_free(&job);
}

/* a PNG somebody edited since Studio made it (its hash is not the manifest's) is kept by a conversion, and replaced only by Regenerate from PSD (force) */
static void test_edited_by_hand(void)
{
    PsdJob job;
    char why[300], masks[700], dash[700], manifest[700], sha_before[MANIFEST_SHA], sha[MANIFEST_SHA], png[700];
    path_of(masks, sizeof masks, "kit4/art-generated/car_masks"), path_of(dash, sizeof dash, "kit4/art-generated/dash_masks"), path_of(manifest, sizeof manifest, "kit4/art-generated/.generated.tsv");
    path_of(png, sizeof png, "kit4/art-generated/car_masks/side_mask.png");
    dir_of("kit4");
    put_dash_psd("psd/TSupra/dash.psd", 1);
    psd_job_init(&job);
    CHECK(psd_job_start(&job, psd_dir, "TSUPRA", masks, dash, manifest, 0, why, sizeof why));
    run_to_end(&job);
    CHECK(job.ok && job.kept == 0 && strstr(job.text, "edited by hand") == NULL);
    CHECK(manifest_file_sha(png, sha_before));
    put("kit4/art-generated/car_masks/side_mask.png", "touched up in GIMP");   /* edited by hand */
    CHECK(psd_job_start(&job, psd_dir, "TSUPRA", masks, dash, manifest, 0, why, sizeof why));
    run_to_end(&job);
    CHECK(job.ok && job.kept == 1 && strcmp(job.kept_names, "side_mask.png") == 0);
    CHECK(content_is("kit4/art-generated/car_masks/side_mask.png", "touched up in GIMP"));   /* not overwritten */
    CHECK(strstr(job.text, "Kept, not replaced: side_mask.png") != NULL && strstr(job.text, "Regenerate from PSD") != NULL);
    CHECK_EQ_INT(job.masks, 1);   /* the other body mask was made again */
    CHECK(exists("kit4/art-generated/dash_masks/dash_mask.png"));
    Manifest m;
    CHECK(manifest_load(manifest, &m) && manifest_sha(&m, "side_mask.png") != NULL && strcmp(manifest_sha(&m, "side_mask.png"), sha_before) == 0);   /* what Studio made is still what is written down */
    CHECK_EQ_INT(manifest_state(&m, masks, "side_mask.png"), GEN_EDITED);
    CHECK(psd_job_start(&job, psd_dir, "TSUPRA", masks, dash, manifest, 1, why, sizeof why));   /* Regenerate from PSD */
    run_to_end(&job);
    CHECK(job.ok && job.kept == 0);
    CHECK(!content_is("kit4/art-generated/car_masks/side_mask.png", "touched up in GIMP"));
    CHECK(manifest_load(manifest, &m) && manifest_file_sha(png, sha) && strcmp(manifest_sha(&m, "side_mask.png"), sha) == 0);
    CHECK_EQ_INT(manifest_state(&m, masks, "side_mask.png"), GEN_OK);
    /* the dash: an edited dash_mask.png is kept, the dash_color.png the PSD makes still comes; an edited dash_color.png is kept when the PSD has no colour layer */
    put("kit4/art-generated/dash_masks/dash_mask.png", "dash edited");
    CHECK(psd_job_start(&job, psd_dir, "TSUPRA", masks, dash, manifest, 0, why, sizeof why));
    run_to_end(&job);
    CHECK(job.ok && strstr(job.kept_names, "dash_mask.png") != NULL && content_is("kit4/art-generated/dash_masks/dash_mask.png", "dash edited"));
    CHECK(exists("kit4/art-generated/dash_masks/dash_color.png"));   /* the colour layer's PNG was not taken for "no colour layer" and removed */
    put("kit4/art-generated/dash_masks/dash_color.png", "colour edited");
    put_dash_psd("psd/TSupra/dash.psd", 0);   /* no colour layer now */
    CHECK(psd_job_start(&job, psd_dir, "TSUPRA", masks, dash, manifest, 0, why, sizeof why));
    run_to_end(&job);
    CHECK(job.ok && content_is("kit4/art-generated/dash_masks/dash_color.png", "colour edited"));   /* the old colour goes, but not one somebody edited */
    CHECK(psd_job_start(&job, psd_dir, "TSUPRA", masks, dash, manifest, 1, why, sizeof why));
    run_to_end(&job);
    CHECK(job.ok && !exists("kit4/art-generated/dash_masks/dash_color.png") && !content_is("kit4/art-generated/dash_masks/dash_mask.png", "dash edited"));   /* Regenerate: all replaced, the colour gone */
    put_dash_psd("psd/TSupra/dash.psd", 1);
    /* every body mask edited: nothing to make, and still not a failure */
    put("kit4/art-generated/car_masks/side_mask.png", "edited");
    put("kit4/art-generated/car_masks/frnt_mask.png", "edited too");
    CHECK(psd_job_start(&job, psd_dir, "TSUPRA", masks, dash, manifest, 0, why, sizeof why));
    run_to_end(&job);
    CHECK(job.ok && job.kept == 2 && job.masks == 0 && strstr(job.kept_names, "frnt_mask.png") != NULL && strstr(job.kept_names, "side_mask.png") != NULL);
    psd_job_free(&job);
}

/* a kit from before the manifest: no line for any PNG. One that is not what the PSD gives is somebody's edit and is kept (with the warning); one that is the same is made again, and the manifest then has its line */
static void test_no_manifest_line(void)
{
    PsdJob job;
    char why[300], masks[700], dash[700], manifest[700], png[700];
    path_of(masks, sizeof masks, "kit5/art-generated/car_masks"), path_of(dash, sizeof dash, "kit5/art-generated/dash_masks"), path_of(manifest, sizeof manifest, "kit5/art-generated/.generated.tsv");
    path_of(png, sizeof png, "kit5/art-generated/car_masks/side_mask.png");
    dir_of("kit5");
    put_dash_psd("psd/TSupra/dash.psd", 1);
    psd_job_init(&job);
    CHECK(psd_job_start(&job, psd_dir, "TSUPRA", masks, dash, manifest, 0, why, sizeof why));
    run_to_end(&job);
    CHECK(job.ok && job.kept == 0);
    unlink(manifest);   /* the kit as an earlier Studio left it */
    put("kit5/art-generated/car_masks/side_mask.png", "somebody's own side mask");
    put("kit5/art-generated/dash_masks/dash_color.png", "somebody's own colour");
    put_dash_psd("psd/TSupra/dash.psd", 0);
    CHECK(psd_job_start(&job, psd_dir, "TSUPRA", masks, dash, manifest, 0, why, sizeof why));
    run_to_end(&job);
    CHECK(job.ok && job.kept == 1 && strcmp(job.kept_names, "side_mask.png") == 0);
    CHECK(content_is("kit5/art-generated/car_masks/side_mask.png", "somebody's own side mask"));
    CHECK(content_is("kit5/art-generated/dash_masks/dash_color.png", "somebody's own colour"));   /* nor is the colour a dash.psd without one would drop */
    CHECK(strstr(job.text, "Kept, not replaced: side_mask.png") != NULL);
    Manifest m;
    CHECK(manifest_load(manifest, &m) && manifest_sha(&m, "frnt_mask.png") != NULL && manifest_sha(&m, "side_mask.png") == NULL);   /* the one that matched is Studio's again; the kept one is not claimed */
    CHECK_EQ_INT(manifest_state(&m, masks, "frnt_mask.png"), GEN_OK);
    CHECK(psd_job_start(&job, psd_dir, "TSUPRA", masks, dash, manifest, 1, why, sizeof why));   /* Regenerate from PSD replaces them */
    run_to_end(&job);
    CHECK(job.ok && job.kept == 0 && !content_is("kit5/art-generated/car_masks/side_mask.png", "somebody's own side mask") && !exists("kit5/art-generated/dash_masks/dash_color.png"));
    psd_job_free(&job);
}

static void remove_tree(void)
{
    char cmd[700];
    snprintf(cmd, sizeof cmd, "find '%s' -depth -delete", root);
    if (system(cmd) != 0) fprintf(stderr, "could not remove %s\n", root);
}

int main(void)
{
    const char *tmp = getenv("TMPDIR");
    snprintf(root, sizeof root, "%s/tnfs-psd-job-XXXXXX", tmp != NULL && tmp[0] != '\0' ? tmp : "/tmp");
    if (mkdtemp(root) == NULL) {
        fprintf(stderr, "no scratch folder\n");
        return 1;
    }
    setup();
    test_find();
    test_body_masks();
    test_failure_leaves_the_project();
    test_dash();
    test_stop();
    test_half_written_never();
    test_kit();
    test_edited_by_hand();
    test_no_manifest_line();
    remove_tree();
    return TEST_REPORT();
}
