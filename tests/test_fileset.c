/* File set tests: names are relative and safe, duplicates refused, data copied. */
#include "formats/exchange.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

int main(void)
{
    CHECK(fileset_name_ok("a.png"));
    CHECK(fileset_name_ok("sub/dir/a.tsv"));
    CHECK(fileset_name_ok("..a/b"));
    CHECK(fileset_name_ok("a..b"));
    CHECK(!fileset_name_ok(""));
    CHECK(!fileset_name_ok(NULL));
    CHECK(!fileset_name_ok("/etc/passwd"));
    CHECK(!fileset_name_ok("../x"));
    CHECK(!fileset_name_ok("a/../x"));
    CHECK(!fileset_name_ok("a/.."));
    CHECK(!fileset_name_ok("a\\b"));
    CHECK(!fileset_name_ok("a//b"));
    CHECK(!fileset_name_ok("dir/"));

    FileSet set;
    fileset_init(&set);
    uint8_t bytes[3] = { 1, 2, 3 };
    CHECK(fileset_add(&set, "one", bytes, 3));
    bytes[0] = 9;
    CHECK(!fileset_add(&set, "one", bytes, 3));        /* taken */
    CHECK(!fileset_add(&set, "../two", bytes, 3));     /* unsafe */
    CHECK(fileset_add(&set, "empty", NULL, 0));
    for (int i = 0; i < 40; ++i) {
        char name[16];
        snprintf(name, sizeof name, "n%d", i);
        CHECK(fileset_add(&set, name, bytes, 1));
    }
    const SetFile *f = fileset_find(&set, "one");
    CHECK(f != NULL && f->size == 3 && f->data[0] == 1);   /* a copy, taken before the edit */
    CHECK(fileset_find(&set, "empty") != NULL);
    CHECK(fileset_find(&set, "missing") == NULL);
    CHECK_EQ_INT(set.count, 42);
    uint8_t *own = malloc(2);
    CHECK(own != NULL && fileset_add_owned(&set, "own", own, 2));
    fileset_free(&set);

    FileSet moved, copied;
    fileset_init(&moved);
    fileset_init(&copied);
    FileSet two;
    fileset_init(&two);
    CHECK(fileset_add(&two, "a.tsv", bytes, 2) && fileset_add(&two, "b/c.bin", bytes, 1));
    CHECK(fileset_move_prefixed(&moved, &two, "c1/"));
    CHECK_EQ_INT(two.count, 0);
    CHECK(fileset_find(&moved, "c1/a.tsv") != NULL && fileset_find(&moved, "c1/b/c.bin") != NULL);
    CHECK(fileset_add(&moved, "other.txt", bytes, 1));
    CHECK(fileset_copy_stripped(&copied, &moved, "c1/"));
    CHECK_EQ_INT(copied.count, 2);
    CHECK(fileset_find(&copied, "a.tsv") != NULL && fileset_find(&copied, "b/c.bin") != NULL);
    CHECK(fileset_find(&copied, "other.txt") == NULL);
    FileSet clash;
    fileset_init(&clash);
    CHECK(fileset_add(&clash, "a.tsv", bytes, 1) && fileset_add(&clash, "x/a.tsv", bytes, 1));
    CHECK(fileset_move_prefixed(&moved, &clash, "zz/"));
    CHECK(fileset_find(&moved, "zz/x/a.tsv") != NULL);
    fileset_free(&moved);
    fileset_free(&copied);

    ExchangeError e;
    exchange_fail(&e, "%s:%u: bad", "x.tsv", 7u);
    CHECK(strcmp(e.message, "x.tsv:7: bad") == 0);
    exchange_fail(NULL, "ignored");
    return TEST_REPORT();
}
