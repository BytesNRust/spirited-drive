/* dep59: a mod's card, mod.ini (formats/mod_ini.h): key=value lines, '#' comments, id / name / version required, each value within its
 * length, nothing that is not text; an id the folder can be named after (a-z, 0-9, '-'), "legacy" kept for the loose files moved there, no name Windows keeps for a
 * device (con, nul, com0-9, ...). */
#include "formats/mod_ini.h"
#include "test.h"

#include <stdio.h>
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
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1", &m, why), 1);
    CHECK_EQ_INT(parse("id=console\nname=A\nversion=1\n", &m, why), 1);   /* a device's name only as the whole id */
    CHECK_EQ_INT(parse("id=com10\nname=A\nversion=1\n", &m, why), 1);   /* no last line end, no author or description */
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
        "id=con\nname=A\nversion=1\n",                          /* Windows keeps these names for devices */
        "id=nul\nname=A\nversion=1\n",
        "id=com0\nname=A\nversion=1\n",
        "id=lpt9\nname=A\nversion=1\n",
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
    CHECK_EQ_INT(parse("id=a\nname=Noche\nversion=1\ndescription=Noche en la costa — Ñ\n", &m, why), 1);
    CHECK(strcmp(m.description, "Noche en la costa — Ñ") == 0);
}

/* dep59 step 3: the fields the game draws raw in its big face (GRAVER18: name, short_name, paint names) hold printable ASCII alone;
 * the refusal names the field and where the first byte that is not is */
static void test_drawn_names_are_ascii(void)
{
    ModInfo m;
    char why[160];
    CHECK_EQ_INT(parse("id=a\nname=Peugeot 205\nversion=1\nshort_name=205 GTI\npaints=Rojo:ff0000\n", &m, why), 1);
    CHECK(strcmp(m.name, "Peugeot 205") == 0 && strcmp(m.short_name, "205 GTI") == 0);
    CHECK_EQ_INT(parse("id=a\nname=Citro\xc3\xabn\nversion=1\n", &m, why), 0);
    CHECK(strstr(why, "name") != NULL && strstr(why, "6") != NULL);
    CHECK_EQ_INT(parse("id=a\nname=A\tB\nversion=1\n", &m, why), 0);
    CHECK(strstr(why, "name") != NULL && strstr(why, "2") != NULL);
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1\nshort_name=Citro\xc3\xabn\n", &m, why), 0);
    CHECK(strstr(why, "short_name") != NULL && strstr(why, "6") != NULL);
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1\npaints=Azul:0000ff,Rojo \xc3\xa9:ff0000\n", &m, why), 0);
    CHECK(strstr(why, "paints") != NULL && strstr(why, "Rojo") == NULL && strstr(why, "6") != NULL);
    CHECK_EQ_INT(parse("id=a\nname=A~ !\nversion=1\n", &m, why), 1);   /* the ends of the range */
}

/* review of 4977a5c5, 8: a value must be UTF-8 with nothing that reorders or hides what the player reads (bidi controls, C1 controls) */
static void test_values_are_clean_utf8(void)
{
    static const char *const BAD[] = {
        "id=a\nname=A\nauthor=\xff\nversion=1\n",                   /* not UTF-8 */
        "id=a\nname=A\nauthor=a\xc3\nversion=1\n",                  /* cut short */
        "id=a\nname=A\nauthor=\xc0\xaf\nversion=1\n",               /* overlong */
        "id=a\nname=A\nauthor=\xe0\x80\xaf\nversion=1\n",
        "id=a\nname=A\nauthor=\xe0\x83\xa9\nversion=1\n",           /* U+00E9 overlong: refused by its form, not its value */
        "id=a\nname=A\nauthor=\xf0\x8f\xbf\xbf\nversion=1\n",
        "id=a\nname=A\nauthor=\xed\xa0\x80\nversion=1\n",           /* a surrogate */
        "id=a\nname=A\nauthor=\xf4\x90\x80\x80\nversion=1\n",       /* over U+10FFFF */
        "id=a\nname=A\nauthor=a\xe2\x80\xae" "b\nversion=1\n",      /* U+202E */
        "id=a\nname=A\nauthor=a\xe2\x80\xaa" "b\nversion=1\n",      /* U+202A */
        "id=a\nname=A\nauthor=a\xe2\x81\xa6" "b\nversion=1\n",      /* U+2066 */
        "id=a\nname=A\nauthor=a\xe2\x81\xa9" "b\nversion=1\n",      /* U+2069 */
        "id=a\nname=A\nauthor=a\xc2\x9b" "b\nversion=1\n",          /* C1 CSI */
        "id=a\nname=A\nversion=1\nauthor=\x1b[31mx\n",    /* ESC */
    };
    ModInfo m;
    char why[160];
    for (size_t i = 0; i < sizeof BAD / sizeof BAD[0]; i++) CHECK_EQ_INT(parse(BAD[i], &m, why), 0);
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1\nauthor=\xf0\x9f\x8f\x81 \xe2\x80\x8b\n", &m, why), 1);   /* four bytes, and U+200B are fine */
}

