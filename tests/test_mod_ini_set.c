/* dep59 car editor 1c: mod_ini_set changes one line of a card and keeps every other byte; an empty value removes the line; paints= is built
 * and read back by the parser's own rules. */
#include "formats/mod_ini.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* the owner's 911 kit card (art/tnfs-cars/911-carrera-84/coche/mod.ini): text only */
static const char KIT[] =
    "id=carrera-84\nname=911 Carrera 3.2\nversion=0.2\ntype=car\nstem=C84\nshort_name=911 3.2\nyear=1984\npower=231 hp\nweight=1160 kg\n"
    "paints=Guards Red:c8102e,Grand Prix White:eeece2,Black:141414,Zermatt Silver:b8bcbe,Baltic Blue:1c3f7a\n"
    "sound_from=911\ndash_from=911\nauthor=Edgar\n"
    "description=The 1984 911 Carrera 3.2 coupe: air-cooled flat six and five gears\n";

/* the paints' fields (the parser leaves what follows a name's NUL as it finds it, so a byte compare would see stack) */
static int same_paints(const ModPaint *a, const ModPaint *b, int count)
{
    for (int i = 0; i < count; i++)
        if (strcmp(a[i].name, b[i].name) != 0 || memcmp(a[i].rgb, b[i].rgb, 3) != 0) return 0;
    return 1;
}

static char out[MOD_INI_MAX_SIZE + 64], why[160];
static size_t out_len;
static int dup;

static int set(const char *text, const char *key, const char *value)
{
    dup = -1;
    out_len = 0;
    return mod_ini_set(text, strlen(text), key, value, out, sizeof out, &out_len, &dup, why, sizeof why);
}

static int result_is(const char *expected)
{
    return out_len == strlen(expected) && memcmp(out, expected, out_len) == 0 && out[out_len] == '\0';
}

static void test_a_line_is_changed_in_place(void)
{
    CHECK_EQ_INT(set("# top\nid=a\n\nname = Old name \nversion=1\n# end\n", "name", "New"), 1);
    CHECK(result_is("# top\nid=a\n\nname = New\nversion=1\n# end\n"));   /* its spacing round '=' stays; nothing else moves */
    CHECK_EQ_INT(dup, 0);
    CHECK_EQ_INT(set("id=a\nname=B\nversion=1\n", "version", "2"), 1);
    CHECK(result_is("id=a\nname=B\nversion=2\n"));
    CHECK_EQ_INT(set("  name\t=  x\n", "name", "y z"), 1);   /* trimmed keys match as the parser trims them */
    CHECK(result_is("  name\t=  y z\n"));
    CHECK_EQ_INT(set("name=x", "name", "y"), 1);   /* the last line with no line end */
    CHECK(result_is("name=y"));
}

static void test_crlf_is_kept(void)
{
    CHECK_EQ_INT(set("id=a\r\nname=B\r\n# c\r\nversion=1\r\n", "name", "C"), 1);
    CHECK(result_is("id=a\r\nname=C\r\n# c\r\nversion=1\r\n"));
    CHECK_EQ_INT(set("id=a\r\nname=B\r\n", "author", "Me"), 1);   /* an appended line follows the card's line end */
    CHECK(result_is("id=a\r\nname=B\r\nauthor=Me\r\n"));
    CHECK_EQ_INT(set("id=a\nname=B\n", "author", "Me"), 1);
    CHECK(result_is("id=a\nname=B\nauthor=Me\n"));
    CHECK_EQ_INT(set("id=a\r\nname=B", "author", "Me"), 1);   /* no line end at the end: one is added first */
    CHECK(result_is("id=a\r\nname=B\r\nauthor=Me\r\n"));
    CHECK_EQ_INT(set("id=a\nname=B", "author", "Me"), 1);
    CHECK(result_is("id=a\nname=B\nauthor=Me\n"));
    CHECK_EQ_INT(set("", "id", "a"), 1);
    CHECK(result_is("id=a\n"));
}

