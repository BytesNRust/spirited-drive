/* dep59: the zip reader a mod is installed from (formats/zip.h). Stored and deflated entries read back whole with their CRC checked; what
 * the reader refuses it refuses before anything is unpacked: not a zip, zip64, more than one disk, too many entries, encryption, another
 * method, a bad name, a file or a total over the limits, a deflated entry that grows too much; and an entry whose bytes are damaged is
 * refused when it is read. */
#include "formats/crc32.h"
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
    CHECK(z.entries[0].is_link);
    zip_close(&z);
    e.host = 0;   /* made on DOS: the high bits are not a mode */
    CHECK_EQ_INT(open_entries(&e, 1, &z), ZIP_OK);
    CHECK(!z.entries[0].is_link);
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

int main(void)
{
    test_crc32();
    test_reads_stored_and_deflated();
    test_refuses_what_is_not_a_zip();
    test_refuses_encryption_methods_and_links();
    test_refuses_names();
    test_limits();
    test_damage_is_refused_when_read();
    return TEST_REPORT();
}
