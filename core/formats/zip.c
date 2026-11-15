/* See zip.h. */
#include "formats/zip.h"

#include "formats/crc32.h"

#include <stdlib.h>
#include <string.h>

#define STBI_NO_STDIO
#include "../../third_party/stb_image.h"   /* the declarations only: the implementation is png.c's */

enum {
    EOCD_SIG = 0x06054b50, EOCD_SIZE = 22, CDH_SIG = 0x02014b50, CDH_SIZE = 46, LFH_SIG = 0x04034b50, LFH_SIZE = 30,
    MAX_COMMENT = 0xffff, FLAG_ENCRYPTED = 1, HOST_UNIX = 3, HOST_OSX = 19, MODE_TYPE = 0170000, MODE_FILE = 0100000, MODE_DIR = 0040000
};

static uint32_t le16(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8; }
static uint32_t le32(const uint8_t *p) { return le16(p) | le16(p + 2) << 16; }

/* the end of central directory record: the last one whose comment ends at the archive's end */
static const uint8_t *find_eocd(const uint8_t *data, size_t size)
{
    if (data == NULL || size < EOCD_SIZE) return NULL;
    const size_t lowest = size - EOCD_SIZE > MAX_COMMENT ? size - EOCD_SIZE - MAX_COMMENT : 0;
    for (size_t at = size - EOCD_SIZE + 1; at-- > lowest;) {
        const uint8_t *p = data + at;
        if (le32(p) == EOCD_SIG && at + EOCD_SIZE + le16(p + 20) == size) return p;
    }
    return NULL;
}

/* a component Windows keeps for a device (CON, PRN, AUX, NUL, COM0-9, LPT0-9), with or without an extension */
static int is_device(const uint8_t *c, size_t len)
{
    size_t n = 0;
    while (n < len && c[n] != '.') n++;
    char b[5] = { 0 };
    if (n < 3 || n > 4) return 0;
    for (size_t i = 0; i < n; i++) b[i] = (char)(c[i] >= 'a' && c[i] <= 'z' ? c[i] - 32 : c[i]);
    if (n == 3) return !memcmp(b, "CON", 3) || !memcmp(b, "PRN", 3) || !memcmp(b, "AUX", 3) || !memcmp(b, "NUL", 3);
    return (!memcmp(b, "COM", 3) || !memcmp(b, "LPT", 3)) && b[3] >= '0' && b[3] <= '9';
}

/* printable ASCII without what Windows refuses in a name; no component a device's, nor ending with '.' or ' ' (Windows drops those, so
 * "A", "A." and "A " would be one file) */
static int name_ok(const uint8_t *name, size_t len)
{
    if (len == 0 || len > ZIP_NAME_MAX) return 0;
    for (size_t i = 0; i < len; i++)
        if (name[i] < 0x20 || name[i] > 0x7e || strchr("\\:*?\"<>|", name[i]) != NULL) return 0;
    for (size_t start = 0; start < len;) {
        size_t end = start;
        while (end < len && name[end] != '/') end++;
        const size_t n = end - start;
        if (n > 0 && (name[end - 1] == '.' || name[end - 1] == ' ' || is_device(name + start, n))) return 0;
        start = end + 1;
    }
    return 1;
}