/* review of 4977a5c5, 8: what the card says back never carries its own bytes for a key the game does not know */
static void test_messages_hold_no_control_bytes(void)
{
    static const char *const CARDS[] = {
        "id=a\nname=A\nversion=1\n\x1b]0;owned\x07=\x01\n",
        "id=a\nname=A\nversion=1\n\x1b[2J\n",
        "\x1b[2Jname=A\n",
    };
    ModInfo m;
    char why[160];
    for (size_t i = 0; i < sizeof CARDS / sizeof CARDS[0]; i++) {
        why[0] = '\0';
        parse(CARDS[i], &m, why);
        for (const char *c = why; *c; c++) CHECK((unsigned char)*c >= 0x20 && *c != 0x7f);
    }
    CHECK_EQ_INT(parse(CARDS[0], &m, why), 1);   /* a key the game does not know is left alone, whatever it holds */
}

static void test_piece_fields(void)
{
    ModInfo m;
    char why[160];
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1\n", &m, why), 1);
    CHECK_EQ_INT(m.type, MOD_TYPE_TWEAK);   /* no type line: a tweak */
    CHECK(m.stem[0] == '\0' && m.short_name[0] == '\0' && m.sound_from[0] == '\0' && m.ai_from[0] == '\0' && m.dash_from[0] == '\0');
    CHECK_EQ_INT(parse("id=f40\nname=F40\nversion=1\ntype=car\nstem=F40\nshort_name=F40\n"
                       "sound_from=512tr\nai_from=diablo\ndash_from=512tr\n", &m, why), 1);
    CHECK_EQ_INT(m.type, MOD_TYPE_CAR);
    CHECK(strcmp(m.stem, "F40") == 0 && strcmp(m.short_name, "F40") == 0);
    CHECK(strcmp(m.sound_from, "512tr") == 0 && strcmp(m.ai_from, "diablo") == 0 && strcmp(m.dash_from, "512tr") == 0);
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1\ntype=tweak\n", &m, why), 1);
    CHECK_EQ_INT(m.type, MOD_TYPE_TWEAK);
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1\ntype=track\n", &m, why), 1);
    CHECK_EQ_INT(m.type, MOD_TYPE_TRACK);
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1\ntype=plane\n", &m, why), 0);   /* an unknown type */
    CHECK(strstr(why, "type") != NULL);
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1\ntype=Car\n", &m, why), 0);
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1\ntype=car\ntype=car\n", &m, why), 0);   /* twice */
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1\ntype=car\nstem=f40\n", &m, why), 0);   /* lower case */
    CHECK(strstr(why, "stem") != NULL);
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1\ntype=car\nstem=TOOLONG99\n", &m, why), 0);
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1\ntype=car\nstem=A/B\n", &m, why), 0);
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1\ntype=car\nstem=..\n", &m, why), 0);
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1\ntype=car\nstem=ABCDEFGH\n", &m, why), 1);   /* 8 */
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1\ntype=car\nshort_name=ABCDEFGHI\n", &m, why), 0);   /* 9 */
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1\ntype=car\nshort_name=T\x1b[2J\n", &m, why), 0);
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1\ntype=car\nsound_from=Viper\n", &m, why), 0);   /* a-z 0-9 only */
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1\ntype=car\nai_from=rx-7\n", &m, why), 0);
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1\ntype=car\ndash_from=aaaaaaaaa\n", &m, why), 0);
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1\ntype=car\nsound_from=lada\n", &m, why), 1);   /* the word is the registry's to check */
    /* 3b: ai_level, a word (which words are levels is car_ai's to say); ai_from is still read, so a piece that has it is refused with why */
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1\ntype=car\nai_level=top\n", &m, why), 1);
    CHECK(strcmp(m.ai_level, "top") == 0);
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1\ntype=car\nai_level=turbo\n", &m, why), 1);
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1\ntype=car\nai_level=Fast\n", &m, why), 0);   /* a-z 0-9 only */
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1\ntype=car\nai_level=top\nai_level=top\n", &m, why), 0);   /* twice */
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1\n", &m, why), 1);
    CHECK(m.ai_level[0] == '\0');
}

/* dep59 step 3: a car's figures, shown as written, and its paints, the Color selector's choices (Review Focus 3) */
static void test_figures_and_paints(void)
{
    ModInfo m;
    char why[160];
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1\n", &m, why), 1);
    CHECK(m.year[0] == '\0' && m.power[0] == '\0' && m.weight[0] == '\0' && m.paint_count == 0);
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1\ntype=car\nyear=1987\npower=478 hp\nweight=1100 kg\n"
                       "paints=Rosso Corsa:c8102e, Giallo:FFD100\n", &m, why), 1);
    CHECK(strcmp(m.year, "1987") == 0 && strcmp(m.power, "478 hp") == 0 && strcmp(m.weight, "1100 kg") == 0);
    CHECK_EQ_INT(m.paint_count, 2);
    CHECK(strcmp(m.paints[0].name, "Rosso Corsa") == 0 && m.paints[0].rgb[0] == 0xc8 && m.paints[0].rgb[1] == 0x10 && m.paints[0].rgb[2] == 0x2e);
    CHECK(strcmp(m.paints[1].name, "Giallo") == 0 && m.paints[1].rgb[0] == 0xff && m.paints[1].rgb[1] == 0xd1 && m.paints[1].rgb[2] == 0x00);
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1\npower=12345678901234567\n", &m, why), 0);   /* 17 */
    CHECK(strstr(why, "power") != NULL);
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1\nweight=1234567890123456\n", &m, why), 1);   /* 16 */
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1\npaints=Nandu negro:000000\n", &m, why), 1);   /* ASCII, as a name is */
    CHECK(strcmp(m.paints[0].name, "Nandu negro") == 0);
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1\npaints=ABCDEFGHIJKLMNOPQRSTUVWX:000000\n", &m, why), 1);   /* 24 */

    char paints[600] = "id=a\nname=A\nversion=1\npaints=";
    for (int i = 0; i < 16; i++) snprintf(paints + strlen(paints), sizeof paints - strlen(paints), "%sP%d:%06x", i ? "," : "", i, i);
    CHECK_EQ_INT(parse(paints, &m, why), 1);
    CHECK_EQ_INT(m.paint_count, 16);
    CHECK(strcmp(m.paints[15].name, "P15") == 0 && m.paints[15].rgb[2] == 15);
    strcat(paints, ",P16:000010");
    CHECK_EQ_INT(parse(paints, &m, why), 0);   /* 17 */
    CHECK(strstr(why, "16") != NULL);

    static const char *const BAD[][2] = {
        { "Rosso:c8102", "Rosso" },       /* five digits */
        { "Rosso:zz102e", "Rosso" },      /* not hex */
        { "Rosso:c8102e0", "Rosso" },     /* seven */
        { ":c8102e", ":c8102e" },         /* no name */
        { "A:b:c8102e", "A:b:c8102e" },   /* a ':' in the name */
        { "Rosso", "Rosso" },             /* no colour */
        { "Rosso:c8102e,rosso:000000", "rosso" },   /* two alike in any case */
        { "Rosso:c8102e,,Blu:0000ff", "empty" },
        { "ABCDEFGHIJKLMNOPQRSTUVWXY:000000", "ABCDEFGHIJKLMNOPQRSTUVWXY" },   /* 25 */
    };
    for (size_t i = 0; i < sizeof BAD / sizeof BAD[0]; i++) {
        char text[200];
        snprintf(text, sizeof text, "id=a\nname=A\nversion=1\npaints=%s\n", BAD[i][0]);
        why[0] = '\0';
        CHECK_EQ_INT(parse(text, &m, why), 0);
        CHECK(strstr(why, "paints") != NULL && strstr(why, BAD[i][1]) != NULL);   /* the reason names the entry */
        CHECK_EQ_INT(m.paint_count, 0);
    }
}

