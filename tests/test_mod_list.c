/* dep59 n4 phase 2: the list of active mods, `mods=` in the mode's file (formats/mod_list.h): ids between commas in their order (the first
 * wins), spaces round them dropped; an id a folder could not be named after, a repeat or one past the limit is left out and counted; the
 * list writes back as it reads. */
#include "formats/mod_list.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

static void test_reads_in_order(void)
{
    ModList l;
    CHECK_EQ_INT(mod_list_parse("coastal-night, night-two ,b2", &l), 0);
    CHECK_EQ_INT(l.count, 3);
    CHECK(strcmp(l.ids[0], "coastal-night") == 0 && strcmp(l.ids[1], "night-two") == 0 && strcmp(l.ids[2], "b2") == 0);
    CHECK_EQ_INT(mod_list_parse("", &l), 0);
    CHECK_EQ_INT(l.count, 0);
    CHECK_EQ_INT(mod_list_parse(NULL, &l), 0);
    CHECK_EQ_INT(l.count, 0);
    CHECK_EQ_INT(mod_list_parse(",,a,,", &l), 0);   /* empty items are nothing */
    CHECK_EQ_INT(l.count, 1);
}

static void test_leaves_out_what_it_cannot_take(void)
{
    ModList l;
    CHECK_EQ_INT(mod_list_parse("a,Bad,../x,a,-b,c d,b", &l), 5);   /* capitals, a path, a repeat, a leading dash, a space inside */
    CHECK_EQ_INT(l.count, 2);
    CHECK(strcmp(l.ids[0], "a") == 0 && strcmp(l.ids[1], "b") == 0);
    char many[MOD_LIST_MAX * 8 + 64] = "";
    for (int i = 0; i < MOD_LIST_MAX + 3; i++) {
        char id[8];
        snprintf(id, sizeof id, "m%d,", i);
        strcat(many, id);
    }
    CHECK_EQ_INT(mod_list_parse(many, &l), 3);
    CHECK_EQ_INT(l.count, MOD_LIST_MAX);
    char longer[64];
    memset(longer, 'a', 40);
    longer[40] = '\0';
    CHECK_EQ_INT(mod_list_parse(longer, &l), 1);   /* over the id's length */
    CHECK_EQ_INT(l.count, 0);
}

static void test_writes_back(void)
{
    ModList l;
    char text[MOD_LIST_TEXT_MAX];
    mod_list_parse(" b , c-2,a ", &l);
    CHECK_EQ_INT(mod_list_format(&l, text, sizeof text), 1);
    CHECK(strcmp(text, "b,c-2,a") == 0);
    l.count = 0;
    CHECK_EQ_INT(mod_list_format(&l, text, sizeof text), 1);
    CHECK(strcmp(text, "") == 0);
    mod_list_parse("abc,def", &l);
    CHECK_EQ_INT(mod_list_format(&l, text, 5), 0);   /* does not fit */
    CHECK(strcmp(text, "") == 0);
    CHECK_EQ_INT(mod_list_index(&l, "def"), 1);
    CHECK_EQ_INT(mod_list_index(&l, "x"), -1);
}

int main(void)
{
    test_reads_in_order();
    test_leaves_out_what_it_cannot_take();
    test_writes_back();
    return TEST_REPORT();
}
