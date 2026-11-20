/* Rehacer (editor/car_rehacer.c): the backup of a kit before its script runs, kept to the last few; the script run with the kit as its folder, its output
 * and exit code taken; the script that is missing. Fixtures are made here with a fake rehacer.command. */
#include "car_rehacer.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static char base[512];

static void path_in(char *out, size_t cap, const char *rel)
{
    snprintf(out, cap, "%s/%s", base, rel);
}

static void make_dir(const char *rel)
{
    char p[1024];
    path_in(p, sizeof p, rel);
    mkdir(p, 0777);
}

static void write_file(const char *rel, const char *text)
{
    char p[1024];
    path_in(p, sizeof p, rel);
    FILE *f = fopen(p, "wb");
    if (f != NULL) fputs(text, f), fclose(f);
}

static int read_file(const char *rel, char *out, size_t cap)
{
    char p[1024];
    path_in(p, sizeof p, rel);
    FILE *f = fopen(p, "rb");
    if (f == NULL) return 0;
    const size_t n = fread(out, 1, cap - 1, f);
    out[n] = '\0';
    fclose(f);
    return 1;
}

static int is_file(const char *rel)
{
    char p[1024];
    struct stat st;
    path_in(p, sizeof p, rel);
    return stat(p, &st) == 0 && S_ISREG(st.st_mode);
}

