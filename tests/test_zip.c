/* dep59: the zip reader a mod is installed from (formats/zip.h). Stored and deflated entries read back whole with their CRC checked; what
 * the reader refuses it refuses before anything is unpacked: not a zip, zip64, more than one disk, too many entries, encryption, another
 * method, a bad name, a file or a total over the limits, a deflated entry that grows too much; and an entry whose bytes are damaged is
 * refused when it is read. The writer (zip_write_stored) gives a set's files back as a zip the reader reads, the same bytes each time. */
#include "formats/crc32.h"
#include "formats/exchange.h"
#include "formats/zip.h"
#include "test.h"
#include "zip_build.h"

#include <stdlib.h>
#include <string.h>

/* "hello hello hello hello hello zip\n" eight times (272 bytes), raw deflate (python zlib, wbits -15) */
static const uint8_t DEFLATED[] = { 0xcb, 0x48, 0xcd, 0xc9, 0xc9, 0x57, 0xc8, 0xc0, 0x41, 0x56, 0x65, 0x16, 0x70, 0x65, 0x8c, 0x28, 0x15, 0x00 };
enum { PLAIN_SIZE = 272, PLAIN_CRC = 0x432a3463 };

static uint8_t buf[1 << 16];

static ZipResult open_entries(const ZbEntry *e, int n, ZipArchive *z)
{
    const size_t size = zb_build(e, n, buf, sizeof buf);
    CHECK(size > 0);
    return zip_open(buf, size, z);
}

static void test_reads_stored_and_deflated(void)
{
    ZbEntry e[3] = { zb_text("mod.ini", "id=a\n"), zb_text("SIMDATA/", ""), zb_text("track/CY1.txt", "track CY1\n") };
    e[1].ext_attr = 040755u << 16;
    ZbEntry d = { "sounds/x.txt", DEFLATED, sizeof DEFLATED, PLAIN_SIZE, PLAIN_CRC, 8, 0, 3, 0100644u << 16 };
    e[2] = d;
    ZipArchive z;
    CHECK_EQ_INT(open_entries(e, 3, &z), ZIP_OK);
    CHECK_EQ_INT((int)z.count, 3);
    CHECK(strcmp(z.entries[0].name, "mod.ini") == 0 && !z.entries[0].is_dir);
    CHECK(z.entries[1].is_dir);
    uint8_t *out = NULL;
    size_t n = 0;
    CHECK_EQ_INT(zip_extract(&z, 0, &out, &n), ZIP_OK);
    CHECK(n == 5 && memcmp(out, "id=a\n", 5) == 0);
    free(out);
    CHECK_EQ_INT(zip_extract(&z, 2, &out, &n), ZIP_OK);
    CHECK_EQ_INT((int)n, PLAIN_SIZE);
    CHECK(out != NULL && memcmp(out, "hello hello", 11) == 0 && out[PLAIN_SIZE - 1] == '\n');
    free(out);
    zip_close(&z);
}

static void test_refuses_what_is_not_a_zip(void)
{
    ZipArchive z;
    CHECK_EQ_INT(zip_open((const uint8_t *)"not a zip at all, not even close", 32, &z), ZIP_ERR_NOT_ZIP);
    CHECK_EQ_INT(zip_open(NULL, 0, &z), ZIP_ERR_NOT_ZIP);
    ZbEntry e = zb_text("a.txt", "x");
    size_t size = zb_build(&e, 1, buf, sizeof buf);
    CHECK_EQ_INT(zip_open(buf, size - 1, &z), ZIP_ERR_NOT_ZIP);   /* the end record cut */
    buf[size - 22 + 16] = 0xff;   /* the directory placed past the end */
    CHECK_EQ_INT(zip_open(buf, size, &z), ZIP_ERR_TRUNCATED);
    size = zb_build(&e, 1, buf, sizeof buf);
    buf[size - 22 + 4] = 1;   /* a second disk */
    CHECK_EQ_INT(zip_open(buf, size, &z), ZIP_ERR_MULTIDISK);
    size = zb_build(&e, 1, buf, sizeof buf);
    buf[size - 22 + 16] = buf[size - 22 + 17] = buf[size - 22 + 18] = buf[size - 22 + 19] = 0xff;   /* zip64's mark */
    CHECK_EQ_INT(zip_open(buf, size, &z), ZIP_ERR_ZIP64);
}

