/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The ISO 9660 reader (core/iso/iso9660.c) on synthetic images built by the test: names, folders, sectors, and what is not an image. */
#define _FILE_OFFSET_BITS 64
#include "iso/iso9660.h"
#include "iso_build.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char dir[] = "/tmp/tnfs_iso_XXXXXX";

static void path_in(const char *name, char *out, size_t cap) { snprintf(out, cap, "%s/%s", dir, name); }

static uint8_t *pattern(size_t n, uint8_t seed)
{
    uint8_t *p = malloc(n);
    for (size_t i = 0; i < n; i++) p[i] = (uint8_t)(i * 31 + seed);
    return p;
}

static void write_file(const char *path, const void *data, size_t n)
{
    FILE *f = fopen(path, "wb");
    CHECK(f != NULL);
    if (f == NULL) return;
    if (n > 0) fwrite(data, 1, n, f);
    fclose(f);
}

static void iso_reads_root_files(void)
{
    char img[600], err[128];
    path_in("root.iso", img, sizeof img);
    uint8_t *data = pattern(3000, 7);
    const IsoBuildFile files[] = { { "NFS_WIN.EXE", data, 3000 }, { "README.TXT", "hello", 5 } };
    CHECK(iso_build(img, files, 2));
    IsoImage *iso = iso_open(img, err, sizeof err);
    CHECK(iso != NULL);
    if (iso == NULL) return;
    IsoEntry e;
    CHECK_EQ_INT(iso_stat(iso, "nfs_win.exe", &e), 1);
    CHECK_EQ_INT((int)e.size, 3000);
    CHECK_EQ_INT(e.is_dir, 0);
    uint8_t back[3000];
    CHECK_EQ_INT((int)iso_read(iso, &e, 0, back, sizeof back), 3000);
    CHECK(memcmp(back, data, 3000) == 0);
    CHECK_EQ_INT((int)iso_read(iso, &e, 3000, back, 10), 0);      /* at its end */
    CHECK_EQ_INT((int)iso_read(iso, &e, 2990, back, 100), 10);    /* clipped to the file */
    CHECK_EQ_INT(iso_stat(iso, "missing.bin", &e), 0);
    CHECK_EQ_INT(iso_list(iso, "", NULL, 0), 2);
    iso_close(iso);
    free(data);
}

static void iso_reads_nested_dirs(void)
{
    char img[600], err[128];
    path_in("nested.iso", img, sizeof img);
    const IsoBuildFile files[] = { { "SIMDATA/MISC/TR1.TRI", "tri", 3 }, { "SIMDATA/GAME.DAT", "dat", 3 } };
    CHECK(iso_build(img, files, 2));
    IsoImage *iso = iso_open(img, err, sizeof err);
    CHECK(iso != NULL);
    if (iso == NULL) return;
    IsoEntry list[8];
    CHECK_EQ_INT(iso_list(iso, "SIMDATA", list, 8), 2);
    int misc = 0;
    for (int i = 0; i < 2; i++)
        if (strcmp(list[i].name, "MISC") == 0 && list[i].is_dir) misc = 1;
    CHECK(misc);
    IsoEntry e;
    CHECK_EQ_INT(iso_stat(iso, "simdata/misc/tr1.tri", &e), 1);
    CHECK_EQ_INT(iso_stat(iso, "SIMDATA/MISC/TR1.TRI/more", &e), 0);   /* a file is not a folder */
    CHECK_EQ_INT(iso_list(iso, "SIMDATA/GAME.DAT", list, 8), -1);
    iso_close(iso);
}