/* section 1 of notes/dep59-added-gallery-showcase-plan.md: card_name=off for a card that shows its own name (default on) */
static void test_card_name(void)
{
    ModInfo m;
    char why[160];
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1\ntype=car\n", &m, why), 1);
    CHECK_EQ_INT(m.card_name_off, 0);
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1\ntype=car\ncard_name=off\n", &m, why), 1);
    CHECK_EQ_INT(m.card_name_off, 1);
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1\ntype=car\ncard_name = on \n", &m, why), 1);
    CHECK_EQ_INT(m.card_name_off, 0);
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1\ntype=car\ncard_name=no\n", &m, why), 0);
    CHECK(strstr(why, "card_name") != NULL && strstr(why, "on or off") != NULL);
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1\ntype=car\ncard_name=Off\n", &m, why), 0);
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1\ntype=car\ncard_name=off\ncard_name=off\n", &m, why), 0);   /* twice */
}

/* dep59 step 5 (T3): a track's keys are read as written, like a car's; whether a piece is a good track is track_piece_check's to say */
static void test_track_fields(void)
{
    ModInfo m;
    char why[160];
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1\ntype=track\n", &m, why), 1);
    CHECK_EQ_INT(m.type, MOD_TYPE_TRACK);
    CHECK(m.kind[0] == '\0' && m.segments[0] == '\0' && m.road[0] == '\0' && m.edge[0] == '\0');   /* none given: "" */
    CHECK(m.length[0] == '\0' && m.laps[0] == '\0' && m.times[0] == '\0');
    CHECK_EQ_INT(parse("id=turin\nname=Turin\nversion=1\ntype=track\nstem=TURINI\nkind=stage\nsegments=2\nlength=14.2 km\n"
                       "laps=4,8,16\ntimes=dawn,day,dusk,night\nroad=asphalt\nedge=gravel\n", &m, why), 1);
    CHECK(strcmp(m.stem, "TURINI") == 0 && strcmp(m.kind, "stage") == 0 && strcmp(m.segments, "2") == 0);
    CHECK(strcmp(m.length, "14.2 km") == 0 && strcmp(m.laps, "4,8,16") == 0);
    CHECK(strcmp(m.times, "dawn,day,dusk,night") == 0 && strcmp(m.road, "asphalt") == 0 && strcmp(m.edge, "gravel") == 0);
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1\ntype=track\n", &m, why), 1);
    CHECK(m.showcase_tint[0] == '\0');   /* none given: "" */
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1\ntype=track\nshowcase_tint=#336699\n", &m, why), 1);
    CHECK(strcmp(m.showcase_tint, "#336699") == 0);
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1\ntype=track\nshowcase_tint=#33669900\n", &m, why), 0);   /* longer than #RRGGBB */
    CHECK(strstr(why, "showcase_tint") != NULL);
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1\ntype=track\nkind=Stage\n", &m, why), 0);   /* a-z and 0-9 only */
    CHECK(strstr(why, "kind") != NULL);
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1\ntype=track\nsegments=12345678\n", &m, why), 1);   /* 8: the piece check judges the value */
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1\ntype=track\nsegments=123456789\n", &m, why), 0);
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1\ntype=track\nroad=asphalt\nroad=dirt\n", &m, why), 0);   /* twice */
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1\ntype=track\nlength=12345678901234567\n", &m, why), 0);   /* 17 > 16 */
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1\ntype=track\ntimes=dawn,day,dusk,night,dawn,day,dusk\n", &m, why), 0);   /* longer than 32 */
    CHECK(strstr(why, "times") != NULL);
}

