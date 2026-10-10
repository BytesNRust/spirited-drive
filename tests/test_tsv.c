/* TSV exchange tests: what the writer emits, the reader parses back (including
 * escapes, hex bytes and 32-bit values) and malformed cells report their line. */
#include "formats/tsv.h"
#include "test.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void test_round_trip(void)
{
    TsvWriter w;
    tsv_writer_init(&w);
    tsv_comment(&w, "field\tvalue");
    tsv_str(&w, "name");
    tsv_int(&w, -5);
    tsv_uint(&w, 0xfffffff0u);
    tsv_hex(&w, (const uint8_t[]){ 0x00, 0xab, 0xff }, 3);
    tsv_row(&w);
    tsv_str(&w, "tab\there\\and\nnewline");
    tsv_hex(&w, NULL, 0);
    tsv_row(&w);
    CHECK(!w.failed);
    CHECK(w.len > 0);

    TsvReader r;
    tsv_reader_init(&r, w.data, w.len);
    TsvRow row;
    CHECK_EQ_INT(tsv_next(&r, &row), TSV_OK);
    CHECK_EQ_INT(row.cells, 4);
    CHECK_EQ_INT(row.line, 2);                       /* the comment is line 1 */
    CHECK(strcmp(row.cell[0], "name") == 0);
    int32_t i = 0;
    uint32_t u = 0;
    uint8_t bytes[8];
    size_t n = 0;
    CHECK_EQ_INT(tsv_get_int(&row, 1, &i), TSV_OK);
    CHECK_EQ_INT(i, -5);
    CHECK_EQ_INT(tsv_get_uint(&row, 2, &u), TSV_OK);
    CHECK(u == 0xfffffff0u);
    CHECK_EQ_INT(tsv_get_hex(&row, 3, bytes, sizeof bytes, &n), TSV_OK);
    CHECK_EQ_INT(n, 3);
    CHECK_MEM(bytes, "\x00\xab\xff", 3);
    CHECK_EQ_INT(tsv_next(&r, &row), TSV_OK);
    CHECK(strcmp(row.cell[0], "tab\there\\and\nnewline") == 0);
    CHECK_EQ_INT(tsv_get_hex(&row, 1, bytes, sizeof bytes, &n), TSV_OK);
    CHECK_EQ_INT(n, 0);
    CHECK_EQ_INT(tsv_next(&r, &row), TSV_END);
    tsv_writer_free(&w);
}

static void test_reader_leniency(void)
{
    const char text[] = "# comment\r\n\r\na\tb\r\n\n  \nc\t\td\n";
    TsvReader r;
    TsvRow row;
    tsv_reader_init(&r, (const uint8_t *)text, sizeof text - 1);
    CHECK_EQ_INT(tsv_next(&r, &row), TSV_OK);
    CHECK_EQ_INT(row.line, 3);
    CHECK_EQ_INT(row.cells, 2);
    CHECK(strcmp(row.cell[1], "b") == 0);              /* CRLF is accepted */
    CHECK_EQ_INT(tsv_next(&r, &row), TSV_OK);
    CHECK_EQ_INT(row.cells, 3);
    CHECK(strcmp(row.cell[1], "") == 0);               /* an empty cell stays a cell */
    CHECK_EQ_INT(tsv_next(&r, &row), TSV_END);
}

static void test_bad_cells(void)
{
    const char text[] = "x\t12z\t0x\t-\t4294967296\tab c\tabc\n";
    TsvReader r;
    TsvRow row;
    int32_t i;
    uint32_t u;
    uint8_t b[4];
    size_t n;
    tsv_reader_init(&r, (const uint8_t *)text, sizeof text - 1);
    CHECK_EQ_INT(tsv_next(&r, &row), TSV_OK);
    CHECK_EQ_INT(tsv_get_int(&row, 1, &i), TSV_ERR_NUMBER);
    CHECK_EQ_INT(tsv_get_int(&row, 3, &i), TSV_ERR_NUMBER);
    CHECK_EQ_INT(tsv_get_uint(&row, 4, &u), TSV_ERR_NUMBER);       /* over 32 bits */
    CHECK_EQ_INT(tsv_get_int(&row, 9, &i), TSV_ERR_MISSING);
    CHECK_EQ_INT(tsv_get_hex(&row, 5, b, sizeof b, &n), TSV_ERR_HEX);   /* a space */
    CHECK_EQ_INT(tsv_get_hex(&row, 6, b, sizeof b, &n), TSV_ERR_HEX);   /* odd length */
    CHECK_EQ_INT(tsv_get_hex(&row, 2, b, sizeof b, &n), TSV_ERR_HEX);
    const char big[] = "x\t0102030405\n";
    tsv_reader_init(&r, (const uint8_t *)big, sizeof big - 1);
    CHECK_EQ_INT(tsv_next(&r, &row), TSV_OK);
    CHECK_EQ_INT(tsv_get_hex(&row, 1, b, sizeof b, &n), TSV_ERR_CAPACITY);
    /* hex and decimal both accepted for integers */
    const char nums[] = "0x10\t16\t-0x10\n";
    tsv_reader_init(&r, (const uint8_t *)nums, sizeof nums - 1);
    CHECK_EQ_INT(tsv_next(&r, &row), TSV_OK);
    tsv_get_int(&row, 0, &i); CHECK_EQ_INT(i, 16);
    tsv_get_int(&row, 1, &i); CHECK_EQ_INT(i, 16);
    tsv_get_int(&row, 2, &i); CHECK_EQ_INT(i, -16);
}

int main(void)
{
    test_round_trip();
    test_reader_leniency();
    test_bad_cells();
    return TEST_REPORT();
}
