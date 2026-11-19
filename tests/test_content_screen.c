/* dep59 step 4b (T5): the CONTENT screen (game/frontend/content_screen.h) with fake ops: the mock-up's words in their places and styles
 * for the standard edition and an own one, the edits saved at once, the card's and the foot's buttons handed to the caller, Done kept
 * shut while the edition lacks a car or a track. The fake records the last page drawn; a key script plays the player. */
#include "game/frontend/content_screen.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { ENTER = 0xd, ESCAPE = 0x1b, UP = 0x4800, DOWN = 0x5000, LEFT = 0x4b00, RIGHT = 0x4d00 };
enum { DRAWN_MAX = 256, FRAMES_MAX = 4000 };

typedef struct {
    char text[96];
    int style, x, y, num, den;
} Drawn;

static struct {
    Drawn drawn[DRAWN_MAX];
    int drawn_count, fills, arrows, previews, backgrounds, shows;
    int pager_arrows[2];   /* the list pager's triangles drawn: the left, the right */
    char title[32];
    const int *keys;
    int key_count, key_next, gap, frames;
    Edition own[2];
    int own_count, saves, saved_index;
    Edition saved;
    int card_calls, card_button, card_tab;
    char card_id[40];
    int foot_calls, foot_button;
    int refused, refused_lacks;
    int used_in;
} fk;

static ModInfo installed[2], removed[1];
static int backups[2] = { 0, 1 };

static void f_background(void *ctx) { (void)ctx, fk.drawn_count = 0, fk.backgrounds++; }
static void f_text(void *ctx, int32_t style, const char *text, int32_t x, int32_t y, int32_t num, int32_t den)
{
    (void)ctx;
    if (fk.drawn_count >= DRAWN_MAX) return;
    Drawn *d = &fk.drawn[fk.drawn_count++];
    snprintf(d->text, sizeof d->text, "%s", text);
    d->style = style, d->x = x, d->y = y, d->num = num, d->den = den;
}
static int32_t f_width(void *ctx, const char *text, int32_t num, int32_t den) { (void)ctx; return (int32_t)strlen(text) * 10 * num / den; }
static void f_title(void *ctx, const char *text, int32_t x, int32_t y)
{
    (void)ctx;
    if (x == CONTENT_TITLE_X && y == CONTENT_TITLE_Y) snprintf(fk.title, sizeof fk.title, "%s", text);
}
static void f_fill(void *ctx, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t rgb) { (void)ctx, (void)x0, (void)y0, (void)x1, (void)y1, (void)rgb, fk.fills++; }
static void f_arrow(void *ctx, int32_t which, int32_t lit, int32_t x, int32_t y)
{
    (void)ctx, (void)lit, (void)x;
    if (y >= CONTENT_PAGE_Y - 10 && (which == CONTENT_ARROW_LEFT || which == CONTENT_ARROW_RIGHT)) fk.pager_arrows[which == CONTENT_ARROW_RIGHT]++;
    fk.arrows++;
}
static void f_preview(void *ctx, int32_t tab, const char *id, int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    (void)ctx, (void)tab, (void)id, (void)x0, (void)y0, (void)x1, (void)y1, fk.previews++;
}
static void f_show(void *ctx) { (void)ctx, fk.shows++; }
/* each key of the script once, a frame of none after it; Escape once it is spent */
static int32_t f_key(void *ctx)
{
    (void)ctx;
    if (fk.gap) return fk.gap = 0, 0;
    fk.gap = 1;
    return fk.key_next < fk.key_count ? fk.keys[fk.key_next++] : ESCAPE;
}
static int32_t f_pointer(void *ctx, int32_t *x, int32_t *y) { (void)ctx, *x = 0, *y = 0; return 0; }
static void f_wait(void *ctx)
{
    (void)ctx;
    if (++fk.frames > FRAMES_MAX) fprintf(stderr, "test_content_screen: the screen never closed\n"), exit(1);
}
static int32_t f_editions(void *ctx) { (void)ctx; return 1 + fk.own_count; }
static void f_edition_name(void *ctx, int32_t i, char *out, size_t cap)
{
    (void)ctx;
    snprintf(out, cap, "%s", i == 0 ? "Restomod '26" : fk.own[i - 1].name);
}
static int32_t f_edition_load(void *ctx, int32_t i, Edition *out)
{
    (void)ctx;
    if (i <= 0 || i > fk.own_count) return 0;
    *out = fk.own[i - 1];
    return 1;
}
static int32_t f_edition_save(void *ctx, int32_t i, const Edition *e)
{
    (void)ctx;
    fk.saves++, fk.saved_index = i, fk.saved = *e;
    if (i > 0 && i <= fk.own_count) fk.own[i - 1] = *e;
    return 1;
}
static void f_mods(void *ctx, ContentMods *out) { (void)ctx, *out = (ContentMods){ installed, backups, 2, removed, 1 }; }
static int32_t f_used_in(void *ctx, const char *id) { (void)ctx, (void)id; return fk.used_in; }
static int32_t f_card(void *ctx, int32_t button, int32_t tab, const char *id)
{
    (void)ctx;
    fk.card_calls++, fk.card_button = button, fk.card_tab = tab;
    snprintf(fk.card_id, sizeof fk.card_id, "%s", id);
    return 0;
}
static int32_t f_foot(void *ctx, int32_t button, int32_t *edition) { (void)ctx, (void)edition, fk.foot_calls++, fk.foot_button = button; return 0; }
static void f_refused(void *ctx, int32_t lacks) { (void)ctx, fk.refused++, fk.refused_lacks = lacks; }

