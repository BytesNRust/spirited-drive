/* port: the original's NFS_WIN.EXE as the port reads it (nothing of it is kept in the repository): a PE image held in a byte
 * buffer, read by virtual address, and the check that the buffer is the PC SE's own file (size and SHA-256 of the manifest,
 * data_manifest.c). The tables the port takes from the exe (exe_tables.h) are read through it at start-up.
 *
 * Reading follows the loader's view of the sections the exe has: an address inside a section's memory is read from its raw
 * data, the bytes past the raw data (and all of .bss, which has none in the file) read as zero. */
#ifndef TNFS_EXE_IMAGE_H
#define TNFS_EXE_IMAGE_H

#include <stddef.h>
#include <stdint.h>

enum { EXE_MAX_SECTIONS = 8 };

typedef struct {
    uint32_t va;         /* the first address, with the image base */
    uint32_t size;       /* bytes in memory: the larger of the virtual and the raw size */
    uint32_t raw_ptr;    /* where its raw data starts in the file (0: none, as .bss) */
    uint32_t raw_size;
} ExeSection;

typedef struct {
    const uint8_t *data;   /* the file, not owned */
    size_t len;
    uint32_t base;         /* the image base */
    int sections;
    ExeSection section[EXE_MAX_SECTIONS];
} ExeImage;

/* Parses the PE headers of `data`. Returns 0, or -1 with `err` (when given) saying why: too short, not an MZ file, no PE
 * signature, more sections than EXE_MAX_SECTIONS, a section's raw data past the end of the file. The buffer must outlive the image. */
int exe_image_open(ExeImage *img, const uint8_t *data, size_t len, char *err, size_t errsz);

/* Reads `n` bytes at the address `va` into `out`. Returns 0, or -1 when `va` is in no section (`out` is then left alone). */
int exe_va_read(const ExeImage *img, uint32_t va, void *out, size_t n);

/* The NUL terminated string at `va`, cut to `cap` - 1 bytes. Returns 0, or -1 when `va` is in no section (`out` is then "" ). */
int exe_va_cstr(const ExeImage *img, uint32_t va, char *out, size_t cap);

/* Is the buffer the PC SE's NFS_WIN.EXE (the size and the SHA-256 of the manifest)? Returns 0, or -1 with a message that names
 * `where` (the folder it was read from) and both sizes and digests. */
int exe_image_verify(const uint8_t *data, size_t len, const char *where, char *err, size_t errsz);
/* The same against a given size and digest (64 lower-case hex digits); for the tests. */
int exe_image_verify_against(const uint8_t *data, size_t len, long size, const char *sha256, const char *where,
                             char *err, size_t errsz);

#endif