/* port (dep59 showcase, the owner 2026-11-17): category=, photos= (the big photos, in order, up to 8) and photo_small= */
static void test_the_showcase_keys(void)
{
    ModInfo m;
    char why[160];
    const char *head = "id=a\nname=A\nversion=1\n";
    char card[400];
    snprintf(card, sizeof card, "%scategory=B\nphotos=front.png, side-2.png,rear_3.png\nphoto_small=engine.png\n", head);
    CHECK_EQ_INT(parse(card, &m, why), 1);
    CHECK(strcmp(m.category, "B") == 0);
    CHECK_EQ_INT(m.photo_count, 3);
    CHECK(strcmp(m.photo[0], "front.png") == 0 && strcmp(m.photo[1], "side-2.png") == 0 && strcmp(m.photo[2], "rear_3.png") == 0);
    CHECK(strcmp(m.photo_small, "engine.png") == 0);
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1\n", &m, why), 1);   /* none given: none */
    CHECK(m.category[0] == '\0' && m.photo_count == 0 && m.photo_small[0] == '\0');
    static const char *const BAD[] = {
        "photos=front.png,front.png\n",                      /* a name twice */
        "photos=a.png,b.png,c.png,d.png,e.png,f.png,g.png,h.png,i.png\n",   /* more than 8 */
        "photos=front.jpg\n",                                /* not a png */
        "photos=Front.png\n",                                /* capitals */
        "photos=dir/front.png\n",                            /* a path */
        "photos=front.png,,side.png\n",                      /* an empty entry */
        "photos=card.png\n",                                 /* a name the game's pictures use */
        "photos=rival.png\n",
        "photos=gallery.png\n",
        "photo_small=a.png,b.png\n",                         /* one only */
        "photo_small=small.bmp\n",
        "photo_small=card.png\n",
        "category=an unreasonably long category\n",          /* up to 16 */
    };
    for (size_t i = 0; i < sizeof BAD / sizeof BAD[0]; i++) {
        snprintf(card, sizeof card, "%s%s", head, BAD[i]);
        CHECK_EQ_INT(parse(card, &m, why), 0);
        CHECK(why[0] != '\0');
        CHECK(strstr(why, "photo") != NULL || strstr(why, "category") != NULL);   /* the refusal names the key */
    }
}