static void iso_strips_version_suffix(void)
{
    char img[600], err[128];
    path_in("version.iso", img, sizeof img);
    const IsoBuildFile files[] = { { "README.", "r", 1 }, { "NFS_WIN.EXE", "x", 1 } };   /* written README.;1 and NFS_WIN.EXE;1 */
    CHECK(iso_build(img, files, 2));
    IsoImage *iso = iso_open(img, err, sizeof err);
    CHECK(iso != NULL);
    if (iso == NULL) return;
    IsoEntry e;
    CHECK_EQ_INT(iso_stat(iso, "README", &e), 1);
    CHECK_EQ_INT(iso_stat(iso, "readme", &e), 1);
    CHECK(strcmp(e.name, "README") == 0);
    CHECK_EQ_INT(iso_stat(iso, "NFS_WIN.EXE", &e), 1);
    CHECK(strcmp(e.name, "NFS_WIN.EXE") == 0);   /* no ";1" in the name a copy would use */
    iso_close(iso);
}

static void iso_reads_across_sectors(void)
{
    char img[600], err[128];
    path_in("sectors.iso", img, sizeof img);
    uint8_t *data = pattern(5000, 3);
    const IsoBuildFile files[] = { { "BIG.BIN", data, 5000 } };
    CHECK(iso_build(img, files, 1));
    IsoImage *iso = iso_open(img, err, sizeof err);
    CHECK(iso != NULL);
    if (iso == NULL) return;
    IsoEntry e;
    CHECK_EQ_INT(iso_stat(iso, "BIG.BIN", &e), 1);
    uint8_t back[100];
    CHECK_EQ_INT((int)iso_read(iso, &e, 2040, back, sizeof back), 100);
    CHECK(memcmp(back, data + 2040, 100) == 0);
    iso_close(iso);
    free(data);
}

static void iso_rejects_garbage(void)
{
    char img[600], err[128];
    uint8_t *junk = malloc(40 * 1024);
    memset(junk, 0x50, 40 * 1024);
    path_in("junk.iso", img, sizeof img);
    write_file(img, junk, 40 * 1024);
    CHECK(iso_open(img, err, sizeof err) == NULL);
    CHECK(strstr(err, "not a CD image") != NULL);
    uint8_t *big = malloc(100 * 1024);   /* long enough to have a sector 16, still not an image */
    memset(big, 0x50, 100 * 1024);
    write_file(img, big, 100 * 1024);
    CHECK(iso_open(img, err, sizeof err) == NULL);
    CHECK(strstr(err, "not a CD image") != NULL);
    write_file(img, "", 0);
    CHECK(iso_open(img, err, sizeof err) == NULL);
    CHECK(strstr(err, "not a CD image") != NULL);
    char gone[600];
    path_in("absent.iso", gone, sizeof gone);
    CHECK(iso_open(gone, err, sizeof err) == NULL);
    CHECK(err[0] != '\0');
    free(junk);
    free(big);
}

static void iso_rejects_truncated(void)
{
    char img[600], cut[600], err[128];
    path_in("whole.iso", img, sizeof img);
    path_in("cut.iso", cut, sizeof cut);
    uint8_t *data = pattern(9000, 1);
    const IsoBuildFile files[] = { { "A.BIN", data, 9000 } };
    CHECK(iso_build(img, files, 1));
    FILE *f = fopen(img, "rb");
    CHECK(f != NULL);
    uint8_t *all = malloc(40 * 2048);
    const size_t n = f != NULL ? fread(all, 1, 40 * 2048, f) : 0;
    if (f != NULL) fclose(f);
    CHECK(n > 18 * 2048);
    write_file(cut, all, 18 * 2048);   /* the header and no folders */
    CHECK(iso_open(cut, err, sizeof err) == NULL);
    CHECK(strstr(err, "not a CD image") != NULL);
    write_file(cut, all, 21 * 2048 + 100);   /* the folders are there, the file's end is not */
    IsoImage *iso = iso_open(cut, err, sizeof err);
    if (iso != NULL) {
        IsoEntry e;
        CHECK_EQ_INT(iso_list(iso, "", NULL, 0), -1);   /* its record reaches past the end: the folder is damaged */
        CHECK_EQ_INT(iso_stat(iso, "A.BIN", &e), 0);
        iso_close(iso);
    }
    free(all);
    free(data);
}

