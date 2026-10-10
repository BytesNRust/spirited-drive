/* Table exchange tests: records exported as columns import back identical; edits land in the right bytes;
 * renamed columns, shifted rows, extra rows and values that do not fit are refused with file and line. */
#include "formats/table_exchange.h"
#include "test.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum { STRIDE = 12, ROWS = 4 };

/* i16 a @0, i16[2] b @2, bytes tag[3] @6, u8 c @9 (@10..11 not in the table) */
static const RecordField fields[] = {
    { 0, REC_I16, 1, "a" },
    { 2, REC_I16, 2, "b" },
    { 6, REC_BYTES, 3, "tag" },
    { 9, REC_U8, 1, "c" }
};
enum { FIELDS = sizeof fields / sizeof fields[0] };

static void fill(uint8_t *r)
{
    for (int i = 0; i < ROWS * STRIDE; ++i) r[i] = (uint8_t)(i * 37 + 5);
}

static int run(const char *text, uint8_t *rec, size_t min, size_t max, size_t *found, ExchangeError *err)
{
    FileSet f;
    fileset_init(&f);
    fileset_add(&f, "t.tsv", (const uint8_t *)text, strlen(text));
    const int ok = table_import(rec, STRIDE, min, max, fields, FIELDS, &f, "t.tsv", found, err);
    fileset_free(&f);
    return ok;
}

int main(void)
{
    uint8_t rec[ROWS * STRIDE], back[ROWS * STRIDE];
    fill(rec);
    FileSet files;
    fileset_init(&files);
    ExchangeError err = {{0}};
    CHECK(table_export(rec, STRIDE, ROWS, fields, FIELDS, "demo", &files, "t.tsv", &err));
    const SetFile *t = fileset_find(&files, "t.tsv");
    CHECK(t != NULL && strstr((const char *)t->data, "row\ta\tb[0]\tb[1]\ttag\tc") != NULL);

    size_t found = 0;
    memcpy(back, rec, sizeof rec);
    memset(back, 0xee, 10);                                    /* wipe what the table covers in row 0 */
    back[10] = rec[10];
    CHECK(table_import(back, STRIDE, ROWS, ROWS, fields, FIELDS, &files, "t.tsv", &found, &err));
    CHECK_EQ_INT(found, ROWS);
    CHECK_MEM(back, rec, sizeof rec);
    fileset_free(&files);

    /* an edit: row 1, a = -2, c = 200; bytes outside the table (10, 11) stay */
    memcpy(back, rec, sizeof rec);
    const char *edit = "row\ta\tb[0]\tb[1]\ttag\tc\n0\t1\t2\t3\t010203\t4\n1\t-2\t0x10\t3\taabbcc\t200\n";
    CHECK(run(edit, back, 1, ROWS, &found, &err));
    CHECK_EQ_INT(found, 2);
    CHECK_EQ_INT(back[STRIDE + 0], 0xfe);
    CHECK_EQ_INT(back[STRIDE + 1], 0xff);
    CHECK_EQ_INT(back[STRIDE + 2], 0x10);
    CHECK_EQ_INT(back[STRIDE + 6], 0xaa);
    CHECK_EQ_INT(back[STRIDE + 9], 200);
    CHECK_EQ_INT(back[STRIDE + 10], rec[STRIDE + 10]);
    CHECK_MEM(back + 2 * STRIDE, rec + 2 * STRIDE, 2 * STRIDE);   /* rows beyond the file untouched */

    const char *bad[] = {
        "row\ta\tb[0]\tb[2]\ttag\tc\n0\t1\t2\t3\t010203\t4\n",            /* renamed column */
        "row\ta\tb[0]\tb[1]\ttag\tc\n1\t1\t2\t3\t010203\t4\n",            /* row 0 missing */
        "row\ta\tb[0]\tb[1]\ttag\tc\n0\t1\t2\t3\t010203\n",               /* short row */
        "row\ta\tb[0]\tb[1]\ttag\tc\n0\t70000\t2\t3\t010203\t4\n",        /* does not fit i16 */
        "row\ta\tb[0]\tb[1]\ttag\tc\n0\t1\t2\t3\t0102\t4\n",              /* tag too short */
        "row\ta\tb[0]\tb[1]\ttag\tc\n0\t1\t2\t3\t010203\t256\n",          /* does not fit u8 */
        "row\ta\tb[0]\tb[1]\ttag\tc\n0\t1\tx\t3\t010203\t4\n",            /* not a number */
        "",                                                               /* empty */
    };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; ++i) {
        memcpy(back, rec, sizeof rec);
        CHECK(!run(bad[i], back, 1, ROWS, &found, &err));
        CHECK(strstr(err.message, "t.tsv") != NULL);
        if (i > 0) CHECK(strstr(err.message, "t.tsv:") != NULL);      /* with a line */
    }
    /* row-count limits */
    CHECK(!run(edit, back, 3, ROWS, &found, &err));                   /* fewer than min */
    CHECK(!run(edit, back, 0, 1, &found, &err));                      /* more than max */
    return TEST_REPORT();
}
