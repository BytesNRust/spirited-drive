/* The boot screen's words and places: who presents it, the game's name for when there is no banner, what it is, whose game it is, the version. */
#include "game/frontend/boot_screen.h"
#include "test.h"

#include <string.h>

static const BootLine *find(const BootLine *lines, int n, const char *needle)
{
    for (int i = 0; i < n; i++)
        if (strstr(lines[i].text, needle) != NULL) return &lines[i];
    return NULL;
}

static void says_what_it_is_and_whose_it_is(void)
{
    BootLine lines[BOOT_LINES_MAX];
    const int n = boot_screen_lines("0.9.0", lines);
    CHECK(n >= 6 && n <= BOOT_LINES_MAX);
    CHECK(find(lines, n, "Bytes & Rust") != NULL && find(lines, n, "Bytes & Rust")->style == BOOT_HOUSE);
    CHECK(find(lines, n, "PRESENTS") != NULL && find(lines, n, "PRESENTS")->style == BOOT_PRESENTS);
    CHECK(find(lines, n, "Spirited Drive") != NULL && find(lines, n, "Spirited Drive")->style == BOOT_NAME);
    CHECK(find(lines, n, "TNFS") == NULL);   /* the game's old name is gone */
    CHECK(find(lines, n, "A restomod of The Need for Speed SE") != NULL);
    CHECK(find(lines, n, "Unofficial. Not affiliated with Electronic Arts") != NULL);
    CHECK(find(lines, n, "The Need for Speed SE (1996) is (c) Electronic Arts.  GPL-3.0") != NULL);
    CHECK(find(lines, n, "v0.9.0") != NULL);
}

/* the owner's mockup's places: the house at the top, the name in the banner's box, the rest below it, top to bottom */
static void the_places_are_the_mockups(void)
{
    BootLine lines[BOOT_LINES_MAX];
    const int n = boot_screen_lines("0.9.0", lines);
    CHECK_EQ_INT(lines[0].y, 30);
    CHECK_EQ_INT(lines[1].y, 102);
    const BootLine *name = find(lines, n, "Spirited Drive");
    CHECK(name != NULL && name->y > BOOT_BANNER_Y && name->y < BOOT_BANNER_Y + BOOT_BANNER_H);
    CHECK(find(lines, n, "A restomod")->y > BOOT_BANNER_Y + BOOT_BANNER_H);
    CHECK(lines[1].y + 20 < BOOT_BANNER_Y);   /* PRESENTS clear of the banner */
    for (int i = 1; i < n; i++) CHECK(lines[i].y > lines[i - 1].y);
    CHECK(lines[n - 1].y < 470);
    CHECK_EQ_INT(BOOT_BANNER_X * 2 + BOOT_BANNER_W, 640);   /* centred */
}

static void the_version_line_is_optional(void)
{
    BootLine with[BOOT_LINES_MAX], without[BOOT_LINES_MAX], empty[BOOT_LINES_MAX];
    const int a = boot_screen_lines("0.9.0-dev+abc", with), b = boot_screen_lines(NULL, without), c = boot_screen_lines("", empty);
    CHECK_EQ_INT(a, b + 1);
    CHECK_EQ_INT(c, b);
    CHECK(find(with, a, "v0.9.0-dev+abc") != NULL);
    CHECK(find(without, b, "v0.") == NULL);
}

static void every_line_ends_and_fits(void)
{
    BootLine lines[BOOT_LINES_MAX];
    const int n = boot_screen_lines("0.9.0-beta.1+0123456789abcdef", lines);
    for (int i = 0; i < n; i++) {
        CHECK(lines[i].text[0] != '\0');
        CHECK(strlen(lines[i].text) < BOOT_LINE_CHARS);
        CHECK(lines[i].y >= 0 && lines[i].y < 480);
    }
}

int main(void)
{
    says_what_it_is_and_whose_it_is();
    the_places_are_the_mockups();
    the_version_line_is_optional();
    every_line_ends_and_fits();
    return TEST_REPORT();
}