static void test_an_empty_value_removes_the_line(void)
{
    CHECK_EQ_INT(set("id=a\ncategory=B\nname=N\n", "category", ""), 1);
    CHECK(result_is("id=a\nname=N\n"));
    CHECK_EQ_INT(set("id=a\r\ncategory=B\r\nname=N\r\n", "category", NULL), 1);   /* NULL is the same */
    CHECK(result_is("id=a\r\nname=N\r\n"));
    CHECK_EQ_INT(set("id=a\ncategory=B", "category", ""), 1);   /* the last line, no line end: the one before keeps its own */
    CHECK(result_is("id=a\n"));
    CHECK_EQ_INT(set("id=a\nname=N\n", "category", ""), 1);   /* a key that is not there: the card as it was */
    CHECK(result_is("id=a\nname=N\n"));
    CHECK_EQ_INT(set("category=B\n", "category", ""), 1);
    CHECK_EQ_INT((int)out_len, 0);
    CHECK_EQ_INT(set("# category=B\nid=a\n", "category", ""), 1);   /* a comment is not the key */
    CHECK(result_is("# category=B\nid=a\n"));
}

static void test_a_missing_key_is_appended(void)
{
    CHECK_EQ_INT(set("id=a\n# category=B\nname=N\n\n", "category", "B"), 1);   /* a comment is not the key: appended after the blank line */
    CHECK(result_is("id=a\n# category=B\nname=N\n\ncategory=B\n"));
    CHECK_EQ_INT(set("id=a\nfuture_key=1\n", "category", "C"), 1);   /* a key the game does not know stays */
    CHECK(result_is("id=a\nfuture_key=1\ncategory=C\n"));
}

static void test_the_first_duplicate_is_changed_and_reported(void)
{
    CHECK_EQ_INT(set("name=a\nid=x\nname=b\n", "name", "c"), 1);
    CHECK(result_is("name=c\nid=x\nname=b\n"));
    CHECK_EQ_INT(dup, 1);
    CHECK_EQ_INT(set("name=a\nid=x\nname=b\n", "name", ""), 1);   /* removing takes the first too */
    CHECK(result_is("id=x\nname=b\n"));
    CHECK_EQ_INT(dup, 1);
    CHECK_EQ_INT(set("id=x\n # name=b\nname=a\n", "name", "c"), 1);
    CHECK_EQ_INT(dup, 0);
    CHECK_EQ_INT(set("id=x\n", "name", "c"), 1);
    CHECK_EQ_INT(dup, 0);
    CHECK_EQ_INT(mod_ini_set("id=a\n", 5, "name", "c", out, sizeof out, &out_len, NULL, why, sizeof why), 1);   /* duplicate may be NULL */
}

static void test_refusals_leave_out_alone(void)
{
    static const char *const BAD_VALUE[] = { "a\nb=c", "a\rb", "x\n", "\r", " lead", "trail ", "tab\t" };
    for (size_t i = 0; i < sizeof BAD_VALUE / sizeof BAD_VALUE[0]; i++) {
        memset(out, 'Z', 16);
        why[0] = '\0';
        CHECK_EQ_INT(set("id=a\n", "name", BAD_VALUE[i]), 0);
        CHECK(why[0] != '\0' && strlen(why) < sizeof why);
        CHECK(out[0] == 'Z');
        CHECK(strchr(why, '\n') == NULL && strchr(why, '\r') == NULL);
    }
    static const char *const BAD_KEY[] = { "", "na me", "name=", "Name", "#name", "na\nme", "a-b", "a:b", "0123456789012345678901234567890123" };
    for (size_t i = 0; i < sizeof BAD_KEY / sizeof BAD_KEY[0]; i++) CHECK_EQ_INT(set("id=a\n", BAD_KEY[i], "v"), 0);
    CHECK_EQ_INT(set("id=a\n", NULL, "v"), 0);
    CHECK_EQ_INT(mod_ini_set(NULL, 0, "name", "v", out, sizeof out, &out_len, &dup, why, sizeof why), 0);
    CHECK_EQ_INT(mod_ini_set("id=a\n", 5, "name", "vvvvvvvv", out, 14, &out_len, &dup, why, sizeof why), 0);   /* "id=a\nname=vvvvvvvv\n" is 19 bytes */
    CHECK_EQ_INT(mod_ini_set("id=a\n", 5, "name", "vvvvvvvv", out, 19, &out_len, &dup, why, sizeof why), 0);   /* no room for the NUL */
    CHECK_EQ_INT(mod_ini_set("id=a\n", 5, "name", "vvvvvvvv", out, 20, &out_len, &dup, why, sizeof why), 1);
}

