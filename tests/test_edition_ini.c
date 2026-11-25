/* dep59 step 4: a Restomod edition's file, editions/<id>.ini (formats/edition_ini.h): name, cars, tracks and tweaks, ids between commas
 * in their order; a piece that is not installed is kept (the loader skips it); a name, a list or an id past its rule is refused with
 * the reason; the file writes back as it reads. */
#include "formats/edition_ini.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

static int parse(const char *text, Edition *e, char *why, size_t cap)
{
    return edition_parse((const uint8_t *)text, strlen(text), e, why, cap);
}

static void test_reads_the_four_keys(void)
{
    Edition e;
    char why[160] = "";
    CHECK_EQ_INT(parse("# mine\r\nname=Europa 80\r\ncars=f40, testarossa-koenig ,alpine-a110,rx7\r\ntracks=city,coastal,rusty\r\n"
                       "tweaks=coastal-night-light,nicer-smoke\r\n", &e, why, sizeof why), 1);
    CHECK(strcmp(e.name, "Europa 80") == 0);
    CHECK_EQ_INT(e.cars, 4);
    CHECK(strcmp(e.car[0], "f40") == 0 && strcmp(e.car[1], "testarossa-koenig") == 0 && strcmp(e.car[3], "rx7") == 0);
    CHECK_EQ_INT(e.tracks, 3);
    CHECK(strcmp(e.track[2], "rusty") == 0);
    CHECK_EQ_INT(e.tweaks, 2);
    CHECK(strcmp(e.tweak[0], "coastal-night-light") == 0 && strcmp(e.tweak[1], "nicer-smoke") == 0);
}

static void test_round_trip(void)
{
    Edition e, back;
    char why[160] = "", text[EDITION_MAX_SIZE];
    CHECK_EQ_INT(parse("tweaks=b-light\ncars=supra,my-car,warrior\nname=Mine\ntracks=vegas,city\nlater_key=kept for a later version\n",
                       &e, why, sizeof why), 1);
    const int n = edition_format(&e, text, sizeof text);
    CHECK(n > 0 && (size_t)n == strlen(text));
    CHECK(strcmp(text, "name=Mine\ncars=supra,my-car,warrior\ntracks=vegas,city\ntweaks=b-light\n") == 0);
    CHECK_EQ_INT(edition_parse((const uint8_t *)text, (size_t)n, &back, why, sizeof why), 1);
    CHECK(memcmp(&e, &back, sizeof e) == 0);
    CHECK_EQ_INT(edition_format(&e, text, 10), 0);   /* does not fit */
    CHECK(text[0] == '\0');
    /* no tweaks, no line */
    CHECK_EQ_INT(parse("name=A\ncars=rx7\ntracks=city\ntweaks=\n", &e, why, sizeof why), 1);
    CHECK_EQ_INT(e.tweaks, 0);
    CHECK(edition_format(&e, text, sizeof text) > 0 && strcmp(text, "name=A\ncars=rx7\ntracks=city\n") == 0);
}

/* a piece the player has not installed is an id like any other: the file keeps it, the loader (edition_file.h) skips it */
static void test_keeps_a_missing_piece(void)
{
    Edition e;
    char why[160] = "";
    CHECK_EQ_INT(parse("name=A\ncars=not-installed,rx7\ntracks=some-track\n", &e, why, sizeof why), 1);
    CHECK_EQ_INT(e.cars, 2);
    CHECK(strcmp(e.car[0], "not-installed") == 0);
    CHECK_EQ_INT(e.tracks, 1);
    CHECK(strcmp(e.track[0], "some-track") == 0);
    /* no cars or tracks at all is a file too: whether it can be used is the loader's to say */
    CHECK_EQ_INT(parse("name=Empty\n", &e, why, sizeof why), 1);
    CHECK_EQ_INT(e.cars + e.tracks + e.tweaks, 0);
    /* empty items are nothing */
    CHECK_EQ_INT(parse("name=A\ncars=,rx7,,nsx,\n", &e, why, sizeof why), 1);
    CHECK_EQ_INT(e.cars, 2);
}

static void refused(const char *text, const char *reason)
{
    Edition e;
    char why[160] = "";
    memset(&e, 0x55, sizeof e);
    const int ok = parse(text, &e, why, sizeof why);
    CHECK_EQ_INT(ok, 0);
    if (strstr(why, reason) == NULL) {
        fprintf(stderr, "for %s: wanted \"%s\" in \"%s\"\n", text, reason, why);
        CHECK(0);
    }
    CHECK_EQ_INT(e.cars + e.tracks + e.tweaks, 0);   /* nothing half read */
    CHECK(e.name[0] == '\0');
}