static void test_refuses_encryption_methods_and_links(void)
{
    ZipArchive z;
    ZbEntry e = zb_text("a.txt", "x");
    e.flags = 1;
    CHECK_EQ_INT(open_entries(&e, 1, &z), ZIP_ERR_ENCRYPTED);
    e.flags = 0, e.method = 12;   /* bzip2 */
    CHECK_EQ_INT(open_entries(&e, 1, &z), ZIP_ERR_METHOD);
    e.method = 0, e.ext_attr = 0120777u << 16;   /* a symbolic link, made on Unix */
    CHECK_EQ_INT(open_entries(&e, 1, &z), ZIP_OK);
    CHECK(z.entries[0].is_special);
    zip_close(&z);
    e.host = 0;   /* made on DOS: the high bits are not a mode */
    CHECK_EQ_INT(open_entries(&e, 1, &z), ZIP_OK);
    CHECK(!z.entries[0].is_special);
    zip_close(&z);
}

static void test_refuses_names(void)
{
    static const char *const BAD[] = { "", "a\001b", "C:x" };
    ZipArchive z;
    for (size_t i = 0; i < sizeof BAD / sizeof BAD[0]; i++) {
        ZbEntry e = zb_text(BAD[i], "x");
        CHECK_EQ_INT(open_entries(&e, 1, &z), ZIP_ERR_NAME);
    }
    char longer[ZIP_NAME_MAX + 2];
    memset(longer, 'a', sizeof longer - 1);
    longer[sizeof longer - 1] = '\0';
    ZbEntry e = zb_text(longer, "x");
    CHECK_EQ_INT(open_entries(&e, 1, &z), ZIP_ERR_NAME);
}

/* review of 4977a5c5, 4: names a Windows player could not unpack, or that would collide there, are refused; only printable ASCII */
static void test_refuses_names_windows_cannot_hold(void)
{
    static const char *const BAD[] = { "a*b", "a?b", "a\"b", "a<b", "a>b", "a|b", "x.", "x ", "dir./x", "dir /x", "CON", "con.txt",
                                       "SIMDATA/NUL.QFS", "com1", "COM0.x", "LPT9.TXT", "aux/x", "Prn", "caf\xc3\xa9", "a\177b" };
    ZipArchive z;
    for (size_t i = 0; i < sizeof BAD / sizeof BAD[0]; i++) {
        ZbEntry e = zb_text(BAD[i], "x");
        CHECK_EQ_INT(open_entries(&e, 1, &z), ZIP_ERR_NAME);
        if (z.entries != NULL) zip_close(&z);
    }
    static const char *const GOOD[] = { "CONFIG.DAT", "console.txt", "COM10", "BY_R&T", "My Mod/mod.ini", "dir/", ".hidden", "a..b" };
    for (size_t i = 0; i < sizeof GOOD / sizeof GOOD[0]; i++) {
        ZbEntry e = zb_text(GOOD[i], "x");
        CHECK_EQ_INT(open_entries(&e, 1, &z), ZIP_OK);
        zip_close(&z);
    }
}

/* review of 4977a5c5, 5: an entry that is neither a file nor a folder is marked special: a link made on Unix or on OS X (host 19), a
 * device or a pipe; no mode at all is a plain file */
static void test_special_entries(void)
{
    static const struct { uint8_t host; uint32_t mode; int special; } CASES[] = {
        { 3, 0120777u, 1 }, { 19, 0120777u, 1 }, { 3, 0020644u, 1 }, { 3, 0010644u, 1 }, { 19, 0060644u, 1 },
        { 3, 0100644u, 0 }, { 19, 0100644u, 0 }, { 3, 0040755u, 0 }, { 3, 0u, 0 }, { 0, 0120777u, 0 },
    };
    ZipArchive z;
    for (size_t i = 0; i < sizeof CASES / sizeof CASES[0]; i++) {
        ZbEntry e = zb_text("a.txt", "x");
        e.host = CASES[i].host, e.ext_attr = CASES[i].mode << 16;
        CHECK_EQ_INT(open_entries(&e, 1, &z), ZIP_OK);
        CHECK_EQ_INT(z.entries[0].is_special, CASES[i].special);
        zip_close(&z);
    }
}

/* the central directory record of entry `k` in the archive of `size` bytes in buf */
static uint8_t *cd_record(size_t size, int k)
{
    uint8_t *p = buf + (buf[size - 6] | buf[size - 5] << 8 | buf[size - 4] << 16 | (uint32_t)buf[size - 3] << 24);
    for (int i = 0; i < k; i++) p += 46 + (p[28] | p[29] << 8) + (p[30] | p[31] << 8) + (p[32] | p[33] << 8);
    return p;
}