static void test_the_size_cap(void)
{
    char *card = malloc(MOD_INI_MAX_SIZE + 2);
    CHECK(card != NULL);
    if (card == NULL) return;
    /* a card of exactly the cap: a line of the right length, then a change that keeps it, then one that grows it by a byte */
    memset(card, 'x', MOD_INI_MAX_SIZE);
    memcpy(card, "description=", 12);
    card[MOD_INI_MAX_SIZE - 1] = '\n';
    card[MOD_INI_MAX_SIZE] = '\0';
    CHECK_EQ_INT(set(card, "description", "short"), 1);   /* it shrinks */
    CHECK_EQ_INT(set(card, "name", "n"), 0);   /* the new line does not fit */
    CHECK(strstr(why, "4096") != NULL);
    card[MOD_INI_MAX_SIZE] = '\n';
    card[MOD_INI_MAX_SIZE + 1] = '\0';
    CHECK_EQ_INT(set(card, "description", "short"), 0);   /* a card over the cap in is refused, as the parser would */
    card[MOD_INI_MAX_SIZE - 1] = '\n';
    card[MOD_INI_MAX_SIZE] = '\0';
    CHECK_EQ_INT(set(card, "description", ""), 1);   /* removing it brings it within */
    CHECK_EQ_INT((int)out_len, 0);
    free(card);
    /* a result of exactly the cap is allowed, one byte more is not */
    char big[MOD_INI_MAX_SIZE];
    memset(big, 'v', sizeof big);
    big[MOD_INI_MAX_SIZE - 1 - 10] = '\0';   /* 4085 'v's */
    CHECK_EQ_INT(set("id=a\n", "name", big), 1);
    CHECK_EQ_INT((int)out_len, 5 + 5 + (MOD_INI_MAX_SIZE - 11) + 1);
    memset(big, 'v', sizeof big);
    big[MOD_INI_MAX_SIZE - 1 - 5] = '\0';
    CHECK_EQ_INT(set("id=a\n", "name", big), 0);   /* 5 + 5 + 4090 + 1 = 4101 > 4096 */
}

