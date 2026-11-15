/* dep59: a mod's card, mod.ini (formats/mod_ini.h): key=value lines, '#' comments, id / name / version required, each value within its
 * length, nothing that is not text; an id the folder can be named after (a-z, 0-9, '-'), "legacy" kept for the loose files moved there. */
#include "formats/mod_ini.h"
#include "test.h"

#include <string.h>

static int parse(const char *text, ModInfo *m, char *why)
{
    return mod_ini_parse((const uint8_t *)text, strlen(text), m, why, 160);
}

static void test_a_full_card(void)
{
    ModInfo m;
    char why[160];
    const char *text = "# a comment\r\nid=coastal-night\r\nname = Coastal Night Light \nversion=1.2\nauthor=Someone\n\n"
                       "description=Darker nights on the coast.\nfuture_key=ignored\n";
    CHECK_EQ_INT(parse(text, &m, why), 1);
    CHECK(strcmp(m.id, "coastal-night") == 0);
    CHECK(strcmp(m.name, "Coastal Night Light") == 0);   /* trimmed */
    CHECK(strcmp(m.version, "1.2") == 0);
    CHECK(strcmp(m.author, "Someone") == 0);
    CHECK(strcmp(m.description, "Darker nights on the coast.") == 0);
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1", &m, why), 1);   /* no last line end, no author or description */
    CHECK(m.author[0] == '\0' && m.description[0] == '\0');
}

static void test_refusals(void)
{
    static const char *const BAD[] = {
        "name=A\nversion=1\n",                                  /* no id */
        "id=a\nversion=1\n",                                    /* no name */
        "id=a\nname=A\n",                                       /* no version */
        "id=A\nname=A\nversion=1\n",                            /* capitals in the id */
        "id=a b\nname=A\nversion=1\n",
        "id=-a\nname=A\nversion=1\n",                           /* a leading dash */
        "id=../x\nname=A\nversion=1\n",
        "id=legacy\nname=A\nversion=1\n",                       /* kept for the loose files */
        "id=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\nname=A\nversion=1\n",   /* 33 */
        "id=a\nid=b\nname=A\nversion=1\n",                      /* twice */
        "id=a\nname=A\nversion=1\nno equals sign\n",
        "id=a\nname=\nversion=1\n",                             /* empty */
        "id=a\nname=A\tB\nversion=1\n",                         /* a control character */
    };
    ModInfo m;
    char why[160];
    for (size_t i = 0; i < sizeof BAD / sizeof BAD[0]; i++) {
        why[0] = '\0';
        CHECK_EQ_INT(parse(BAD[i], &m, why), 0);
        CHECK(why[0] != '\0');   /* every refusal says why */
    }
    const uint8_t nul[] = "id=a\0\nname=A\nversion=1\n";
    CHECK_EQ_INT(mod_ini_parse(nul, sizeof nul - 1, &m, why, sizeof why), 0);
    char longer[600] = "id=a\nname=A\nversion=1\ndescription=";
    memset(longer + strlen(longer), 'x', MOD_INI_DESCRIPTION_MAX + 1);
    CHECK_EQ_INT(parse(longer, &m, why), 0);
    static char big[MOD_INI_MAX_SIZE + 2];
    memset(big, '#', sizeof big - 1);
    CHECK_EQ_INT(mod_ini_parse((const uint8_t *)big, sizeof big - 1, &m, why, sizeof why), 0);
}

static void test_utf8_text_is_kept(void)
{
    ModInfo m;
    char why[160];
    CHECK_EQ_INT(parse("id=a\nname=Noche en la costa — Ñ\nversion=1\n", &m, why), 1);
    CHECK(strcmp(m.name, "Noche en la costa — Ñ") == 0);
}

int main(void)
{
    test_a_full_card();
    test_refusals();
    test_utf8_text_is_kept();
    return TEST_REPORT();
}
