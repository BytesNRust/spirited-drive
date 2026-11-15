/* dep59: the install check of a mod's zip (formats/mod_check.h): a good mod passes with its card, kinds and root; every refusal refuses the
 * whole mod and says why (zip-slip names, links, case twins, files the game does not read, bad content, no card); the system's litter is
 * left out; the zip of a folder takes that folder as its root. */
#include "formats/mod_check.h"
#include "formats/png.h"
#include "test.h"
#include "zip_build.h"

#include <stdlib.h>
#include <string.h>

static const char *const CARD = "id=test-mod\nname=Test Mod\nversion=1.0\nauthor=Tester\n";
static uint8_t buf[1 << 18];
static char why[256];

static int check(const ZbEntry *e, int n, ModCheck *c)
{
    const size_t size = zb_build(e, n, buf, sizeof buf);
    CHECK(size > 0);
    why[0] = '\0';
    const int ok = mod_check_zip(buf, size, c, why, sizeof why);
    if (!ok) CHECK(why[0] != '\0');   /* a refusal always says why */
    return ok;
}

/* a 2 x 2 PNG, `w` x `h` when asked */
static ZbEntry png_entry(const char *name, int w, int h, uint8_t **keep)
{
    static uint8_t px[600 * 300];
    uint8_t pal[768] = { 0 };
    size_t len = 0;
    *keep = NULL;
    CHECK_EQ_INT(png_write_indexed(px, w, h, pal, keep, &len), PNG_OK);
    ZbEntry e = { name, *keep, (uint32_t)len, 0, 0, 0, 0, 3, 0100644u << 16 };
    return e;
}

static void test_a_good_mod(void)
{
    uint8_t *png = NULL, *preview = NULL;
    ZbEntry e[] = {
        zb_text("mod.ini", CARD),
        zb_text("track_light/CY1N.txt", "track CY1 N\n"),
        zb_text("track/", ""),
        png_entry("car_masks/NSX/body_mask.png", 2, 2, &png),
        png_entry("preview.png", 512, 256, &preview),
        zb_text("README.md", "# Test\n"),
        zb_text("__MACOSX/._mod.ini", "junk"),
        zb_text("sounds/.DS_Store", "junk"),
    };
    ModCheck c;
    CHECK_EQ_INT(check(e, (int)(sizeof e / sizeof e[0]), &c), 1);
    CHECK(strcmp(c.info.id, "test-mod") == 0 && strcmp(c.info.version, "1.0") == 0);
    CHECK_EQ_INT((int)c.files, 5);   /* the card, the light, the mask, the preview and the README; the folder entry and the litter not */
    CHECK_EQ_INT((int)c.kinds[MOD_KIND_TEXT], 1);
    CHECK_EQ_INT((int)c.kinds[MOD_KIND_PNG], 2);
    CHECK_EQ_INT((int)c.kinds[MOD_KIND_CARD], 1);
    CHECK_EQ_INT((int)c.kinds[MOD_KIND_DOC], 1);
    CHECK_EQ_INT(c.has_preview, 1);
    CHECK(c.root[0] == '\0');
    char rel[300];
    CHECK_EQ_INT(mod_check_install_path(&c, "track_light/CY1N.txt", rel, sizeof rel), 1);
    CHECK(strcmp(rel, "track_light/CY1N.txt") == 0);
    free(png), free(preview);
}

static void test_the_zip_of_a_folder(void)
{
    ZbEntry e[] = { zb_text("My Mod/", ""), zb_text("My Mod/mod.ini", CARD), zb_text("My Mod/track/CY1.txt", "track CY1\n"),
                    zb_text("__MACOSX/My Mod/._mod.ini", "junk") };
    ModCheck c;
    CHECK_EQ_INT(check(e, 4, &c), 1);
    CHECK(strcmp(c.root, "My Mod/") == 0);
    CHECK_EQ_INT((int)c.files, 2);
    char rel[300];
    CHECK_EQ_INT(mod_check_install_path(&c, "My Mod/track/CY1.txt", rel, sizeof rel), 1);
    CHECK(strcmp(rel, "track/CY1.txt") == 0);
    CHECK_EQ_INT(mod_check_install_path(&c, "Other/track/CY1.txt", rel, sizeof rel), 0);
    ZbEntry two[] = { zb_text("A/mod.ini", CARD), zb_text("B/track/CY1.txt", "track CY1\n") };   /* a file outside the folder */
    CHECK_EQ_INT(check(two, 2, &c), 0);
}