/* one central directory header at `p` (with `left` bytes after it) into `e`; the bytes it takes, or 0 with *err set */
static size_t read_header(const uint8_t *p, size_t left, ZipEntry *e, ZipResult *err)
{
    *err = ZIP_ERR_TRUNCATED;
    if (left < CDH_SIZE || le32(p) != CDH_SIG) return 0;
    const size_t name_len = le16(p + 28), extra = le16(p + 30), comment = le16(p + 32);
    const size_t total = CDH_SIZE + name_len + extra + comment;
    if (total > left) return 0;
    memset(e, 0, sizeof *e);
    const uint32_t flags = le16(p + 8), host = le16(p + 4) >> 8, mode = le32(p + 38) >> 16;
    e->method = (uint16_t)le16(p + 10), e->crc = le32(p + 16), e->csize = le32(p + 20), e->usize = le32(p + 24);
    e->local_offset = le32(p + 42);
    if (e->csize == 0xffffffffu || e->usize == 0xffffffffu || e->local_offset == 0xffffffffu) return *err = ZIP_ERR_ZIP64, 0;
    if (le16(p + 34) != 0) return *err = ZIP_ERR_MULTIDISK, 0;
    if (flags & FLAG_ENCRYPTED) return *err = ZIP_ERR_ENCRYPTED, 0;
    if (e->method != 0 && e->method != 8) return *err = ZIP_ERR_METHOD, 0;
    if (!name_ok(p + CDH_SIZE, name_len)) return *err = ZIP_ERR_NAME, 0;
    memcpy(e->name, p + CDH_SIZE, name_len);
    e->is_dir = p[CDH_SIZE + name_len - 1] == '/';
    const uint32_t type = mode & MODE_TYPE;   /* only Unix and OS X write a mode there; 0 when they wrote none */
    e->is_special = (host == HOST_UNIX || host == HOST_OSX) && type != 0 && type != MODE_FILE && type != MODE_DIR;
    if (e->usize > ZIP_MAX_FILE) return *err = ZIP_ERR_FILE_TOO_BIG, 0;
    if (e->method == 0 && e->csize != e->usize) return 0;
    if (e->method == 8 && e->usize > 0 && (uint64_t)e->usize > (uint64_t)e->csize * ZIP_MAX_RATIO) return *err = ZIP_ERR_RATIO, 0;
    *err = ZIP_OK;
    return total;
}

typedef struct {
    uint64_t start, end;   /* an entry's local header and bytes */
} Span;

static int span_order(const void *a, const void *b)
{
    const uint64_t x = ((const Span *)a)->start, y = ((const Span *)b)->start;
    return x < y ? -1 : x > y;
}

/* each entry's local header is there, and its header and bytes lie before the central directory, apart from every other entry's: the
 * overlapping bomb (many entries over one run of bytes) is refused */
static ZipResult spans_ok(const uint8_t *data, uint64_t cd_offset, const ZipEntry *e, size_t count)
{
    Span *s = malloc((count > 0 ? count : 1) * sizeof *s);
    if (s == NULL) return ZIP_ERR_MEMORY;
    for (size_t i = 0; i < count; i++) {
        const uint64_t at = e[i].local_offset;
        if (at + LFH_SIZE > cd_offset || le32(data + at) != LFH_SIG) return free(s), ZIP_ERR_TRUNCATED;
        s[i].start = at;
        s[i].end = at + LFH_SIZE + le16(data + at + 26) + le16(data + at + 28) + e[i].csize;
        if (s[i].end > cd_offset) return free(s), ZIP_ERR_OVERLAP;
    }
    qsort(s, count, sizeof *s, span_order);
    for (size_t i = 1; i < count; i++)
        if (s[i].start < s[i - 1].end) return free(s), ZIP_ERR_OVERLAP;
    free(s);
    return ZIP_OK;
}

ZipResult zip_open(const uint8_t *data, size_t size, ZipArchive *out)
{
    memset(out, 0, sizeof *out);
    if (size > ZIP_MAX_SIZE) return ZIP_ERR_TOO_BIG;
    const uint8_t *eocd = find_eocd(data, size);
    if (eocd == NULL) return ZIP_ERR_NOT_ZIP;
    const uint32_t count = le16(eocd + 10), cd_size = le32(eocd + 12), cd_offset = le32(eocd + 16);
    if (count == 0xffff || cd_size == 0xffffffffu || cd_offset == 0xffffffffu) return ZIP_ERR_ZIP64;
    if (le16(eocd + 4) != 0 || le16(eocd + 6) != 0 || le16(eocd + 8) != count) return ZIP_ERR_MULTIDISK;
    if (count > ZIP_MAX_ENTRIES) return ZIP_ERR_TOO_MANY;
    if ((uint64_t)cd_offset + cd_size > (size_t)(eocd - data)) return ZIP_ERR_TRUNCATED;
    ZipEntry *entries = calloc(count > 0 ? count : 1, sizeof *entries);
    if (entries == NULL) return ZIP_ERR_MEMORY;
    const uint8_t *p = data + cd_offset;
    size_t left = cd_size;
    uint64_t total = 0;
    for (uint32_t i = 0; i < count; i++) {
        ZipResult err;
        const size_t used = read_header(p, left, &entries[i], &err);
        if (used == 0) return free(entries), err;
        total += entries[i].usize;
        if (total > ZIP_MAX_TOTAL) return free(entries), ZIP_ERR_TOTAL_TOO_BIG;
        p += used, left -= used;
    }
    const ZipResult spans = spans_ok(data, cd_offset, entries, count);
    if (spans != ZIP_OK) return free(entries), spans;
    out->data = data, out->size = size, out->entries = entries, out->count = count;
    return ZIP_OK;
}