static const ContentOps OPS = { f_background, f_text, f_width, f_title, f_fill, f_arrow, f_preview, f_show, f_key, f_pointer, f_wait,
                                f_editions, f_edition_name, f_edition_load, f_edition_save, f_mods, f_used_in, f_card, f_foot, f_refused,
                                NULL, NULL };

static ModInfo mod(const char *id, const char *name, const char *version, const char *author, int type)
{
    ModInfo m;
    memset(&m, 0, sizeof m);
    snprintf(m.id, sizeof m.id, "%s", id);
    snprintf(m.name, sizeof m.name, "%s", name);
    snprintf(m.version, sizeof m.version, "%s", version);
    snprintf(m.author, sizeof m.author, "%s", author);
    snprintf(m.description, sizeof m.description, "%s", "A car of the test's, made for it and for nothing else at all.");
    m.type = type;
    return m;
}

static Edition edition(const char *name, const char *cars, const char *tracks)
{
    char text[512];
    snprintf(text, sizeof text, "name=%s\ncars=%s\ntracks=%s\n", name, cars, tracks);
    Edition e;
    char why[128];
    CHECK(edition_parse((const uint8_t *)text, strlen(text), &e, why, sizeof why));
    return e;
}

static void reset(const int *keys, int n)
{
    memset(&fk, 0, sizeof fk);
    fk.keys = keys, fk.key_count = n;
    fk.gap = 1;   /* the first read none: the screen takes keys once it has seen none down */
    installed[0] = mod("porsche-993", "Porsche 993", "1.2", "Ed", MOD_TYPE_CAR);
    installed[1] = mod("night-ring", "Night Ring", "2.0", "Al", MOD_TYPE_TRACK);
    removed[0] = mod("old-car", "Old Car", "0.1", "Bo", MOD_TYPE_CAR);
    fk.own[0] = edition("Mine", "porsche-993,gone-car,rx7", "city");
    fk.own_count = 1;
    fk.used_in = 2;
}

static const Drawn *drawn(const char *text)
{
    for (int i = 0; i < fk.drawn_count; i++)
        if (strcmp(fk.drawn[i].text, text) == 0) return &fk.drawn[i];
    return NULL;
}

/* 1 when `text` was drawn there in that style (any of the times it was drawn); the places it was drawn printed when not */
static int drawn_at(const char *text, int x, int y, int style)
{
    for (int i = 0; i < fk.drawn_count; i++)
        if (strcmp(fk.drawn[i].text, text) == 0 && fk.drawn[i].x == x && fk.drawn[i].y == y && fk.drawn[i].style == style) return 1;
    for (int i = 0; i < fk.drawn_count; i++)
        if (strcmp(fk.drawn[i].text, text) == 0) fprintf(stderr, "%s at %d,%d style %d\n", text, fk.drawn[i].x, fk.drawn[i].y, fk.drawn[i].style);
    return 0;
}