static void test_the_kit_card_round_trips(void)
{
    ModInfo m, n;
    char w[160];
    CHECK_EQ_INT(mod_ini_parse((const uint8_t *)KIT, strlen(KIT), &m, w, sizeof w), 1);
    /* setting a key to the value it has changes no byte */
    static const char *const KEYS[] = { "id", "name", "version", "type", "stem", "short_name", "year", "power", "weight", "sound_from", "dash_from",
                                        "author" };
    static const char *const VALUES[] = { "carrera-84", "911 Carrera 3.2", "0.2", "car", "C84", "911 3.2", "1984", "231 hp", "1160 kg", "911", "911",
                                          "Edgar" };
    for (size_t i = 0; i < sizeof KEYS / sizeof KEYS[0]; i++) {
        CHECK_EQ_INT(set(KIT, KEYS[i], VALUES[i]), 1);
        CHECK(result_is(KIT));
    }
    CHECK_EQ_INT(set(KIT, "description", "The 1984 911 Carrera 3.2 coupe: air-cooled flat six and five gears"), 1);
    CHECK(result_is(KIT));
    /* the paints line is read back by format and written back the same */
    ModPaint p[MOD_INI_PAINTS_MAX];
    int count = 0;
    CHECK_EQ_INT(mod_ini_paints_format(m.paints, m.paint_count, w, sizeof w, why, sizeof why), 1);
    const char *line = strstr(KIT, "paints=") + 7;
    CHECK(strncmp(w, line, strlen(w)) == 0 && line[strlen(w)] == '\n');
    CHECK_EQ_INT(set(KIT, "paints", w), 1);
    CHECK(result_is(KIT));
    CHECK_EQ_INT(mod_ini_paints_parse(w, p, &count, why, sizeof why), 1);
    CHECK_EQ_INT(count, m.paint_count);
    CHECK(same_paints(p, m.paints, count));
    /* a real edit: the card still parses and only that line differs */
    CHECK_EQ_INT(set(KIT, "category", "B"), 1);
    CHECK(strncmp(out, KIT, strlen(KIT)) == 0 && strcmp(out + strlen(KIT), "category=B\n") == 0);
    CHECK_EQ_INT(mod_ini_parse((const uint8_t *)out, out_len, &n, w, sizeof w), 1);
    CHECK(strcmp(n.category, "B") == 0 && strcmp(n.name, m.name) == 0);
    static char removed[sizeof out];
    memcpy(removed, out, out_len + 1);
    CHECK_EQ_INT(set(removed, "category", ""), 1);
    CHECK(result_is(KIT));   /* add then remove gives the card back */
    CHECK_EQ_INT(set(KIT, "weight", "1200 kg"), 1);
    CHECK_EQ_INT(mod_ini_parse((const uint8_t *)out, out_len, &n, w, sizeof w), 1);
    CHECK(strcmp(n.weight, "1200 kg") == 0 && strcmp(n.power, m.power) == 0 && n.paint_count == m.paint_count);
}

static void test_paints_format_and_parse(void)
{
    ModPaint in[MOD_INI_PAINTS_MAX], back[MOD_INI_PAINTS_MAX];
    char line[1024];
    int count = 0;
    memset(in, 0, sizeof in);
    snprintf(in[0].name, sizeof in[0].name, "Guards Red");
    in[0].rgb[0] = 0xc8, in[0].rgb[1] = 0x10, in[0].rgb[2] = 0x2e;
    snprintf(in[1].name, sizeof in[1].name, "Black");
    CHECK_EQ_INT(mod_ini_paints_format(in, 2, line, sizeof line, why, sizeof why), 1);
    CHECK(strcmp(line, "Guards Red:c8102e,Black:000000") == 0);
    CHECK_EQ_INT(mod_ini_paints_parse(line, back, &count, why, sizeof why), 1);
    CHECK_EQ_INT(count, 2);
    CHECK(same_paints(in, back, 2));
    CHECK_EQ_INT(mod_ini_paints_parse("  A : FFEEDD , b:00ff00", back, &count, why, sizeof why), 1);   /* the parser's own leniency */
    CHECK(count == 2 && strcmp(back[0].name, "A") == 0 && back[0].rgb[0] == 0xff && back[1].rgb[1] == 0xff);
    CHECK_EQ_INT(mod_ini_paints_parse("A:zz0000", back, &count, why, sizeof why), 0);
    CHECK(strstr(why, "A") != NULL);
    CHECK_EQ_INT(mod_ini_paints_parse("A:000000,a:111111", back, &count, why, sizeof why), 0);   /* names alike in any case */
    CHECK_EQ_INT(mod_ini_paints_parse("", back, &count, why, sizeof why), 0);
    /* sixteen is the most */
    for (int i = 0; i < MOD_INI_PAINTS_MAX; i++) {
        snprintf(in[i].name, sizeof in[i].name, "P%d", i);
        in[i].rgb[0] = (uint8_t)(i * 16), in[i].rgb[1] = (uint8_t)(255 - i), in[i].rgb[2] = (uint8_t)i;
    }
    CHECK_EQ_INT(mod_ini_paints_format(in, MOD_INI_PAINTS_MAX, line, sizeof line, why, sizeof why), 1);
    CHECK_EQ_INT(mod_ini_paints_parse(line, back, &count, why, sizeof why), 1);
    CHECK_EQ_INT(count, MOD_INI_PAINTS_MAX);
    CHECK(same_paints(in, back, MOD_INI_PAINTS_MAX));
    CHECK_EQ_INT(mod_ini_paints_format(in, MOD_INI_PAINTS_MAX + 1, line, sizeof line, why, sizeof why), 0);
    CHECK_EQ_INT(mod_ini_paints_format(in, 0, line, sizeof line, why, sizeof why), 0);
    CHECK_EQ_INT(mod_ini_paints_format(in, -1, line, sizeof line, why, sizeof why), 0);
    CHECK_EQ_INT(mod_ini_paints_format(in, 2, line, 5, why, sizeof why), 0);   /* too small */
    CHECK_EQ_INT(mod_ini_paints_format(in, 2, line, sizeof line, why, sizeof why), 1);
}