/* a folder whose record points at its parent: listing it never reports "." or ".." and a path through it is bounded */
static void iso_rejects_dir_loop(void)
{
    char img[600], err[128];
    path_in("loop.iso", img, sizeof img);
    const IsoBuildFile files[] = { { "SIMDATA/A.BIN", "a", 1 } };
    CHECK(iso_build(img, files, 1));
    FILE *f = fopen(img, "r+b");
    CHECK(f != NULL);
    if (f == NULL) return;
    uint8_t root[2048];
    fseek(f, ISO_BUILD_ROOT_LBA * 2048L, SEEK_SET);
    CHECK(fread(root, 1, sizeof root, f) == sizeof root);
    size_t pos = 68;   /* after "." and ".." */
    CHECK(root[pos + 33] == 'S');   /* SIMDATA's record: its extent now says the root */
    root[pos + 2] = ISO_BUILD_ROOT_LBA, root[pos + 3] = root[pos + 4] = root[pos + 5] = 0;
    root[pos + 6] = root[pos + 7] = root[pos + 8] = 0, root[pos + 9] = ISO_BUILD_ROOT_LBA;
    fseek(f, ISO_BUILD_ROOT_LBA * 2048L, SEEK_SET);
    fwrite(root, 1, sizeof root, f);
    fclose(f);
    IsoImage *iso = iso_open(img, err, sizeof err);
    CHECK(iso != NULL);
    if (iso == NULL) return;
    IsoEntry list[8];
    const int n = iso_list(iso, "SIMDATA", list, 8);
    CHECK_EQ_INT(n, 1);   /* it lists the root, which has SIMDATA only */
    for (int i = 0; i < n; i++) CHECK(strcmp(list[i].name, ".") != 0 && strcmp(list[i].name, "..") != 0);
    IsoEntry e;
    char deep[600] = "SIMDATA";
    for (int i = 0; i < 60; i++) strcat(deep, "/SIMDATA");
    CHECK_EQ_INT(iso_stat(iso, deep, &e), 0);   /* the depth is bounded: no endless walk */
    iso_close(iso);
}

/* a damaged record (a length that runs out of its folder) is an error, not a read past the buffer */
static void iso_rejects_damaged_records(void)
{
    char img[600], err[128];
    path_in("damaged.iso", img, sizeof img);
    const IsoBuildFile files[] = { { "A.BIN", "a", 1 } };
    CHECK(iso_build(img, files, 1));
    FILE *f = fopen(img, "r+b");
    CHECK(f != NULL);
    if (f == NULL) return;
    fseek(f, ISO_BUILD_ROOT_LBA * 2048L + 68, SEEK_SET);
    const uint8_t big = 250;   /* A.BIN's record claims 250 bytes: a folder of one sector has room, but the name length no longer fits */
    fwrite(&big, 1, 1, f);
    fseek(f, ISO_BUILD_ROOT_LBA * 2048L + 68 + 32, SEEK_SET);
    const uint8_t name_len = 255;
    fwrite(&name_len, 1, 1, f);
    fclose(f);
    IsoImage *iso = iso_open(img, err, sizeof err);
    CHECK(iso != NULL);
    if (iso == NULL) return;
    CHECK_EQ_INT(iso_list(iso, "", NULL, 0), -1);
    iso_close(iso);
}

int main(void)
{
    CHECK(mkdtemp(dir) != NULL);
    iso_reads_root_files();
    iso_reads_nested_dirs();
    iso_strips_version_suffix();
    iso_reads_across_sectors();
    iso_rejects_garbage();
    iso_rejects_truncated();
    iso_rejects_dir_loop();
    iso_rejects_damaged_records();
    char cmd[300];
    snprintf(cmd, sizeof cmd, "find %s -delete", dir);
    CHECK_EQ_INT(system(cmd), 0);
    return TEST_REPORT();
}
