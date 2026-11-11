/* Our credits pages: the homage and ours, the words the owner approved (i030), where they stand, and that they fit. */
#include "game/frontend/credits_pages.h"
#include "test.h"

#include <string.h>

static const CreditsItem *find(const CreditsItem *items, int n, const char *text)
{
    for (int i = 0; i < n; i++)
        if (strcmp(items[i].text, text) == 0) return &items[i];
    return NULL;
}

static void the_homage_is_the_owners_words(void)
{
    CreditsItem it[CREDITS_ITEMS_MAX];
    const int n = credits_page(0, "0.9.0", it);
    CHECK_EQ_INT(n, 6);
    CHECK(strcmp(it[0].text, "They built it in 1996.") == 0 && it[0].kind == CREDITS_BIG && it[0].colour == CREDITS_YELLOW);
    CHECK(strcmp(it[1].text, "We kept it running.") == 0 && it[1].kind == CREDITS_BIG && it[1].colour == CREDITS_CREAM);
    CHECK(strcmp(it[2].text, "For the people at EA Canada") == 0);
    CHECK(strcmp(it[3].text, "Pioneer Productions and EA Seattle") == 0);   /* i055: the owner's option A, four lines, no commas */
    CHECK(strcmp(it[4].text, "who made it") == 0);
    CHECK(strcmp(it[5].text, "and for everyone who still plays it") == 0);
    for (int i = 2; i < n; i++) CHECK(it[i].kind == CREDITS_LINE && it[i].y > it[i - 1].y);
    for (int i = 3; i < n; i++) CHECK_EQ_INT(it[i].y - it[i - 1].y, it[3].y - it[2].y);   /* evenly spaced */
    CHECK_EQ_INT(it[2].y + it[5].y, 2 * 310);   /* the block centred on y 310 */
    CHECK(it[2].y - it[1].y >= 100);   /* the owner: clear room between the big lines and the dedication */
}

static void ours_is_set_in_two_columns(void)
{
    CreditsItem it[CREDITS_ITEMS_MAX];
    const int n = credits_page(1, "0.9.0", it);
    static const char *const LEFT[] = { "Original game", "EA Canada", "Pioneer Productions", "with Road & Track", "1994-1996", "Special Edition", "EA Seattle", "with EA Canada" };
    static const char *const RIGHT[] = { "Spirited Drive", "Restomod", "Bytes & Rust", "and contributors", "Licence", "Code GPL-3.0-or-later",
                                         "Our data CC BY-SA 4.0", "Made with", "SDL3" };
    int last = -1;
    for (size_t i = 0; i < sizeof LEFT / sizeof LEFT[0]; i++) {
        const CreditsItem *c = find(it, n, LEFT[i]);
        CHECK(c != NULL);
        if (c == NULL) continue;
        CHECK(c->cx < 320);
        CHECK(c->y > last);   /* top to bottom in the order read */
        last = c->y;
    }
    last = -1;
    for (size_t i = 0; i < sizeof RIGHT / sizeof RIGHT[0]; i++) {
        const CreditsItem *c = find(it, n, RIGHT[i]);
        CHECK(c != NULL);
        if (c == NULL) continue;
        CHECK(c->cx > 320);
        CHECK(c->y > last);
        last = c->y;
    }
    CHECK(find(it, n, "Original game")->kind == CREDITS_HEADER);
    CHECK(find(it, n, "Licence")->kind == CREDITS_HEADER);
    CHECK(find(it, n, "Special Edition")->kind == CREDITS_HEADER);
    CHECK(find(it, n, "Video") == NULL);   /* Pioneer made the game with EA Canada, not its video (i055) */
    CHECK(find(it, n, "Bytes & Rust")->kind == CREDITS_NAME);
    CHECK(find(it, n, "Spirited Drive")->colour == CREDITS_YELLOW);
    CHECK(find(it, n, "Unofficial. Not affiliated with Electronic Arts") != NULL);
    CHECK(find(it, n, "The Need for Speed SE (1996) is (c) Electronic Arts.  GPL-3.0") != NULL);
    CHECK(find(it, n, "v0.9.0") != NULL);
}

static void the_version_line_is_optional(void)
{
    CreditsItem a[CREDITS_ITEMS_MAX], b[CREDITS_ITEMS_MAX], c[CREDITS_ITEMS_MAX];
    const int with = credits_page(1, "0.9.0-dev+abc", a), without = credits_page(1, NULL, b), empty = credits_page(1, "", c);
    CHECK_EQ_INT(with, without + 1);
    CHECK_EQ_INT(empty, without);
    CHECK(find(a, with, "v0.9.0-dev+abc") != NULL);
}

static void only_two_pages_and_everything_on_screen(void)
{
    CreditsItem it[CREDITS_ITEMS_MAX];
    CHECK_EQ_INT(credits_page(2, "0.9.0", it), 0);
    CHECK_EQ_INT(credits_page(-1, "0.9.0", it), 0);
    for (int page = 0; page < CREDITS_PAGES; page++) {
        const int n = credits_page(page, "0.9.0-beta.1+0123456789abcdef", it);
        CHECK(n > 0 && n <= CREDITS_ITEMS_MAX);
        for (int i = 0; i < n; i++) {
            CHECK(it[i].text[0] != '\0' && strlen(it[i].text) < CREDITS_ITEM_CHARS);
            CHECK(it[i].y >= 0 && it[i].y < 470);
            CHECK(it[i].cx - CREDITS_COLUMN_W / 2 >= 0 && it[i].cx + CREDITS_COLUMN_W / 2 <= 640);
            CHECK(strchr(it[i].text, ',') == NULL);   /* the approved words have none */
        }
    }
}

int main(void)
{
    the_homage_is_the_owners_words();
    ours_is_set_in_two_columns();
    the_version_line_is_optional();
    only_two_pages_and_everything_on_screen();
    return TEST_REPORT();
}
