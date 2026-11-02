/* port (the first-run wizard): is a folder the CD's files of The Need for Speed SE (PC)? A few key files (the exe, tracks, cars, art, music) are hashed and compared
 * with the manifest generated from the CD (data_manifest.c, tools/gen_data_manifest.py). The folder may hold the files directly, or in one folder under it (a CD
 * copied into a "NFS" folder); a file's name is matched without regard to case. */
#ifndef TNFS_DATA_CHECK_H
#define TNFS_DATA_CHECK_H

#include <stddef.h>

typedef struct {
    const char *path;     /* relative, with / */
    long size;
    const char *sha256;   /* 64 lower-case hex digits */
} DataFile;

extern const DataFile DATA_MANIFEST[];
extern const int DATA_MANIFEST_COUNT;

typedef struct {
    int total, matched, missing, different;   /* of the manifest's files */
    char root[1024];                          /* the folder that holds the files (the given one or the one under it), "" when none does */
} DataCheck;

/* Checks `dir` against `files` (`count` of them). Returns 1 when every file is there and equal, else 0 (the counts say how it fell short). */
int data_check_against(const char *dir, const DataFile *files, int count, DataCheck *out);
/* The same against the manifest of the PC SE. */
int data_check(const char *dir, DataCheck *out);

/* Cheap: whether `dir` has the manifest's first file at its size (the exe), what a copy that was verified leaves; no hashing. */
int data_looks_installed(const char *dir);

/* Writes <dir>/.accepted (what the check found): the folder was copied by Use it anyway, so data_looks_installed takes it on the next start. 1 on success. */
int data_mark_accepted(const char *dir, const DataCheck *c);

#endif