static void test_refusals(void)
{
    static const struct { const char *name, *text; } BAD[] = {
        { "../evil.txt", "x" },                    /* zip-slip */
        { "track/../../evil.txt", "x" },
        { "/etc/evil", "x" },                      /* absolute */
        { "SIMDATA/../../x", "x" },
        { "run.sh", "#!/bin/sh\n" },               /* nothing the game reads */
        { "lib/evil.dylib", "x" },
        { "track_light/sub/CY1N.txt", "x" },       /* not where the game looks */
        { "track_light/CY1N.dat", "x" },
        { "sounds/thunder.wav", "not a wav" },     /* bad content */
        { "car_envmap.png", "not a png" },
        { "track/CY1.txt", "a\0b" },               /* text with a NUL (cut at the NUL by zb_text: see below) */
    };
    ModCheck c;
    for (size_t i = 0; i < sizeof BAD / sizeof BAD[0]; i++) {
        ZbEntry e[2] = { zb_text("mod.ini", CARD), zb_text(BAD[i].name, BAD[i].text) };
        if (i == sizeof BAD / sizeof BAD[0] - 1) e[1].csize = 3;   /* the NUL kept */
        CHECK_EQ_INT(check(e, 2, &c), 0);
    }
    ZbEntry none[] = { zb_text("track/CY1.txt", "track CY1\n") };   /* no card */
    CHECK_EQ_INT(check(none, 1, &c), 0);
    CHECK(strstr(why, "mod.ini") != NULL);
    ZbEntry bad_card[] = { zb_text("mod.ini", "id=Bad Id\nname=x\nversion=1\n") };
    CHECK_EQ_INT(check(bad_card, 1, &c), 0);
    ZbEntry link[] = { zb_text("mod.ini", CARD), zb_text("track/CY1.txt", "/etc/passwd") };
    link[1].ext_attr = 0120777u << 16;
    CHECK_EQ_INT(check(link, 2, &c), 0);
    ZbEntry twins[] = { zb_text("mod.ini", CARD), zb_text("track/CY1.txt", "a"), zb_text("TRACK/cy1.TXT", "b") };
    CHECK_EQ_INT(check(twins, 3, &c), 0);
    uint8_t *big = NULL;
    ZbEntry wide[] = { zb_text("mod.ini", CARD), png_entry("preview.png", 513, 2, &big) };
    CHECK_EQ_INT(check(wide, 2, &c), 0);   /* the preview over its size */
    free(big);
    ZbEntry game[] = { zb_text("mod.ini", CARD), zb_text("SIMDATA/MISC/TR1.TRI", "too short") };
    CHECK_EQ_INT(check(game, 2, &c), 0);   /* the original would not load it */
    CHECK(strstr(why, "TR1.TRI") != NULL);
    CHECK_EQ_INT(mod_check_zip((const uint8_t *)"PK no", 5, &c, why, sizeof why), 0);
}

static void test_kinds(void)
{
    CHECK_EQ_INT(mod_file_kind("SIMDATA/MISC/TR1.TRI"), MOD_KIND_GAME);
    CHECK_EQ_INT(mod_file_kind("simdata/misc/tr1.tri"), MOD_KIND_GAME);   /* the overlay finds either case */
    CHECK_EQ_INT(mod_file_kind("FRONTEND/ART/CONTROL/CITY.QFS"), MOD_KIND_GAME);
    CHECK_EQ_INT(mod_file_kind("SIMDATA"), MOD_KIND_NONE);
    CHECK_EQ_INT(mod_file_kind("track_light/CY1N.txt"), MOD_KIND_TEXT);
    CHECK_EQ_INT(mod_file_kind("dash_masks/NSX/dash_color.png"), MOD_KIND_PNG);
    CHECK_EQ_INT(mod_file_kind("track_masks/CY1/slot_03.png"), MOD_KIND_PNG);
    CHECK_EQ_INT(mod_file_kind("car_masks/NSX/body_mask.txt"), MOD_KIND_NONE);
    CHECK_EQ_INT(mod_file_kind("sounds/rain_puddle_3.wav"), MOD_KIND_WAV);
    CHECK_EQ_INT(mod_file_kind("LICENSE"), MOD_KIND_DOC);
    CHECK_EQ_INT(mod_file_kind("readme.txt"), MOD_KIND_DOC);
    CHECK_EQ_INT(mod_file_kind("Thumbs.db"), MOD_KIND_SKIP);
    CHECK_EQ_INT(mod_file_kind("notes.txt"), MOD_KIND_NONE);
}

int main(void)
{
    test_kinds();
    test_a_good_mod();
    test_the_zip_of_a_folder();
    test_refusals();
    return TEST_REPORT();
}