/* review of 4977a5c5, 6: entries whose bytes overlap (the overlapping bomb), or run into the central directory, or past the archive,
 * are refused when the zip is opened */
static void test_refuses_overlapping_entries(void)
{
    ZipArchive z;
    ZbEntry e[2] = { zb_text("a.txt", "hello"), zb_text("b.txt", "world") };
    size_t size = zb_build(e, 2, buf, sizeof buf);
    zb_32(cd_record(size, 1) + 42, 0);   /* b's local header is a's */
    CHECK_EQ_INT(zip_open(buf, size, &z), ZIP_ERR_OVERLAP);
    size = zb_build(e, 2, buf, sizeof buf);
    zb_32(cd_record(size, 1) + 42, 3);   /* inside a's bytes */
    CHECK_EQ_INT(zip_open(buf, size, &z), ZIP_ERR_TRUNCATED);   /* no local header there */
    size = zb_build(e, 2, buf, sizeof buf);
    uint8_t *r = cd_record(size, 0);
    zb_32(r + 20, 5 + 30 + 5 + 3), zb_32(r + 24, 5 + 30 + 5 + 3);   /* a's bytes said to run over b's */
    CHECK_EQ_INT(zip_open(buf, size, &z), ZIP_ERR_OVERLAP);
    size = zb_build(e, 2, buf, sizeof buf);
    r = cd_record(size, 1);
    zb_32(r + 20, 5 + 10), zb_32(r + 24, 5 + 10);   /* b's bytes said to run into the central directory */
    CHECK_EQ_INT(zip_open(buf, size, &z), ZIP_ERR_OVERLAP);
    size = zb_build(e, 2, buf, sizeof buf);
    CHECK_EQ_INT(zip_open(buf, size, &z), ZIP_OK);   /* untouched, it opens */
    zip_close(&z);
}

static void test_limits(void)
{
    ZipArchive z;
    ZbEntry e = zb_text("a.txt", "x");
    e.usize = ZIP_MAX_FILE + 1u, e.method = 8;
    CHECK_EQ_INT(open_entries(&e, 1, &z), ZIP_ERR_FILE_TOO_BIG);
    ZbEntry d = { "a.txt", DEFLATED, sizeof DEFLATED, sizeof DEFLATED * ZIP_MAX_RATIO + 1u, PLAIN_CRC, 8, 0, 3, 0 };
    CHECK_EQ_INT(open_entries(&d, 1, &z), ZIP_ERR_RATIO);   /* grows more than the ratio allows: a bomb */
    ZbEntry s = zb_text("a.txt", "xyz");
    s.usize = 9;   /* stored, yet the sizes differ */
    CHECK_EQ_INT(open_entries(&s, 1, &z), ZIP_ERR_TRUNCATED);
    ZbEntry many[3];
    for (int i = 0; i < 3; i++) many[i] = zb_text(i == 0 ? "a" : i == 1 ? "b" : "c", "x");
    many[0].usize = many[1].usize = many[2].usize = ZIP_MAX_TOTAL / 2u, many[0].method = many[1].method = many[2].method = 8;
    many[0].csize = many[1].csize = many[2].csize = 1;
    CHECK(zip_open(buf, zb_build(many, 3, buf, sizeof buf), &z) != ZIP_OK);   /* each over the file limit, or the total over its own */
    CHECK(zip_open(buf, ZIP_MAX_SIZE + 1u, &z) == ZIP_ERR_TOO_BIG);   /* refused by its size alone, before it is read */
}

static void test_damage_is_refused_when_read(void)
{
    ZipArchive z;
    ZbEntry e = zb_text("a.txt", "hello");
    e.crc = 0x12345678u;
    CHECK_EQ_INT(open_entries(&e, 1, &z), ZIP_OK);
    uint8_t *out = NULL;
    size_t n = 0;
    CHECK_EQ_INT(zip_extract(&z, 0, &out, &n), ZIP_ERR_CRC);
    CHECK(out == NULL);
    zip_close(&z);
    uint8_t bad[sizeof DEFLATED];
    memcpy(bad, DEFLATED, sizeof bad);
    bad[3] ^= 0x5a;
    ZbEntry d = { "a.txt", bad, sizeof bad, PLAIN_SIZE, PLAIN_CRC, 8, 0, 3, 0 };
    CHECK_EQ_INT(open_entries(&d, 1, &z), ZIP_OK);
    const ZipResult r = zip_extract(&z, 0, &out, &n);
    CHECK(r == ZIP_ERR_INFLATE || r == ZIP_ERR_CRC);
    CHECK(out == NULL);
    zip_close(&z);
    CHECK_EQ_INT(open_entries(&e, 1, &z), ZIP_OK);
    z.entries[0].local_offset = 0xfffff;   /* the local header past the end */
    CHECK_EQ_INT(zip_extract(&z, 0, &out, &n), ZIP_ERR_TRUNCATED);
    CHECK_EQ_INT(zip_extract(&z, 5, &out, &n), ZIP_ERR_TRUNCATED);   /* no such entry */
    zip_close(&z);
}

