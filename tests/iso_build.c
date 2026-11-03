/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "iso_build.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { SECTOR = 2048, MAX_DIRS = 32 };

typedef struct {
    char path[160];   /* upper case, no leading or trailing '/'; "" the root */
    uint32_t lba;
} Dir;

static void both32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8), p[2] = (uint8_t)(v >> 16), p[3] = (uint8_t)(v >> 24);
    p[4] = (uint8_t)(v >> 24), p[5] = (uint8_t)(v >> 16), p[6] = (uint8_t)(v >> 8), p[7] = (uint8_t)v;
}

static void upper(char *s)
{
    for (; *s != '\0'; s++) *s = (char)toupper((unsigned char)*s);
}

static int dir_index(const Dir *dirs, int n, const char *path)
{
    for (int i = 0; i < n; i++)
        if (strcmp(dirs[i].path, path) == 0) return i;
    return -1;
}

/* a record at buf+pos; returns its length */
static size_t put_record(uint8_t *buf, size_t pos, const char *name, int name_len, uint32_t lba, uint32_t size, int is_dir)
{
    size_t len = 33 + (size_t)name_len;
    if (len % 2) len++;
    uint8_t *r = buf + pos;
    r[0] = (uint8_t)len;
    both32(r + 2, lba);
    both32(r + 10, size);
    r[25] = is_dir ? 2 : 0;
    r[28] = 1, r[31] = 1;   /* volume sequence number, both-endian */
    r[32] = (uint8_t)name_len;
    memcpy(r + 33, name, (size_t)name_len);
    return len;
}

/* the parent of "A/B" is "A", of "A" the root */
static void parent_of(const char *path, char *out)
{
    const char *slash = strrchr(path, '/');
    size_t n = slash != NULL ? (size_t)(slash - path) : 0;
    memcpy(out, path, n);
    out[n] = '\0';
}

int iso_build(const char *out_path, const IsoBuildFile *files, int count)
{
    Dir dirs[MAX_DIRS];
    int ndirs = 1;
    memset(dirs, 0, sizeof dirs);
    char upath[count > 0 ? count : 1][160];
    for (int i = 0; i < count; i++) {
        snprintf(upath[i], sizeof upath[i], "%s", files[i].path);
        upper(upath[i]);
        for (char *s = upath[i]; (s = strchr(s, '/')) != NULL; s++) {   /* a folder for each part before the last */
            char prefix[160];
            memcpy(prefix, upath[i], (size_t)(s - upath[i]));
            prefix[s - upath[i]] = '\0';
            if (dir_index(dirs, ndirs, prefix) < 0) {
                if (ndirs >= MAX_DIRS) return 0;
                snprintf(dirs[ndirs++].path, sizeof dirs[0].path, "%s", prefix);
            }
        }
    }
    uint32_t next = ISO_BUILD_ROOT_LBA + (uint32_t)ndirs;
    for (int i = 0; i < ndirs; i++) dirs[i].lba = ISO_BUILD_ROOT_LBA + (uint32_t)i;
    uint32_t file_lba[count > 0 ? count : 1];
    for (int i = 0; i < count; i++) {
        file_lba[i] = next;
        next += files[i].size == 0 ? 1 : (uint32_t)((files[i].size + SECTOR - 1) / SECTOR);
    }
    const size_t total = (size_t)next * SECTOR;
    uint8_t *img = calloc(1, total);
    if (img == NULL) return 0;
    uint8_t *pvd = img + 16 * SECTOR;
    pvd[0] = 1, memcpy(pvd + 1, "CD001", 5), pvd[6] = 1;
    memset(pvd + 40, ' ', 32), memcpy(pvd + 40, "TESTDISC", 8);
    both32(pvd + 80, next);   /* the volume's size (only its low half is read by anyone) */
    pvd[128] = 0, pvd[129] = 8, pvd[130] = 8, pvd[131] = 0;   /* 2048, both-endian */
    put_record(pvd, 156, "\0", 1, dirs[0].lba, SECTOR, 1);
    uint8_t *term = img + 17 * SECTOR;
    term[0] = 255, memcpy(term + 1, "CD001", 5), term[6] = 1;
    for (int d = 0; d < ndirs; d++) {
        uint8_t *sector = img + (size_t)dirs[d].lba * SECTOR;
        char parent[160];
        parent_of(dirs[d].path, parent);
        size_t pos = 0;
        pos += put_record(sector, pos, "\0", 1, dirs[d].lba, SECTOR, 1);
        pos += put_record(sector, pos, "\1", 1, dirs[dir_index(dirs, ndirs, parent) < 0 ? 0 : dir_index(dirs, ndirs, parent)].lba, SECTOR, 1);
        for (int k = 1; k < ndirs; k++) {   /* the sub-folders */
            char p[160];
            parent_of(dirs[k].path, p);
            if (strcmp(p, dirs[d].path) != 0) continue;
            const char *leaf = strrchr(dirs[k].path, '/') != NULL ? strrchr(dirs[k].path, '/') + 1 : dirs[k].path;
            if (pos + 40 + strlen(leaf) > SECTOR) goto too_big;
            pos += put_record(sector, pos, leaf, (int)strlen(leaf), dirs[k].lba, SECTOR, 1);
        }
        for (int i = 0; i < count; i++) {   /* the files, upper-case with ";1" */
            char p[160];
            parent_of(upath[i], p);
            if (strcmp(p, dirs[d].path) != 0) continue;
            const char *leaf = strrchr(upath[i], '/') != NULL ? strrchr(upath[i], '/') + 1 : upath[i];
            char name[80];
            const int n = snprintf(name, sizeof name, "%s;1", leaf);
            if (pos + 40 + (size_t)n > SECTOR) goto too_big;
            pos += put_record(sector, pos, name, n, file_lba[i], (uint32_t)files[i].size, 0);
        }
    }
    for (int i = 0; i < count; i++)
        if (files[i].size > 0) memcpy(img + (size_t)file_lba[i] * SECTOR, files[i].data, files[i].size);
    FILE *f = fopen(out_path, "wb");
    const int ok = f != NULL && fwrite(img, 1, total, f) == total;
    if (f != NULL) fclose(f);
    free(img);
    return ok && f != NULL;
too_big:
    free(img);
    return 0;
}