/* port (owner 2026-11-18): make=, model= and country= are read and kept, year= was already; empty when not given, refused when too long */
static void test_the_identity_keys(void)
{
    ModInfo m;
    char why[160];
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1\nmake=Porsche\nmodel=911 Carrera\nyear=1996\ncountry=Germany\n", &m, why), 1);
    CHECK(strcmp(m.make, "Porsche") == 0 && strcmp(m.model, "911 Carrera") == 0 && strcmp(m.year, "1996") == 0 && strcmp(m.country, "Germany") == 0);
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1\n", &m, why), 1);
    CHECK(m.make[0] == '\0' && m.model[0] == '\0' && m.country[0] == '\0');
    CHECK_EQ_INT(parse("id=a\nname=A\nversion=1\nmake=a make that is far too long to be a make of any car\n", &m, why), 0);
    CHECK(strstr(why, "make") != NULL);
}

int main(void)
{
    test_the_identity_keys();
    test_a_full_card();
    test_refusals();
    test_utf8_text_is_kept();
    test_the_showcase_keys();
    test_drawn_names_are_ascii();
    test_values_are_clean_utf8();
    test_messages_hold_no_control_bytes();
    test_piece_fields();
    test_figures_and_paints();
    test_card_name();
    test_track_fields();
    return TEST_REPORT();
}