static void test_crc32(void)
{
    CHECK_EQ_INT((int)(crc32_update(0, (const uint8_t *)"123456789", 9) == 0xCBF43926u), 1);   /* the check value */
    CHECK_EQ_INT((int)(crc32_update(crc32_update(0, (const uint8_t *)"1234", 4), (const uint8_t *)"56789", 5) == 0xCBF43926u), 1);
}

/* a set's files written as a zip: the reader opens it, its entries are the set's in order with the same bytes, and the bytes do not
 * change from one writing to the next (no clock in them) */
static void test_writes_stored(void)
{
    FileSet set;
    fileset_init(&set);
    fileset_add(&set, "mod.ini", (const uint8_t *)"id=x\n", 5);
    fileset_add(&set, "empty.txt", (const uint8_t *)"", 0);
    fileset_add(&set, "SIMDATA/CARFAMS/F40.CFM", (const uint8_t *)"hello hello", 11);
    size_t size = 0, again_size = 0;
    uint8_t *zip = zip_write_stored(&set, &size);
    CHECK(zip != NULL);
    ZipArchive z;
    CHECK_EQ_INT(zip != NULL ? zip_open(zip, size, &z) : ZIP_ERR_NOT_ZIP, ZIP_OK);
    if (zip != NULL && z.count == 3) {
        for (size_t i = 0; i < 3; i++) {
            CHECK_EQ_INT(strcmp(z.entries[i].name, set.file[i].name), 0);
            CHECK_EQ_INT(z.entries[i].method, 0);
            CHECK_EQ_INT(z.entries[i].is_special, 0);
            uint8_t *out = NULL;
            size_t n = 0;
            CHECK_EQ_INT(zip_extract(&z, i, &out, &n), ZIP_OK);
            CHECK_EQ_INT((int)n, (int)set.file[i].size);
            CHECK(out != NULL && memcmp(out, set.file[i].data, n) == 0);
            free(out);
        }
    }
    if (zip != NULL) {
        CHECK_EQ_INT((int)z.count, 3);
        zip_close(&z);
    }
    uint8_t *again = zip_write_stored(&set, &again_size);
    CHECK(again != NULL && again_size == size && memcmp(again, zip, size) == 0);
    free(again);
    free(zip);
    fileset_free(&set);
}

/* what the reader would refuse is not written: an empty set, a name past ZIP_NAME_MAX, a name the reader refuses */
static void test_write_refusals(void)
{
    FileSet set;
    size_t size = 1;
    fileset_init(&set);
    CHECK(zip_write_stored(&set, &size) == NULL);
    char name[ZIP_NAME_MAX + 2];
    memset(name, 'a', ZIP_NAME_MAX + 1);
    name[ZIP_NAME_MAX + 1] = 0;
    fileset_add(&set, name, (const uint8_t *)"x", 1);
    CHECK(zip_write_stored(&set, &size) == NULL);
    fileset_free(&set);
    fileset_init(&set);
    name[ZIP_NAME_MAX] = 0;
    fileset_add(&set, name, (const uint8_t *)"x", 1);
    uint8_t *zip = zip_write_stored(&set, &size);
    CHECK(zip != NULL);
    free(zip);
    fileset_free(&set);
    fileset_init(&set);
    fileset_add(&set, "CON.txt", (const uint8_t *)"x", 1);
    CHECK(zip_write_stored(&set, &size) == NULL);
    fileset_free(&set);
}

int main(void)
{
    test_crc32();
    test_reads_stored_and_deflated();
    test_refuses_what_is_not_a_zip();
    test_refuses_encryption_methods_and_links();
    test_refuses_names();
    test_refuses_names_windows_cannot_hold();
    test_special_entries();
    test_refuses_overlapping_entries();
    test_limits();
    test_damage_is_refused_when_read();
    test_writes_stored();
    test_write_refusals();
    return TEST_REPORT();
}