/* the standard edition: its stock pieces on and dim, nothing to change; the note; Rename and Delete grey; Escape leaves on it */
static void test_the_standard_edition(void)
{
    reset(NULL, 0);
    int32_t edition = 0;
    content_screen_run(&OPS, &edition);
    CHECK_EQ_INT(edition, 0);
    CHECK(strcmp(fk.title, "Content") == 0);
    const char *sel = "Restomod '26";   /* between the gallery's two arrows, centred */
    CHECK(drawn_at(sel, CONTENT_SELECTOR_X - f_width(NULL, sel, 1, 1) / 2, CONTENT_SELECTOR_Y, CONTENT_CREAM));
    CHECK(fk.arrows >= 2);
    CHECK(drawn_at("Cars", 90 - f_width(NULL, "Cars", 3, 4) / 2, CONTENT_TAB_Y, CONTENT_YELLOW));   /* the chosen tab yellow, no brackets, centred */
    CHECK(drawn_at("Tracks", 197 - f_width(NULL, "Tracks", 3, 4) / 2, CONTENT_TAB_Y, CONTENT_CREAM));
    CHECK(drawn_at("Tweaks", 304 - f_width(NULL, "Tweaks", 3, 4) / 2, CONTENT_TAB_Y, CONTENT_CREAM));
    CHECK(drawn("[ Cars ]") == NULL);
    CHECK(drawn_at("All", CONTENT_NAME_X, CONTENT_LIST_Y, CONTENT_DIM));
    CHECK(drawn_at("[-]", CONTENT_BOX_X, CONTENT_LIST_Y, CONTENT_DIM));   /* locked, it shows what it holds: the stock on, the mods off */
    CHECK(drawn_at("Acura NSX", CONTENT_NAME_X, CONTENT_LIST_Y + 2 * CONTENT_LINE, CONTENT_DIM));
    CHECK(drawn("(game)") == NULL);   /* no version column (the owner's note 5): the card says what a piece of the game is */
    const Drawn *status = drawn("Read only. Duplicate to edit.");   /* under the selector, grey */
    CHECK(status != NULL && status->style == CONTENT_DIM && status->y == CONTENT_STATUS_Y);
    /* the selected row is a stock car: its card says whose it is and what, counts the standard edition, and has no buttons that never apply */
    CHECK(drawn("ORIGINAL - ELECTRONIC ARTS") != NULL);
    CHECK(drawn("Original car from the 1996 game") != NULL);
    {   /* a stock piece is in the standard edition too: one more than the mod store counts */
        char used[48];
        snprintf(used, sizeof used, "Used in: %d edition%s", fk.used_in + 1, fk.used_in + 1 == 1 ? "" : "s");
        CHECK(drawn(used) != NULL);
    }
    CHECK(drawn("Update...") == NULL && drawn("Uninstall") == NULL && drawn("Previous version") == NULL);
    CHECK(drawn("Rename") != NULL && drawn("Rename")->style == CONTENT_DIM);
    CHECK(drawn("Delete") != NULL && drawn("Delete")->style == CONTENT_DIM);
    CHECK(drawn("New") != NULL && drawn("New")->style == CONTENT_CREAM);
    const Drawn *done = drawn("Done");
    CHECK(done != NULL && done->x == CONTENT_DONE_X && done->num == 11 && done->den == 12);   /* at the CD's DONE place, left, its capitals 11 rows */
    CHECK(drawn("New") != NULL && drawn("New")->x > CONTENT_DONE_END);   /* the other buttons to its right */
    CHECK_EQ_INT(fk.pager_arrows[1], 1);   /* the page indicator's, on page 1: only the right triangle, none for a page that is not there (the kit's pager has no disabled state) */
    CHECK_EQ_INT(fk.pager_arrows[0], 0);
    CHECK(drawn("1/2") != NULL);
    CHECK_EQ_INT(fk.saves, 0);
    CHECK(fk.shows > 0);
}

/* an own edition: "missing" red, the card of the selected row; the first row's box switched off and saved at once */
static void test_an_own_edition(void)
{
    static const int keys[] = { ENTER };
    reset(keys, 1);
    int32_t edition = 1;
    content_screen_run(&OPS, &edition);
    CHECK_EQ_INT(edition, 1);
    CHECK_EQ_INT(fk.saves, 1);
    CHECK_EQ_INT(fk.saved_index, 1);
    CHECK_EQ_INT(fk.saved.cars, 2);
    CHECK(strcmp(fk.saved.car[0], "gone-car") == 0 && strcmp(fk.saved.car[1], "rx7") == 0);
    CHECK(drawn("MISSING") != NULL && drawn("MISSING")->style == CONTENT_RED);
    CHECK(drawn_at("Rename", 170 + CONTENT_FOOT_SHIFT, CONTENT_FOOT_Y, CONTENT_CREAM));
    CHECK(drawn("Read only. Duplicate to edit.") == NULL);
    CHECK(drawn("Your edition - in use") == NULL);   /* an own edition has no line under its name */
}

