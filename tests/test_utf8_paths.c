/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Windows: the program's C library takes UTF-8 paths (game/sys/utf8.manifest, linked into this exe through the same spirited.rc): a folder and files with letters outside the ANSI
 * code page are made, written, read, listed, renamed and removed by the calls the game uses (_mkdir, fopen, stat, rename, remove, opendir), and the name on the disk is the real one (the wide API finds it). Not built elsewhere. */
#ifdef _WIN32
#include <dirent.h>
#include <direct.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <windows.h>

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL: %s (line %d)\n", #c, __LINE__); failures++; } } while (0)

/* "Zoë Ñandú 日本" */
#define DIR_NAME "utf8 Zo" "\xc3\xab" " " "\xc3\x91" "and" "\xc3\xba" " " "\xe6\x97\xa5\xe6\x9c\xac"
#define FILE_A DIR_NAME "/file " "\xc3\xb1" ".txt"
#define NAME_B "file2 " "\xc3\xbc" ".txt"
#define FILE_B DIR_NAME "/" NAME_B

static int on_disk(const char *utf8)
{
    wchar_t wide[512];
    return MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide, 512) > 0 && GetFileAttributesW(wide) != INVALID_FILE_ATTRIBUTES;
}

int main(void)
{
    CHECK(GetACP() == CP_UTF8);   /* the manifest is in this exe */
    _rmdir(DIR_NAME);
    CHECK(_mkdir(DIR_NAME) == 0);
    CHECK(on_disk(DIR_NAME));
    struct stat st;
    CHECK(stat(DIR_NAME, &st) == 0 && (st.st_mode & S_IFDIR) != 0);
    FILE *f = fopen(FILE_A, "wb");
    CHECK(f != NULL);
    if (f != NULL) { fputs("round trip", f); fclose(f); }
    CHECK(on_disk(FILE_A));
    char text[32] = { 0 };
    f = fopen(FILE_A, "rb");
    CHECK(f != NULL);
    if (f != NULL) { CHECK(fread(text, 1, sizeof text - 1, f) == 10); fclose(f); }
    CHECK(strcmp(text, "round trip") == 0);
    CHECK(rename(FILE_A, FILE_B) == 0);
    CHECK(stat(FILE_A, &st) != 0 && stat(FILE_B, &st) == 0 && st.st_size == 10);
    int found = 0;
    DIR *d = opendir(DIR_NAME);
    CHECK(d != NULL);
    for (struct dirent *e = d != NULL ? readdir(d) : NULL; e != NULL; e = readdir(d)) found += strcmp(e->d_name, NAME_B) == 0;
    if (d != NULL) closedir(d);
    CHECK(found == 1);
    CHECK(remove(FILE_B) == 0);
    CHECK(_rmdir(DIR_NAME) == 0);
    CHECK(!on_disk(DIR_NAME));
    printf("%d failures\n", failures);
    return failures != 0;
}
#else
int main(void) { return 0; }
#endif
