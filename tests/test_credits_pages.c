/* Our credits pages: the homage and ours, the words the owner approved (i030), where they stand, and that they fit. */
#include "game/frontend/credits_pages.h"
#include "game/frontend/credits.h"
#include "game/sys/game_name.h"
#include "game/sys/input.h"
#include "game/sys/pointer.h"
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

/* the ask for a star: two centred lines under both columns and over the legal lines, the words in yellow and the address under them */
static void ours_asks_for_a_star(void)
{
    CreditsItem it[CREDITS_ITEMS_MAX];
    const int n = credits_page(1, "0.9.0", it);
    const CreditsItem *const star = find(it, n, "Star us on GitHub"), *const where = find(it, n, "github.com/BytesNRust/spirited-drive");
    CHECK(star != NULL && where != NULL);
    if (star == NULL || where == NULL) return;
    CHECK_EQ_INT(star->cx, 320);
    CHECK_EQ_INT(where->cx, 320);
    CHECK(star->colour == CREDITS_YELLOW && where->kind == CREDITS_NOTE);
    CHECK(star->y > find(it, n, "SDL3")->y + 30 && star->y > find(it, n, "with EA Canada")->y + 30);
    CHECK(where->y > star->y);
    CHECK(where->y + 30 < find(it, n, "Unofficial. Not affiliated with Electronic Arts")->y);
    CHECK(strstr(GAME_REPO_URL, where->text) != NULL);   /* the address shown is the one the Customize Game row opens */
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

/* the owner: our pages went by on their own in under a second each; they stay as the original five do (credits.h: 2000 ticks) and take the
 * same keys and buttons: Space, Enter, the left button and the pad's first button the next page, Escape, the right button and the pad's second
 * leave */
static void our_pages_stay_and_move_on_as_the_originals(void)
{
    CHECK_EQ_INT(CREDITS_OURS_PAGE_TICKS, CREDITS_PAGE_TICKS);
    HoldSource s[HOLD_SOURCES];
    static const struct { int32_t key, buttons, pad; int source; HoldStep step; } CASES[] = {
        { KEY_SPACE, 0, 0, HOLD_KEYS, HOLD_NEXT },   { KEY_ENTER, 0, 0, HOLD_KEYS, HOLD_NEXT },   { KEY_ESCAPE, 0, 0, HOLD_KEYS, HOLD_LEAVE },
        { KEY_UP, 0, 0, HOLD_KEYS, HOLD_STAY },      { 0, POINTER_BUTTON_SELECT, 0, HOLD_BUTTONS, HOLD_NEXT },
        { 0, POINTER_BUTTON_BACK, 0, HOLD_BUTTONS, HOLD_LEAVE }, { 0, 0, KEY_SPACE, HOLD_PAD, HOLD_NEXT }, { 0, 0, KEY_ESCAPE, HOLD_PAD, HOLD_LEAVE } };
    for (size_t i = 0; i < sizeof CASES / sizeof CASES[0]; i++) {
        credits_pages_sources(CASES[i].key, CASES[i].buttons, CASES[i].pad, s);
        CHECK(s[CASES[i].source].down);
        CHECK_EQ_INT(s[CASES[i].source].press, CASES[i].step);
        for (int k = 0; k < HOLD_SOURCES; k++) CHECK(k == CASES[i].source || !s[k].down);
    }
}

int main(void)
{
    our_pages_stay_and_move_on_as_the_originals();
    the_homage_is_the_owners_words();
    ours_is_set_in_two_columns();
    ours_asks_for_a_star();
    the_version_line_is_optional();
    only_two_pages_and_everything_on_screen();
    return TEST_REPORT();
}