static int is_dir(const char *rel)
{
    char p[1024];
    struct stat st;
    path_in(p, sizeof p, rel);
    return stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

static void make_kit(const char *kit, const char *script)
{
    char rel[256];
    make_dir(kit);
    static const char *const DIRS[] = { "coche", "coche/SIMDATA", "coche/SIMDATA/CARFAMS", "coche/car_masks", "coche/car_masks/K1", "modelo", "modelo/c1", "specs" };
    for (size_t i = 0; i < sizeof DIRS / sizeof DIRS[0]; i++) {
        snprintf(rel, sizeof rel, "%s/%s", kit, DIRS[i]);
        make_dir(rel);
    }
    snprintf(rel, sizeof rel, "%s/coche/mod.ini", kit), write_file(rel, "id=k1\n");
    snprintf(rel, sizeof rel, "%s/coche/SIMDATA/CARFAMS/K1.CFM", kit), write_file(rel, "the car file as it was");
    snprintf(rel, sizeof rel, "%s/coche/car_masks/K1/rsid_mask.png", kit), write_file(rel, "mask");
    snprintf(rel, sizeof rel, "%s/modelo/c1/0010_rsid.png", kit), write_file(rel, "texture");
    snprintf(rel, sizeof rel, "%s/specs/specs.tsv", kit), write_file(rel, "specs");
    snprintf(rel, sizeof rel, "%s/.DS_Store", kit), write_file(rel, "noise");
    if (script != NULL) {
        snprintf(rel, sizeof rel, "%s/rehacer.command", kit);
        write_file(rel, script);
    }
}

static char *kit_path(const char *kit)
{
    static char p[1024];
    path_in(p, sizeof p, kit);
    return p;
}

static void check_script_present(void)
{
    make_kit("withscript", "#!/bin/sh\nexit 0\n");
    make_kit("noscript", NULL);
    CHECK(rehacer_script_present(kit_path("withscript")));
    CHECK(!rehacer_script_present(kit_path("noscript")));
    make_dir("asdir");
    make_dir("asdir/rehacer.command");   /* a folder of that name is not a script */
    CHECK(!rehacer_script_present(kit_path("asdir")));
    CHECK(!rehacer_script_present("/home/p/nowhere"));
    CHECK(!rehacer_script_present(NULL));
}

static void check_stamp(void)
{
    char s[32];
    rehacer_stamp(s, sizeof s);
    CHECK_EQ_INT((int)strlen(s), 15);
    CHECK(s[8] == '-');
    for (int i = 0; i < 15; i++)
        if (i != 8) CHECK(s[i] >= '0' && s[i] <= '9');
}

static void check_backup_copies_the_kit(void)
{
    char out[1024], why[200], got[200];
    make_kit("bk", "#!/bin/sh\nexit 0\n");
    make_dir("bk/.studio-backup");
    make_dir("bk/.studio-backup/20200101-000000");   /* an older backup: not copied into the new one */
    write_file("bk/.studio-backup/20200101-000000/x.txt", "old");
    CHECK_EQ_INT(rehacer_backup(kit_path("bk"), "20261119-221000", REHACER_KEEP, 100, out, sizeof out, why, sizeof why), 1);
    CHECK(strstr(out, "bk/.studio-backup/20261119-221000") != NULL);
    CHECK(is_file("bk/.studio-backup/20261119-221000/coche/SIMDATA/CARFAMS/K1.CFM"));
    CHECK(read_file("bk/.studio-backup/20261119-221000/coche/SIMDATA/CARFAMS/K1.CFM", got, sizeof got) && strcmp(got, "the car file as it was") == 0);
    CHECK(is_file("bk/.studio-backup/20261119-221000/coche/car_masks/K1/rsid_mask.png") && is_file("bk/.studio-backup/20261119-221000/modelo/c1/0010_rsid.png"));
    CHECK(is_file("bk/.studio-backup/20261119-221000/specs/specs.tsv") && is_file("bk/.studio-backup/20261119-221000/rehacer.command"));
    CHECK(!is_dir("bk/.studio-backup/20261119-221000/.studio-backup"));   /* never into itself */
    CHECK(!is_file("bk/.studio-backup/20261119-221000/.DS_Store"));
    char p[1024];
    struct stat st;
    path_in(p, sizeof p, "bk/.studio-backup/20261119-221000/rehacer.command");
    CHECK(stat(p, &st) == 0 && st.st_size == 17);
    /* the same stamp again: another folder, nothing overwritten */
    CHECK_EQ_INT(rehacer_backup(kit_path("bk"), "20261119-221000", REHACER_KEEP, 100, out, sizeof out, why, sizeof why), 1);
    CHECK(strstr(out, "20261119-221000-2") != NULL && is_file("bk/.studio-backup/20261119-221000-2/coche/mod.ini"));
}

static void check_rotation(void)
{
    char out[1024], why[200];
    make_kit("rot", NULL);
    make_dir("rot/.studio-backup");
    make_dir("rot/.studio-backup/notes");   /* what is not a stamp is never touched */
    write_file("rot/.studio-backup/notes/keep.txt", "mine");
    write_file("rot/.studio-backup/readme.txt", "mine");
    static const char *const STAMPS[] = { "20261101-100000", "20261102-100000", "20261103-100000", "20261104-100000", "20261105-100000", "20261106-100000",
                                          "20261107-100000" };
    for (size_t i = 0; i < sizeof STAMPS / sizeof STAMPS[0]; i++) {
        CHECK_EQ_INT(rehacer_backup(kit_path("rot"), STAMPS[i], REHACER_KEEP, 100, out, sizeof out, why, sizeof why), 1);
    }
    char names[16][REHACER_STAMP_NAME];
    const int n = rehacer_backups(kit_path("rot"), names, 16);
    CHECK_EQ_INT(n, REHACER_KEEP);   /* the last five */
    CHECK(n == REHACER_KEEP && strcmp(names[0], "20261103-100000") == 0 && strcmp(names[4], "20261107-100000") == 0);
    CHECK(!is_dir("rot/.studio-backup/20261101-100000") && !is_dir("rot/.studio-backup/20261102-100000"));
    CHECK(is_file("rot/.studio-backup/20261103-100000/coche/mod.ini"));
    CHECK(is_file("rot/.studio-backup/notes/keep.txt") && is_file("rot/.studio-backup/readme.txt"));
    CHECK_EQ_INT(rehacer_backup(kit_path("rot"), "20261108-100000", 2, 100, out, sizeof out, why, sizeof why), 1);   /* a smaller keep trims more */
    CHECK_EQ_INT(rehacer_backups(kit_path("rot"), names, 16), 2);
    CHECK(strcmp(names[0], "20261107-100000") == 0 && strcmp(names[1], "20261108-100000") == 0);
    CHECK_EQ_INT(rehacer_backups(kit_path("noscript"), names, 16), 0);
}

static void check_backup_refusals(void)
{
    char out[1024], why[200];
    make_kit("big", NULL);
    char *blob = malloc(2 * 1024 * 1024);
    CHECK(blob != NULL);
    if (blob != NULL) {
        memset(blob, 'b', 2 * 1024 * 1024 - 1), blob[2 * 1024 * 1024 - 1] = '\0';
        write_file("big/modelo/big.blend", blob);
        free(blob);
    }
    CHECK_EQ_INT(rehacer_backup(kit_path("big"), "20261119-000001", REHACER_KEEP, 1, out, sizeof out, why, sizeof why), 0);   /* over 1 MB */
    CHECK(strstr(why, "1 MB") != NULL && out[0] == '\0');
    CHECK(!is_dir("big/.studio-backup/20261119-000001"));
    CHECK_EQ_INT(rehacer_backup(kit_path("big"), "20261119-000001", REHACER_KEEP, 3, out, sizeof out, why, sizeof why), 1);
    CHECK_EQ_INT(rehacer_backup("/home/p/nowhere", "20261119-000002", REHACER_KEEP, 100, out, sizeof out, why, sizeof why), 0);
    CHECK_EQ_INT(rehacer_backup(kit_path("big"), "../evil", REHACER_KEEP, 100, out, sizeof out, why, sizeof why), 0);   /* not a stamp */
    CHECK_EQ_INT(rehacer_backup(kit_path("big"), "", REHACER_KEEP, 100, out, sizeof out, why, sizeof why), 0);
    /* a backup that fails part way leaves no partial folder: a file that cannot be read */
    make_kit("part", NULL);
    write_file("part/coche/locked.bin", "x");
    char p[1024];
    path_in(p, sizeof p, "part/coche/locked.bin");
    chmod(p, 0000);
    if (access(p, R_OK) != 0) {   /* not as root */
        CHECK_EQ_INT(rehacer_backup(kit_path("part"), "20261119-000003", REHACER_KEEP, 100, out, sizeof out, why, sizeof why), 0);
        CHECK(!is_dir("part/.studio-backup/20261119-000003") && strstr(why, "locked.bin") != NULL);
    }
    chmod(p, 0600);
}

/* runs the script to its end (bounded) */
static void run_to_end(Rehacer *r)
{
    for (int i = 0; i < 4000 && rehacer_poll(r); i++) usleep(5000);
}

static void check_run(void)
{
    char why[200], got[400];
    make_kit("run", "#!/bin/sh\necho \"cwd is $(pwd)\"\necho \"to stderr\" >&2\nread answer\necho \"read gave: [$answer]\"\n"
                    "echo proof > proof.txt\necho changed > coche/SIMDATA/CARFAMS/K1.CFM\nexit 3\n");
    Rehacer r;
    memset(&r, 0, sizeof r);
    CHECK_EQ_INT(rehacer_start(&r, kit_path("run"), why, sizeof why), 1);
    CHECK(r.run.running && !r.run.finished);
    run_to_end(&r);
    CHECK(!r.run.running && r.run.finished);
    CHECK_EQ_INT(r.run.exit_code, 3);
    CHECK(r.run.log != NULL && strstr(r.run.log, "to stderr") != NULL && strstr(r.run.log, "read gave: []") != NULL);   /* both streams; no input */
    CHECK(r.run.log != NULL && strstr(r.run.log, "run") != NULL && strstr(r.run.log, "cwd is ") != NULL);
    CHECK(is_file("run/proof.txt"));   /* the kit was its folder */
    CHECK(read_file("run/coche/SIMDATA/CARFAMS/K1.CFM", got, sizeof got) && strcmp(got, "changed\n") == 0);
    CHECK_EQ_INT(rehacer_poll(&r), 0);   /* polled again: nothing more */
    rehacer_free(&r);
    CHECK(r.run.log == NULL);

    make_kit("ok", "#!/bin/sh\necho fine\nexit 0\n");   /* the exit code is told as it is */
    memset(&r, 0, sizeof r);
    CHECK_EQ_INT(rehacer_start(&r, kit_path("ok"), why, sizeof why), 1);
    run_to_end(&r);
    CHECK(r.run.finished && r.run.exit_code == 0 && strstr(r.run.log, "fine") != NULL);
    rehacer_free(&r);

    make_kit("noexec", "echo \"no shebang, no exec bit\"\n");   /* run with sh: the script need not be executable */
    memset(&r, 0, sizeof r);
    CHECK_EQ_INT(rehacer_start(&r, kit_path("noexec"), why, sizeof why), 1);
    run_to_end(&r);
    CHECK(r.run.finished && r.run.exit_code == 0 && strstr(r.run.log, "no shebang") != NULL);
    rehacer_free(&r);

    memset(&r, 0, sizeof r);   /* no script: refused, with a plain reason */
    CHECK_EQ_INT(rehacer_start(&r, kit_path("noscript"), why, sizeof why), 0);
    CHECK(strstr(why, "rehacer.command") != NULL && !r.run.running);
    CHECK_EQ_INT(rehacer_start(&r, "/home/p/nowhere", why, sizeof why), 0);
}

static void check_long_output_and_stop(void)
{
    char why[200];
    make_kit("noisy", "#!/bin/sh\ni=0\nwhile [ $i -lt 6000 ]; do echo \"line $i of the output of a script that talks a lot\"; i=$((i+1)); done\necho THE-END\n");
    Rehacer r;
    memset(&r, 0, sizeof r);
    CHECK_EQ_INT(rehacer_start(&r, kit_path("noisy"), why, sizeof why), 1);
    run_to_end(&r);
    CHECK(r.run.finished && r.run.exit_code == 0);
    CHECK(r.run.log_len <= REHACER_LOG_MAX && r.run.log_cut && strstr(r.run.log, "THE-END") != NULL && strstr(r.run.log, "line 0 of") == NULL);   /* the last of it is kept */
    rehacer_free(&r);

    make_kit("slow", "#!/bin/sh\necho started\nsleep 30\necho never\n");
    memset(&r, 0, sizeof r);
    CHECK_EQ_INT(rehacer_start(&r, kit_path("slow"), why, sizeof why), 1);
    for (int i = 0; i < 400 && (r.run.log == NULL || strstr(r.run.log, "started") == NULL); i++) rehacer_poll(&r), usleep(5000);
    CHECK(r.run.running && r.run.log != NULL && strstr(r.run.log, "started") != NULL);
    rehacer_stop(&r);
    run_to_end(&r);
    CHECK(r.run.finished && r.run.stopped && r.run.exit_code == 128 + 15);   /* ended by TERM */
    CHECK(strstr(r.run.log, "never") == NULL);
    rehacer_free(&r);

    make_kit("slow2", "#!/bin/sh\nsleep 30\n");   /* freeing a run that is going ends it */
    memset(&r, 0, sizeof r);
    CHECK_EQ_INT(rehacer_start(&r, kit_path("slow2"), why, sizeof why), 1);
    rehacer_free(&r);
    CHECK(!r.run.running);
}

int main(void)
{
    const char *tmp = getenv("TMPDIR");
    snprintf(base, sizeof base, "%s/tnfs-rehacer-XXXXXX", tmp != NULL && tmp[0] != '\0' ? tmp : "/tmp");
    if (mkdtemp(base) == NULL) {
        perror("mkdtemp");
        return 1;
    }
    char real[512];
    if (realpath(base, real) != NULL) snprintf(base, sizeof base, "%s", real);
    check_script_present();
    check_stamp();
    check_backup_copies_the_kit();
    check_rotation();
    check_backup_refusals();
    check_run();
    check_long_output_and_stop();
    char cmd[600];
    snprintf(cmd, sizeof cmd, "rm -rf '%s'", base);
    if (system(cmd) != 0) fprintf(stderr, "could not remove %s\n", base);
    return TEST_REPORT();
}
