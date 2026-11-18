/* The text spec of a track's art (formats/fam_spec.h): the lines in order, comments and blanks skipped, the ids checked, a horizon
 * required, and each refusal naming its line. */
#include "formats/fam_spec.h"
#include "test.h"

#include <string.h>

static int parse(const char *text, FamSpec *spec, char *message, size_t cap)
{
    ExchangeError err = { { 0 } };
    const int ok = fam_spec_parse(text, spec, &err);
    snprintf(message, cap, "%s", err.message);
    return ok;
}

static void test_good_spec(void)
{
    static const char text[] = "# a stage\n\nterrain 0 grass.png   # the verge\nterrain 1 sub/asphalt.png\r\nscenery trees.png\nhorizon sky.png\n";
    FamSpec spec;
    char msg[256];
    CHECK(parse(text, &spec, msg, sizeof msg));
    CHECK_EQ_INT(spec.count, 4);
    CHECK_EQ_INT(spec.item[0].kind, FAM_SPEC_TERRAIN);
    CHECK(strcmp(spec.item[0].path, "grass.png") == 0);
    CHECK(strcmp(spec.item[1].path, "sub/asphalt.png") == 0);
    CHECK_EQ_INT(spec.item[2].kind, FAM_SPEC_SCENERY);
    CHECK_EQ_INT(spec.item[3].kind, FAM_SPEC_HORIZON);
    CHECK(strcmp(spec.item[3].path, "sky.png") == 0);
}

static void test_path_with_spaces(void)
{
    static const char text[] = "terrain 0 my road.png\nterrain 1 b.png\nhorizon sky one.png\n";
    FamSpec spec;
    char msg[256];
    CHECK(parse(text, &spec, msg, sizeof msg));
    CHECK(strcmp(spec.item[0].path, "my road.png") == 0);
    CHECK(strcmp(spec.item[2].path, "sky one.png") == 0);
}

static void test_refusals(void)
{
    static const struct { const char *text, *want; } bad[] = {
        { "terrain 1 a.png\nhorizon h.png\n", "line 1: terrain id 1 where 0 comes" },
        { "terrain 0 a.png\nterrain 0 b.png\nhorizon h.png\n", "line 2: terrain id 0 where 1 comes" },
        { "terrain x a.png\nhorizon h.png\n", "line 1: a terrain line" },
        { "terrain 0\nhorizon h.png\n", "line 1: no picture named" },
        { "terrain 0 a.png\nroad b.png\nhorizon h.png\n", "line 2: 'road' is not terrain" },
        { "terrain 0 a.png\nhorizon h.png\nhorizon i.png\n", "line 3: a second horizon" },
        { "terrain 0 a.png\nterrain 1 b.png\n", "the spec names no horizon" },
    };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        FamSpec spec;
        char msg[256];
        CHECK(!parse(bad[i].text, &spec, msg, sizeof msg));
        CHECK(strstr(msg, bad[i].want) != NULL);
        if (strstr(msg, bad[i].want) == NULL) fprintf(stderr, "got: %s\n", msg);
    }
}

static void test_empty_text_has_no_horizon(void)
{
    FamSpec spec;
    char msg[256];
    CHECK(!parse("", &spec, msg, sizeof msg));
    CHECK(strstr(msg, "no horizon") != NULL);
}

static void test_letters(void)
{
    FamSpec spec;
    char msg[256];
    CHECK(parse("terrain 0 letters=AC rock.png\nterrain 1 letters=B  grass 2.png\nterrain 2 plain.png\nhorizon sky.png\n", &spec, msg, sizeof msg));
    CHECK_EQ_INT(spec.item[0].letters, FAM_LETTER_A | FAM_LETTER_C);
    CHECK(strcmp(spec.item[0].path, "rock.png") == 0);
    CHECK_EQ_INT(spec.item[1].letters, FAM_LETTER_B);
    CHECK(strcmp(spec.item[1].path, "grass 2.png") == 0);
    CHECK_EQ_INT(spec.item[2].letters, FAM_LETTERS_ALL);   /* nothing said: all three */
    CHECK_EQ_INT(spec.item[3].letters, FAM_LETTERS_ALL);
    CHECK(!parse("terrain 0 letters=AD x.png\nhorizon s.png\n", &spec, msg, sizeof msg));
    CHECK(strstr(msg, "line 1") != NULL && strstr(msg, "'D'") != NULL);
    CHECK(!parse("terrain 0 letters= x.png\nhorizon s.png\n", &spec, msg, sizeof msg));
    CHECK(strstr(msg, "no letter") != NULL);
    CHECK(!parse("terrain 0 letters=A\nhorizon s.png\n", &spec, msg, sizeof msg));   /* the path is still needed */
    CHECK(strstr(msg, "no picture") != NULL);
}

int main(void)
{
    test_letters();
    test_good_spec();
    test_path_with_spaces();
    test_refusals();
    test_empty_text_has_no_horizon();
    return TEST_REPORT();
}
