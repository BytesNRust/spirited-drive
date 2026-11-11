/* The boot screen's words: who presents it, what it is, whose game it is, the version. */
#include "game/frontend/boot_screen.h"
#include "test.h"

#include <string.h>

static int has(const BootLine *lines, int n, const char *needle)
{
    for (int i = 0; i < n; i++)
        if (strstr(lines[i].text, needle) != NULL) return 1;
    return 0;
}

static void says_what_it_is_and_whose_it_is(void)
{
    BootLine lines[BOOT_LINES_MAX];
    const int n = boot_screen_lines("0.9.0", lines);
    CHECK(n >= 5 && n <= BOOT_LINES_MAX);
    CHECK(has(lines, n, "Bytes & Rust presents"));
    CHECK(has(lines, n, "Spirited Drive"));
    CHECK(!has(lines, n, "TNFS"));   /* the game's old name is gone */
    CHECK(has(lines, n, "unofficial restomod of The Need for Speed SE for PC (1996)"));
    CHECK(has(lines, n, "2026 Bytes & Rust"));
    CHECK(has(lines, n, "GPL-3.0"));
    CHECK(has(lines, n, "Original game (c) 1995-96 Electronic Arts. Not affiliated with EA"));
    CHECK(has(lines, n, "v0.9.0"));
    CHECK_EQ_INT(lines[0].size, BOOT_HEADLINE);
}

static void the_version_line_is_optional(void)
{
    BootLine with[BOOT_LINES_MAX], without[BOOT_LINES_MAX], empty[BOOT_LINES_MAX];
    const int a = boot_screen_lines("0.9.0-dev+abc", with), b = boot_screen_lines(NULL, without), c = boot_screen_lines("", empty);
    CHECK_EQ_INT(a, b + 1);
    CHECK_EQ_INT(c, b);
    CHECK(has(with, a, "v0.9.0-dev+abc"));
    CHECK(!has(without, b, "v0."));
}

static void every_line_ends_and_fits(void)
{
    BootLine lines[BOOT_LINES_MAX];
    const int n = boot_screen_lines("0.9.0-beta.1+0123456789abcdef", lines);
    for (int i = 0; i < n; i++) {
        CHECK(lines[i].text[0] != '\0');
        CHECK(strlen(lines[i].text) < BOOT_LINE_CHARS);
        CHECK(lines[i].gap >= 0);
    }
}

static void the_credits_add_a_dedication_and_ours(void)
{
    BootLine lines[BOOT_LINES_MAX];
    int n = credits_extra_lines(0, "0.9.0", lines);
    CHECK(n >= 3);
    CHECK(has(lines, n, "This is their game."));
    CHECK(has(lines, n, "We restored it."));
    n = credits_extra_lines(1, "0.9.0", lines);
    CHECK(has(lines, n, "Original game: EA Canada with Road & Track 1994-1996"));
    CHECK(has(lines, n, "Spirited Drive: Bytes & Rust and contributors"));
    CHECK(!has(lines, n, "TNFS") && !has(lines, n, "Restomod"));   /* the credits name the game, not the mode */
    CHECK(has(lines, n, "GPL-3.0-or-later"));
    CHECK(has(lines, n, "CC BY-SA 4.0"));
    CHECK(has(lines, n, "Not affiliated with EA"));
    CHECK(has(lines, n, "v0.9.0"));
    CHECK_EQ_INT(credits_extra_lines(2, "0.9.0", lines), 0);    /* only the two pages */
    CHECK_EQ_INT(credits_extra_lines(-1, "0.9.0", lines), 0);
    for (int page = 0; page < CREDITS_EXTRA_PAGES; page++) {
        n = credits_extra_lines(page, "0.9.0", lines);
        for (int i = 0; i < n; i++) CHECK(strchr(lines[i].text, ',') == NULL);   /* the small font has no comma */
    }
}

int main(void)
{
    the_credits_add_a_dedication_and_ours();
    says_what_it_is_and_whose_it_is();
    the_version_line_is_optional();
    every_line_ends_and_fits();
    return TEST_REPORT();
}
