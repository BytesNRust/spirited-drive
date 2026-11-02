/* SPDX-License-Identifier: GPL-3.0-or-later */
/* A test-only writer of synthetic ISO 9660 images (2048-byte sectors, upper-case names with ";1"): the tests never use a real CD's files. */
#ifndef TNFS_TEST_ISO_BUILD_H
#define TNFS_TEST_ISO_BUILD_H

#include <stddef.h>

typedef struct {
    const char *path;   /* with '/', any case; a folder is made for each part before the last */
    const void *data;
    size_t size;
} IsoBuildFile;

enum { ISO_BUILD_ROOT_LBA = 20 };   /* the root folder's sector; the other folders follow, then the files */

/* Writes the image. 1 on success. A folder's records must fit one sector. */
int iso_build(const char *out_path, const IsoBuildFile *files, int count);

#endif