static void test_refuses_with_a_reason(void)
{
    refused("name=Thirteen chrs\ncars=rx7\n", "1 to 12 characters");
    refused("cars=rx7\n", "has no name");
    refused("name=\n", "name");
    refused("name=Caf\xc3\xa9\n", "printable ASCII");
    refused("name=a\ttab\n", "printable ASCII");
    refused("name=A\ncars=rx7,original\n", "original");
    refused("name=A\ntracks=city,../x\n", "tracks: entry 2");
    refused("name=A\ncars=RX7\n", "cars: entry 1");
    refused("name=A\ncars=rx7,nsx,rx7\n", "cars names rx7 twice");
    refused("name=A\nname=B\n", "name is given twice");
    refused("name=A\ncars=rx7\ncars=nsx\n", "cars is given twice");
    refused("name=A\nno equals sign\n", "no '='");
    char many[EDITION_MAX_SIZE];
    strcpy(many, "name=A\ncars=");
    for (int i = 0; i < EDITION_LIST_MAX + 1; i++) snprintf(many + strlen(many), sizeof many - strlen(many), "c%d,", i);
    strcat(many, "\n");
    refused(many, "cars has more than 64");
    strcpy(many, "name=A\ntracks=");
    for (int i = 0; i < EDITION_TRACKS_MAX + 1; i++) snprintf(many + strlen(many), sizeof many - strlen(many), "t%d,", i);
    refused(many, "tracks has more than 32");
    strcpy(many, "name=A\ntweaks=");
    for (int i = 0; i < EDITION_TWEAKS_MAX + 1; i++) snprintf(many + strlen(many), sizeof many - strlen(many), "w%d,", i);
    refused(many, "tweaks has more than 32");
    Edition e;
    char why[160];
    CHECK_EQ_INT(edition_parse((const uint8_t *)"name=A\0", 7, &e, why, sizeof why), 0);   /* not text */
    CHECK_EQ_INT(edition_parse(NULL, 0, &e, why, sizeof why), 0);
    static uint8_t big[EDITION_MAX_SIZE + 1];
    memset(big, '#', sizeof big);
    CHECK_EQ_INT(edition_parse(big, sizeof big, &e, why, sizeof why), 0);
    CHECK(strstr(why, "larger") != NULL);
}

static void test_names_and_ids(void)
{
    char why[160] = "";
    CHECK_EQ_INT(edition_name_ok("Twelve chars", why, sizeof why), 1);
    CHECK_EQ_INT(edition_name_ok("Thirteen char", why, sizeof why), 0);
    CHECK_EQ_INT(edition_name_ok("", why, sizeof why), 0);
    CHECK_EQ_INT(edition_name_ok("customize game", why, sizeof why), 0);   /* Game Mode draws that label from its own art */
    CHECK_EQ_INT(edition_name_ok("!~ 09", why, sizeof why), 1);
    CHECK_EQ_INT(edition_id_ok("europa-80"), 1);
    CHECK_EQ_INT(edition_id_ok("original"), 0);
    CHECK_EQ_INT(edition_id_ok("legacy"), 0);
    CHECK_EQ_INT(edition_id_ok("Europa"), 0);
    CHECK_EQ_INT(edition_id_ok(""), 0);
    CHECK_EQ_INT(edition_id_ok("con"), 0);
}

/* an edition file saved with a UTF-8 byte order mark keeps its first key; UTF-16 is refused with the way out */
static void test_bom_and_utf16(void)
{
    Edition e;
    char why[160] = "";
    CHECK_EQ_INT(parse("\xEF\xBB\xBF" "name=Bom\n", &e, why, sizeof why), 1);
    CHECK_EQ_INT(strcmp(e.name, "Bom"), 0);
    CHECK_EQ_INT(edition_parse((const uint8_t *)"\xFE\xFF\0n\0a", 6, &e, why, sizeof why), 0);
    CHECK(strstr(why, "UTF-16") != NULL);
}

int main(void)
{
    test_reads_the_four_keys();
    test_bom_and_utf16();
    test_round_trip();
    test_keeps_a_missing_piece();
    test_refuses_with_a_reason();
    test_names_and_ids();
    return TEST_REPORT();
}
