/* Convert PSD (editor/psd_job.c): fake scripts (python, stdlib only) stand in for psd-tools; the flows are the real ones: found without case, converted
 * into the project, a failure leaving the project as it was, the dash too, a PSD folder left untouched. */
#include "psd_job.h"
#include "test.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char root[400];

static void path_of(char *out, size_t cap, const char *rel)
{
    snprintf(out, cap, "%s/%s", root, rel);
}

static int put(const char *rel, const char *text)
{
    char p[700];
    path_of(p, sizeof p, rel);
    FILE *f = fopen(p, "wb");
    if (f == NULL) return 0;
    const size_t n = strlen(text);
    const int ok = fwrite(text, 1, n, f) == n;
    return fclose(f) == 0 && ok;
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

/* the fake scripts: <dest>/<CAR>/<name> files; FAKE_PSD_FAIL=1 makes the body script fail after writing one file */
static const char BODY[] =
    "import os, sys\n"
    "src, dest = sys.argv[1], sys.argv[2]\n"
    "car = os.path.basename(src.rstrip('/'))\n"
    "os.makedirs(os.path.join(dest, car), exist_ok=True)\n"
    "for n in sorted(os.listdir(src)):\n"
    "    if n.endswith('.psd') and n != 'dash.psd' and '.' not in n[:-4]:\n"
    "        open(os.path.join(dest, car, n[:-4] + '_mask.png'), 'w').write('png of ' + n)\n"
    "        if os.environ.get('FAKE_PSD_FAIL'):\n"
    "            print('psd-tools exploded on ' + n); sys.exit(3)\n";
static const char DASH[] =
    "import os, sys\n"
    "src, dest = sys.argv[1], sys.argv[2]\n"
    "car = os.path.basename(src.rstrip('/'))\n"
    "os.makedirs(os.path.join(dest, car), exist_ok=True)\n"
    "open(os.path.join(dest, car, 'dash_mask.png'), 'w').write('dash mask')\n"
    "open(os.path.join(dest, car, 'dash_color.png'), 'w').write('dash colour')\n";

static void run_to_end(PsdJob *job)
{
    for (int i = 0; i < 4000 && psd_job_poll(job); i++) usleep(5000);
}

static char tools[500], psd_dir[500], piece[500];

static void setup(void)
{
    const char *dirs[] = { "tools", "psd", "psd/TSupra", "proj", "proj/car_masks", "proj/car_masks/TSUPRA" };
    for (size_t i = 0; i < sizeof dirs / sizeof dirs[0]; i++) dir_of(dirs[i]);
    put("tools/car_mask_from_psd.py", BODY);
    put("tools/dash_mask_from_psd.py", DASH);
    put("psd/TSupra/side.psd", "psd bytes");
    put("psd/TSupra/frnt.psd", "psd bytes");
    put("psd/TSupra/side.before-1603.psd", "backup");
    path_of(tools, sizeof tools, "tools"), path_of(psd_dir, sizeof psd_dir, "psd/TSupra"), path_of(piece, sizeof piece, "proj");
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
    CHECK(psd_job_start(&job, tools, psd_dir, "TSUPRA", piece, why, sizeof why));
    CHECK(psd_job_busy(&job));
    char scratch[700];
    snprintf(scratch, sizeof scratch, "%s", job.scratch);
    CHECK(scratch[0] != '\0' && access(scratch, F_OK) == 0);                      /* a scratch folder while it runs */
    run_to_end(&job);
    CHECK(job.finished && job.ok);
    CHECK_EQ_INT(job.masks, 2);   /* side and frnt: a backup is not a texture */
    CHECK(content_is("proj/car_masks/TSUPRA/side_mask.png", "png of side.psd"));
    CHECK(content_is("proj/car_masks/TSUPRA/frnt_mask.png", "png of frnt.psd"));
    CHECK(content_is("proj/car_masks/TSUPRA/mine_mask.png", "not in any PSD"));   /* a mask no PSD makes stays */
    CHECK(!exists("proj/dash_masks"));                                           /* no dash.psd: no dash folder */
    CHECK(strstr(job.text, "2 masks") != NULL);
    CHECK(access(scratch, F_OK) != 0);                                           /* the scratch folder is gone */
    CHECK(exists("psd/TSupra/side.psd") && content_is("psd/TSupra/side.psd", "psd bytes"));   /* the PSDs untouched */
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
    setenv("FAKE_PSD_FAIL", "1", 1);
    CHECK(psd_job_start(&job, tools, psd_dir, "TSUPRA", piece, why, sizeof why));
    char scratch[700];
    snprintf(scratch, sizeof scratch, "%s", job.scratch);
    run_to_end(&job);
    unsetenv("FAKE_PSD_FAIL");
    CHECK(job.finished && !job.ok);
    CHECK(strstr(job.text, "exploded") != NULL);                                  /* what the script said */
    CHECK(content_is("proj/car_masks/TSUPRA/side_mask.png", "the old side"));     /* the project as it was */
    CHECK(!exists("proj/car_masks/TSUPRA/frnt_mask.png"));
    CHECK(access(scratch, F_OK) != 0);
    psd_job_free(&job);
}

static void test_dash_and_refusals(void)
{
    PsdJob job;
    char why[300];
    psd_job_init(&job);
    put("psd/TSupra/dash.psd", "dash psd");
    CHECK(psd_job_start(&job, tools, psd_dir, "TSUPRA", piece, why, sizeof why));
    run_to_end(&job);
    CHECK(job.finished && job.ok);
    CHECK_EQ_INT(job.dash_files, 2);
    CHECK(content_is("proj/dash_masks/TSUPRA/dash_mask.png", "dash mask"));
    CHECK(content_is("proj/dash_masks/TSUPRA/dash_color.png", "dash colour"));
    CHECK(!exists("proj/car_masks/TSUPRA/dash_mask.png"));                       /* the dash goes to its own folder */
    CHECK(strstr(job.text, "dash") != NULL);
    /* busy: a second start is refused; no scripts: said */
    CHECK(psd_job_start(&job, tools, psd_dir, "TSUPRA", piece, why, sizeof why));
    CHECK(!psd_job_start(&job, tools, psd_dir, "TSUPRA", piece, why, sizeof why) && strstr(why, "already") != NULL);
    run_to_end(&job);
    CHECK(!psd_job_start(&job, "/no/tools", psd_dir, "TSUPRA", piece, why, sizeof why) && strstr(why, "car_mask_from_psd.py") != NULL);
    psd_job_free(&job);
}

static void remove_tree(void)
{
    const char *files[] = { "tools/car_mask_from_psd.py", "tools/dash_mask_from_psd.py", "psd/TSupra/side.psd", "psd/TSupra/frnt.psd", "psd/TSupra/side.before-1603.psd", "psd/TSupra/dash.psd",
                            "proj/car_masks/TSUPRA/side_mask.png", "proj/car_masks/TSUPRA/mine_mask.png", "proj/car_masks/TSUPRA/frnt_mask.png",
                            "proj/dash_masks/TSUPRA/dash_mask.png", "proj/dash_masks/TSUPRA/dash_color.png" };
    char p[700];
    for (size_t i = 0; i < sizeof files / sizeof files[0]; i++) path_of(p, sizeof p, files[i]), unlink(p);
    const char *dirs[] = { "tools", "psd/TSupra", "psd", "proj/car_masks/TSUPRA", "proj/car_masks", "proj/dash_masks/TSUPRA", "proj/dash_masks", "proj" };
    for (size_t i = 0; i < sizeof dirs / sizeof dirs[0]; i++) path_of(p, sizeof p, dirs[i]), rmdir(p);
    rmdir(root);
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
    test_dash_and_refusals();
    remove_tree();
    return TEST_REPORT();
}
