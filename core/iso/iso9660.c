/* SPDX-License-Identifier: GPL-3.0-or-later */
/* port: see iso9660.h. */
#define _FILE_OFFSET_BITS 64
#include "iso/iso9660.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { SECTOR = 2048, PVD_SECTOR = 16, MAX_DIR_BYTES = 8 * 1024 * 1024, MAX_PATH_PARTS = 32 };

struct IsoImage {
    FILE *f;
    int64_t size;
    uint32_t root_lba, root_size;
};

static void say(char *err, size_t cap, const char *text)
{
    if (err != NULL && cap > 0) snprintf(err, cap, "%s", text);
}

static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

/* bytes [off, off + n) of the image into buf: 1 when all of them were there */
static int read_at(const IsoImage *iso, int64_t off, void *buf, size_t n)
{
    if (off < 0 || (int64_t)n > iso->size - off) return 0;
    if (fseeko(iso->f, (off_t)off, SEEK_SET) != 0) return 0;
    return fread(buf, 1, n, iso->f) == n;
}

/* is [lba, lba + size) inside the image */
static int extent_fits(const IsoImage *iso, uint32_t lba, uint32_t size)
{
    return (int64_t)lba * SECTOR + (int64_t)size <= iso->size;
}

IsoImage *iso_open(const char *path, char *err, size_t err_cap)
{
    FILE *f = path != NULL ? fopen(path, "rb") : NULL;
    if (f == NULL) return say(err, err_cap, "the file could not be opened"), NULL;
    IsoImage *iso = calloc(1, sizeof *iso);
    if (iso == NULL || fseeko(f, 0, SEEK_END) != 0) {
        free(iso);
        fclose(f);
        return say(err, err_cap, "the file could not be read"), NULL;
    }
    iso->f = f;
    iso->size = (int64_t)ftello(f);
    uint8_t pvd[SECTOR];
    if (iso->size < (int64_t)(PVD_SECTOR + 1) * SECTOR || !read_at(iso, (int64_t)PVD_SECTOR * SECTOR, pvd, sizeof pvd) || pvd[0] != 1 || memcmp(pvd + 1, "CD001", 5) != 0 ||
        pvd[6] != 1) {
        iso_close(iso);
        return say(err, err_cap, "not a CD image: it has no ISO 9660 header"), NULL;
    }
    const uint8_t *root = pvd + 156;   /* the root directory's record */
    iso->root_lba = le32(root + 2);
    iso->root_size = le32(root + 10);
    if (root[0] < 34 || iso->root_size == 0 || iso->root_size > MAX_DIR_BYTES) {
        iso_close(iso);
        return say(err, err_cap, "not a CD image: its root folder is damaged"), NULL;
    }
    if (!extent_fits(iso, iso->root_lba, iso->root_size)) {
        iso_close(iso);
        return say(err, err_cap, "not a CD image: the file is cut short (an incomplete download?)"), NULL;
    }
    return iso;
}

void iso_close(IsoImage *iso)
{
    if (iso == NULL) return;
    if (iso->f != NULL) fclose(iso->f);
    free(iso);
}

/* the name a record carries: ";N" dropped, a trailing '.' dropped. 0 for a name that cannot be a file's (empty, too long, a '/' or '\\' in it, "." or "..") */
static int clean_name(const uint8_t *raw, int len, char out[64])
{
    if (len <= 0 || len > 63) return 0;
    int n = 0;
    for (int i = 0; i < len; i++) {
        if (raw[i] == ';') break;
        if (raw[i] < 0x20 || raw[i] >= 0x7f || raw[i] == '/' || raw[i] == '\\') return 0;
        out[n++] = (char)raw[i];
    }
    if (n > 0 && out[n - 1] == '.') n--;
    out[n] = '\0';
    return n > 0 && strcmp(out, ".") != 0 && strcmp(out, "..") != 0;
}

/* the records of the folder at [lba, lba + size) into `out` (up to cap): the count of its entries, -1 for a damaged folder */
static int read_dir(IsoImage *iso, uint32_t lba, uint32_t size, IsoEntry *out, int cap)
{
    if (size == 0 || size > MAX_DIR_BYTES || !extent_fits(iso, lba, size)) return -1;
    uint8_t *buf = malloc(size);
    if (buf == NULL || !read_at(iso, (int64_t)lba * SECTOR, buf, size)) {
        free(buf);
        return -1;
    }
    int count = 0;
    uint32_t pos = 0;
    while (pos < size) {
        const uint8_t len = buf[pos];
        if (len == 0) {   /* the rest of the sector is padding */
            pos = (pos / SECTOR + 1) * SECTOR;
            continue;
        }
        if (len < 34 || pos + len > size || 33 + (uint32_t)buf[pos + 32] > len) {
            free(buf);
            return -1;
        }
        const uint8_t *rec = buf + pos;
        pos += len;
        IsoEntry e = { { 0 }, (rec[25] & 2) != 0, le32(rec + 2), le32(rec + 10) };
        if (!clean_name(rec + 33, rec[32], e.name)) continue;   /* "." and ".." are one byte, 0 and 1: not names */
        if (!e.is_dir && !extent_fits(iso, e.lba, e.size)) {
            free(buf);
            return -1;
        }
        if (out != NULL && count < cap) out[count] = e;
        count++;
    }
    free(buf);
    return count;
}

static int same_name(const char *a, const char *b)
{
    for (; *a != '\0' && *b != '\0'; a++, b++)
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
    return *a == *b;
}

/* resolves `path` to its entry; the root is a made-up entry with the root's extent */
static int resolve(IsoImage *iso, const char *path, IsoEntry *out)
{
    IsoEntry here = { { 0 }, 1, iso->root_lba, iso->root_size };
    char part[64];
    const char *p = path != NULL ? path : "";
    int parts = 0;
    while (*p != '\0') {
        while (*p == '/') p++;
        if (*p == '\0') break;
        size_t n = 0;
        while (p[n] != '\0' && p[n] != '/') n++;
        if (n >= sizeof part || ++parts > MAX_PATH_PARTS || !here.is_dir) return 0;
        memcpy(part, p, n), part[n] = '\0';
        p += n;
        const int total = read_dir(iso, here.lba, here.size, NULL, 0);
        if (total <= 0) return 0;
        IsoEntry *list = malloc((size_t)total * sizeof *list);
        if (list == NULL) return 0;
        int found = 0;
        if (read_dir(iso, here.lba, here.size, list, total) == total) {
            for (int i = 0; i < total && !found; i++)
                if (same_name(list[i].name, part)) here = list[i], found = 1;
        }
        free(list);
        if (!found) return 0;
    }
    if (out != NULL) *out = here;
    return 1;
}

int iso_stat(IsoImage *iso, const char *path, IsoEntry *out)
{
    return iso != NULL && resolve(iso, path, out);
}

int iso_list(IsoImage *iso, const char *dir, IsoEntry *out, int cap)
{
    IsoEntry d;
    if (iso == NULL || !resolve(iso, dir, &d) || !d.is_dir) return -1;
    return read_dir(iso, d.lba, d.size, out, cap);
}

int64_t iso_read(IsoImage *iso, const IsoEntry *file, int64_t off, void *buf, size_t n)
{
    if (iso == NULL || file == NULL || file->is_dir || off < 0) return -1;
    if (off >= (int64_t)file->size) return 0;
    if ((int64_t)n > (int64_t)file->size - off) n = (size_t)((int64_t)file->size - off);
    if (!extent_fits(iso, file->lba, file->size) || !read_at(iso, (int64_t)file->lba * SECTOR + off, buf, n)) return -1;
    return (int64_t)n;
}