/* the card: the selected row's name, version and author, "Used in"; Update handed to the caller for the piece */
static void test_the_card(void)
{
    static const int keys[] = { RIGHT, RIGHT, RIGHT, ENTER };
    reset(keys, 4);
    int32_t edition = 1;
    content_screen_run(&OPS, &edition);
    CHECK_EQ_INT(fk.card_calls, 1);
    CHECK_EQ_INT(fk.card_button, CONTENT_CARD_UPDATE);
    CHECK_EQ_INT(fk.card_tab, CONTENT_CARS);
    CHECK(strcmp(fk.card_id, "porsche-993") == 0);
    CHECK(drawn("Porsche 993") != NULL);
    CHECK(drawn_at("Porsche 993", CONTENT_CARD_X0, CONTENT_CARD_NAME_Y, CONTENT_YELLOW));
    CHECK(drawn("v1.2 - by Ed") != NULL);
    CHECK(drawn("Used in: 2 editions") != NULL);
    CHECK(drawn_at("Update...", CONTENT_CARD_X0, CONTENT_CARD_BUTTONS_Y, CONTENT_YELLOW));
    CHECK(drawn("Previous version") != NULL && drawn("Previous version")->style == CONTENT_DIM);   /* no backup */
    CHECK(fk.previews > 0);
}

/* a mod's own words (its name, author, description) and an edition's name drawn as the face draws them right: a UTF-8 character
 * dropped whole, a control character a space, ',' kept */
static void test_mod_words(void)
{
    static const int keys[] = { RIGHT, RIGHT, RIGHT };
    reset(keys, 3);
    installed[0] = mod("porsche-993", "Porsch\xc3\xa9 993,\tRS", "1.2", "\xe2\x82\xac" "Ed", MOD_TYPE_CAR);
    snprintf(installed[0].description, sizeof installed[0].description, "%s", "Light,\nfast");
    snprintf(fk.own[0].name, sizeof fk.own[0].name, "%s", "Min\xc3\xa9");
    int32_t edition = 1;
    content_screen_run(&OPS, &edition);
    CHECK(drawn_at("Porsch 993, RS", CONTENT_NAME_X, CONTENT_LIST_Y + CONTENT_LINE, CONTENT_CREAM));
    CHECK(drawn_at("Porsch 993, RS", CONTENT_CARD_X0, CONTENT_CARD_NAME_Y, CONTENT_YELLOW));
    CHECK(drawn("v1.2 - by Ed") != NULL);
    CHECK(drawn_at("Light, fast", CONTENT_CARD_X0, CONTENT_CARD_TEXT_Y, CONTENT_CREAM));
    CHECK(drawn("Min") != NULL);
}

/* Done kept shut while no car can be raced; the selector to the standard edition, then Done */
static void test_the_rule(void)
{
    static const int keys[] = { ESCAPE, UP, UP, UP, LEFT, ESCAPE };   /* the All row, the tabs, the selector */
    reset(keys, 6);
    fk.own[0] = edition("Mine", "gone-car,old-car", "city");
    int32_t edition = 1;
    content_screen_run(&OPS, &edition);
    CHECK_EQ_INT(fk.refused, 1);
    CHECK_EQ_INT(fk.refused_lacks, CONTENT_RULES_NO_CAR);
    CHECK_EQ_INT(edition, 0);
}

/* the foot's buttons go to the caller; the selector round the editions */
/* T7: a file dropped on the window, installed by the caller (ContentOps.dropped), which may put another edition on the selector: the screen
 * reads the editions and the mods again and goes on on that edition */
static int drops;
static int32_t f_dropped(void *ctx, int32_t *edition)
{
    (void)ctx;
    if (drops++ != 0) return 0;
    *edition = 1;
    return 1;
}

static void test_a_dropped_file(void)
{
    reset(NULL, 0);   /* no key: Escape at once */
    drops = 0;
    ContentOps o = OPS;
    o.dropped = f_dropped;
    int32_t edition = 0;
    content_screen_run(&o, &edition);
    CHECK(drops >= 1);
    CHECK_EQ_INT(edition, 1);
    reset(NULL, 0);   /* no dropped op (NULL): as before */
    edition = 0;
    content_screen_run(&OPS, &edition);
    CHECK_EQ_INT(edition, 0);
}

static void test_the_foot_and_the_selector(void)
{
    /* Up round to the foot (on Done), Left to New past the grey Delete and Rename, Enter; Down round to the selector, Right */
    static const int keys[] = { UP, UP, UP, UP, LEFT, LEFT, LEFT, LEFT, LEFT, ENTER, DOWN, RIGHT };
    reset(keys, 12);
    int32_t edition = 0;
    content_screen_run(&OPS, &edition);
    CHECK_EQ_INT(fk.foot_calls, 1);
    CHECK_EQ_INT(fk.foot_button, CONTENT_FOOT_NEW);
    CHECK_EQ_INT(edition, 1);   /* Down from the foot: the selector; Right: the next edition */
}

int main(void)
{
    test_the_standard_edition();
    test_an_own_edition();
    test_the_card();
    test_mod_words();
    test_the_rule();
    test_the_foot_and_the_selector();
    test_a_dropped_file();
    return TEST_REPORT();
}