static void test_paints_format_refuses_what_the_parser_would(void)
{
    ModPaint p[2];
    char line[256];
    memset(p, 0, sizeof p);
    static const char *const BAD[] = { "", "a:b", "a,b", " a", "a ", "caf\xc3\xa9", "tab\tname", "ctl\x01" };
    for (size_t i = 0; i < sizeof BAD / sizeof BAD[0]; i++) {
        memset(p[0].name, 0, sizeof p[0].name);
        strncpy(p[0].name, BAD[i], sizeof p[0].name - 1);
        why[0] = '\0';
        CHECK_EQ_INT(mod_ini_paints_format(p, 1, line, sizeof line, why, sizeof why), 0);
        CHECK(why[0] != '\0');
    }
    memset(p[0].name, 'n', sizeof p[0].name);   /* no NUL: not a string at all */
    CHECK_EQ_INT(mod_ini_paints_format(p, 1, line, sizeof line, why, sizeof why), 0);
    snprintf(p[0].name, sizeof p[0].name, "Red");
    snprintf(p[1].name, sizeof p[1].name, "RED");
    CHECK_EQ_INT(mod_ini_paints_format(p, 2, line, sizeof line, why, sizeof why), 0);
    snprintf(p[1].name, sizeof p[1].name, "Blue");
    CHECK_EQ_INT(mod_ini_paints_format(p, 2, line, sizeof line, why, sizeof why), 1);
    CHECK_EQ_INT(mod_ini_paints_format(NULL, 1, line, sizeof line, why, sizeof why), 0);
    /* whatever format writes, set accepts, and the parser reads the card back */
    ModInfo m;
    char card[512];
    snprintf(card, sizeof card, "id=a\nname=N\nversion=1\n");
    CHECK_EQ_INT(mod_ini_paints_format(p, 2, line, sizeof line, why, sizeof why), 1);
    CHECK_EQ_INT(set(card, "paints", line), 1);
    CHECK_EQ_INT(mod_ini_parse((const uint8_t *)out, out_len, &m, card, sizeof card), 1);
    CHECK_EQ_INT(m.paint_count, 2);
}

int main(void)
{
    test_a_line_is_changed_in_place();
    test_crlf_is_kept();
    test_an_empty_value_removes_the_line();
    test_a_missing_key_is_appended();
    test_the_first_duplicate_is_changed_and_reported();
    test_refusals_leave_out_alone();
    test_the_size_cap();
    test_the_kit_card_round_trips();
    test_paints_format_and_parse();
    test_paints_format_refuses_what_the_parser_would();
    return TEST_REPORT();
}
