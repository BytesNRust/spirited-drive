/* SPDX-License-Identifier: GPL-3.0-or-later */
/* port: a read-only ISO 9660 reader for the first start (the player's .iso of the CD). Plain ISO 9660 level 1/2 with 2048-byte sectors; Joliet and Rock Ridge are
 * ignored. Every read is bounded by the file's size, so a truncated or hostile image gives an error and never a crash or a hang. */
#ifndef TNFS_ISO9660_H
#define TNFS_ISO9660_H

#include <stddef.h>
#include <stdint.h>

typedef struct IsoImage IsoImage;   /* opaque; owns a FILE* */

typedef struct {
    char name[64];   /* as the image has it, without a ";N" version and without a trailing '.' */
    int is_dir;
    uint32_t lba;
    uint32_t size;
} IsoEntry;

/* Opens the image. NULL and a reason in `err` when it is not a CD image ("not a CD image: ..."), is cut short, or cannot be read. */
IsoImage *iso_open(const char *path, char *err, size_t err_cap);
void iso_close(IsoImage *iso);

/* `path` relative, with '/', matched without regard to case; ";N" and a trailing '.' in the image's names are ignored. "" is the root. 1 when found. */
int iso_stat(IsoImage *iso, const char *path, IsoEntry *out);
/* The entries of folder `dir` ("" the root), never "." or "..": up to `cap` of them into `out` (NULL with cap 0 just counts). Returns how many the folder has, or -1. */
int iso_list(IsoImage *iso, const char *dir, IsoEntry *out, int cap);
/* Up to `n` bytes of `file` from `off`: how many were read (0 at its end), or -1. */
int64_t iso_read(IsoImage *iso, const IsoEntry *file, int64_t off, void *buf, size_t n);

#endif
