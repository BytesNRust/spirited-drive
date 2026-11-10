#include "game/exe/exe_image.h"

#include "game/sys/data_check.h"
#include "game/sys/sha256.h"

#include <stdio.h>
#include <string.h>

static void fail(char *err, size_t errsz, const char *text)
{
    if (err && errsz) snprintf(err, errsz, "%s", text);
}

static uint32_t rd16(const uint8_t *p) { return (uint32_t)(p[0] | p[1] << 8); }
static uint32_t rd32(const uint8_t *p) { return rd16(p) | rd16(p + 2) << 16; }

int exe_image_open(ExeImage *img, const uint8_t *data, size_t len, char *err, size_t errsz)
{
    if (err && errsz) err[0] = 0;
    if (!img || !data || len < 0x40) { fail(err, errsz, "the file is too short to be an exe"); return -1; }
    if (data[0] != 'M' || data[1] != 'Z') { fail(err, errsz, "not an MZ exe"); return -1; }
    size_t pe = rd32(data + 0x3C);
    if (pe > len || len - pe < 24 + 28 + 4) { fail(err, errsz, "the file is too short: no PE header"); return -1; }
    if (memcmp(data + pe, "PE\0\0", 4) != 0) { fail(err, errsz, "no PE signature"); return -1; }
    uint32_t nsec = rd16(data + pe + 6), opt = rd16(data + pe + 20);
    if (nsec > EXE_MAX_SECTIONS) { fail(err, errsz, "the exe has more sections than the port reads"); return -1; }
    size_t table = pe + 24 + opt;
    if (table > len || len - table < 40 * (size_t)nsec) { fail(err, errsz, "the file is too short for its section table"); return -1; }
    ExeImage r = { data, len, rd32(data + pe + 24 + 28), (int)nsec, { { 0, 0, 0, 0 } } };
    for (uint32_t i = 0; i < nsec; i++) {
        const uint8_t *h = data + table + 40 * (size_t)i;
        uint32_t vsize = rd32(h + 8), vaddr = rd32(h + 12), raw_size = rd32(h + 16), raw_ptr = rd32(h + 20);
        if (raw_ptr && ((size_t)raw_ptr > len || (size_t)raw_size > len - raw_ptr)) {
            fail(err, errsz, "a section's raw data runs past the end of the file");
            return -1;
        }
        r.section[i].va = r.base + vaddr;
        r.section[i].size = vsize > raw_size ? vsize : raw_size;
        r.section[i].raw_ptr = raw_ptr;
        r.section[i].raw_size = raw_size;
    }
    *img = r;
    return 0;
}

static const ExeSection *section_at(const ExeImage *img, uint32_t va)
{
    for (int i = 0; i < img->sections; i++) {
        const ExeSection *s = &img->section[i];
        if (va >= s->va && (uint64_t)va - s->va < s->size) return s;
    }
    return NULL;
}

int exe_va_read(const ExeImage *img, uint32_t va, void *out, size_t n)
{
    const ExeSection *s = section_at(img, va);
    if (!s) return -1;
    uint8_t *o = out;
    memset(o, 0, n);
    if (!s->raw_ptr) return 0;
    size_t off = va - s->va;
    if (off >= s->raw_size) return 0;
    size_t take = s->raw_size - off;
    if (take > n) take = n;
    memcpy(o, img->data + s->raw_ptr + off, take);
    return 0;
}

int exe_va_cstr(const ExeImage *img, uint32_t va, char *out, size_t cap)
{
    if (!cap) return section_at(img, va) ? 0 : -1;
    out[0] = 0;
    const ExeSection *s = section_at(img, va);
    if (!s) return -1;
    size_t i = 0;
    for (; i + 1 < cap; i++) {
        uint8_t c;
        if (exe_va_read(img, va + (uint32_t)i, &c, 1) != 0 || c == 0) break;
        out[i] = (char)c;
    }
    out[i] = 0;
    return 0;
}

int exe_image_verify_against(const uint8_t *data, size_t len, long size, const char *sha256, const char *where,
                             char *err, size_t errsz)
{
    char hex[65] = "";
    Sha256 s;
    sha256_init(&s);
    sha256_update(&s, data, len);
    sha256_hex(&s, hex);
    int same_size = (long)len == size, same_sha = strcmp(hex, sha256) == 0;
    if (same_size && same_sha) return 0;
    if (err && errsz)
        snprintf(err, errsz,
                 "NFS_WIN.EXE in %s is not the PC Special Edition build this port was made for (%s: size %lu, sha256 %s; "
                 "expected size %ld, sha256 %s). The port reads its tables from that exact file; another release or a patched "
                 "exe has them elsewhere. Choose the folder of the PC Special Edition CD.",
                 where ? where : "the data folder", same_size ? "the size is right, the digest is not" : "the size differs",
                 (unsigned long)len, hex, size, sha256);
    return -1;
}

int exe_image_verify(const uint8_t *data, size_t len, const char *where, char *err, size_t errsz)
{
    for (int i = 0; i < DATA_MANIFEST_COUNT; i++)
        if (strcmp(DATA_MANIFEST[i].path, "NFS_WIN.EXE") == 0)
            return exe_image_verify_against(data, len, DATA_MANIFEST[i].size, DATA_MANIFEST[i].sha256, where, err, errsz);
    if (err && errsz) snprintf(err, errsz, "the manifest has no NFS_WIN.EXE");
    return -1;
}