/* where entry `e`'s bytes start, or NULL when its local header or bytes run past the archive */
static const uint8_t *entry_bytes(const ZipArchive *z, const ZipEntry *e)
{
    const uint64_t at = e->local_offset;
    if (at + LFH_SIZE > z->size || le32(z->data + at) != LFH_SIG) return NULL;
    const uint64_t start = at + LFH_SIZE + le16(z->data + at + 26) + le16(z->data + at + 28);
    return start + e->csize <= z->size ? z->data + start : NULL;
}

ZipResult zip_extract(const ZipArchive *z, size_t i, uint8_t **out, size_t *out_size)
{
    *out = NULL, *out_size = 0;
    if (z == NULL || i >= z->count) return ZIP_ERR_TRUNCATED;
    const ZipEntry *e = &z->entries[i];
    const uint8_t *in = entry_bytes(z, e);
    if (in == NULL) return ZIP_ERR_TRUNCATED;
    uint8_t *buf = malloc(e->usize > 0 ? e->usize : 1);
    if (buf == NULL) return ZIP_ERR_MEMORY;
    if (e->method == 0) {
        memcpy(buf, in, e->usize);
    } else if (e->usize > 0) {   /* the sizes are under ZIP_MAX_FILE, so they fit an int */
        const int n = stbi_zlib_decode_noheader_buffer((char *)buf, (int)e->usize, (const char *)in, (int)e->csize);
        if (n != (int)e->usize) return free(buf), ZIP_ERR_INFLATE;
    }
    if (crc32_update(0, buf, e->usize) != e->crc) return free(buf), ZIP_ERR_CRC;
    *out = buf, *out_size = e->usize;
    return ZIP_OK;
}

void zip_close(ZipArchive *z)
{
    if (z == NULL) return;
    free(z->entries);
    memset(z, 0, sizeof *z);
}

const char *zip_result_text(ZipResult r)
{
    switch (r) {
    case ZIP_OK: return "ok";
    case ZIP_ERR_TOO_BIG: return "the zip is larger than 256 MB";
    case ZIP_ERR_NOT_ZIP: return "not a zip file";
    case ZIP_ERR_TRUNCATED: return "the zip is cut short or damaged";
    case ZIP_ERR_ZIP64: return "zip64 archives are not supported";
    case ZIP_ERR_MULTIDISK: return "split (multi-part) zips are not supported";
    case ZIP_ERR_TOO_MANY: return "the zip has more than 4096 entries";
    case ZIP_ERR_ENCRYPTED: return "the zip is encrypted";
    case ZIP_ERR_METHOD: return "an entry is compressed with a method other than deflate";
    case ZIP_ERR_NAME: return "an entry has an unusable name";
    case ZIP_ERR_FILE_TOO_BIG: return "a file is larger than 64 MB unpacked";
    case ZIP_ERR_TOTAL_TOO_BIG: return "the files add up to more than 512 MB unpacked";
    case ZIP_ERR_RATIO: return "an entry would grow too much when unpacked";
    case ZIP_ERR_INFLATE: return "an entry's compressed data is damaged";
    case ZIP_ERR_CRC: return "an entry fails its checksum";
    case ZIP_ERR_OVERLAP: return "entries share their bytes (a crafted zip)";
    case ZIP_ERR_MEMORY: return "out of memory";
    }
    return "unknown zip error";
}
