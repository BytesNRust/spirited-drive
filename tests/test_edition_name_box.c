/* dep59 step 4b, T6: the CONTENT screen's dialogs (game/frontend/edition_name_box.h): the install prompt's words, buttons, keys and clicks;
 * the standard edition's notice; the edition's name box: the field's start and size, the name cleaned and trimmed, Escape and an empty
 * name leaving it as it was */
#include "game/frontend/edition_name_box.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { DRAWN_MAX = 64, FRAMES_MAX = 2000 };

typedef struct {
    char text[160];
    int style, x, y, num, den;
} Drawn;

static struct {
    Drawn drawn[DRAWN_MAX];
    int drawn_count, darkened, panels, frames_lit, closed, shows, frames;
    const int *keys;
    int key_count, key_next, gap, held;   /* held: the keys one after another with no frame of none between them */
    int click_x, click_y, click_at;   /* a click there once `click_at` frames have gone; -1 none */
    char field_start[64], field_gives[64];
    int field_max, field_x, field_result, field_calls;
} fk;

static void f_backdrop(void *ctx, int32_t percent) { (void)ctx, fk.darkened = percent, fk.drawn_count = 0, fk.frames_lit = 0; }
static void f_panel(void *ctx, int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    (void)ctx;
    fk.panels += x0 == DIALOG_X0 && y0 == DIALOG_Y0 && x1 == DIALOG_X1 && y1 == DIALOG_Y1;
}
static void f_frame(void *ctx, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t lit) { (void)ctx, (void)x0, (void)y0, (void)x1, (void)y1, fk.frames_lit += lit != 0; }
static void f_text(void *ctx, int32_t style, const char *text, int32_t x, int32_t y, int32_t num, int32_t den)
{
    (void)ctx;
    if (fk.drawn_count >= DRAWN_MAX) return;
    Drawn *d = &fk.drawn[fk.drawn_count++];
    snprintf(d->text, sizeof d->text, "%s", text);
    d->style = style, d->x = x, d->y = y, d->num = num, d->den = den;
}
static int32_t f_width(void *ctx, const char *text, int32_t num, int32_t den) { (void)ctx; return (int32_t)strlen(text) * 10 * num / den; }
static int32_t f_field(void *ctx, char *text, int32_t max, int32_t x, int32_t y)
{
    (void)ctx, (void)y;
    fk.field_calls++, fk.field_max = max, fk.field_x = x;
    snprintf(fk.field_start, sizeof fk.field_start, "%s", text);
    if (fk.field_result) snprintf(text, (size_t)max + 1, "%s", fk.field_gives);
    return fk.field_result;
}
static void f_show(void *ctx) { (void)ctx, fk.shows++; }
static int32_t f_key(void *ctx)
{
    (void)ctx;
    if (fk.gap) return fk.gap = 0, 0;
    fk.gap = !fk.held;
    if (fk.click_at >= 0 && fk.frames < fk.click_at + 3) return 0;   /* no keys until the click is over */
    return fk.key_next < fk.key_count ? fk.keys[fk.key_next++] : DIALOG_KEY_ESCAPE;
}
static int32_t f_pointer(void *ctx, int32_t *x, int32_t *y)
{
    (void)ctx;
    *x = fk.click_x, *y = fk.click_y;
    return fk.click_at >= 0 && fk.frames >= fk.click_at && fk.frames < fk.click_at + 3;
}
static void f_wait(void *ctx)
{
    (void)ctx;
    if (++fk.frames > FRAMES_MAX) fprintf(stderr, "test_edition_name_box: the dialog never closed\n"), exit(1);
}
static void f_close(void *ctx) { (void)ctx, fk.closed++; }

static const ContentDialogOps OPS = { f_backdrop, f_panel, f_frame, f_text, f_width, f_field, f_show, f_key, f_pointer, f_wait, f_close, NULL };

static void reset(const int *keys, int n)
{
    memset(&fk, 0, sizeof fk);
    fk.keys = keys, fk.key_count = n, fk.gap = 1, fk.click_at = -1, fk.click_x = fk.click_y = -1;
}

static const Drawn *drawn(const char *text)
{
    for (int i = 0; i < fk.drawn_count; i++)
        if (strcmp(fk.drawn[i].text, text) == 0) return &fk.drawn[i];
    return NULL;
}

static void test_the_prompt(void)
{
    static const int enter[] = { DIALOG_KEY_ENTER };
    reset(enter, 1);
    CHECK_EQ_INT(content_install_prompt(&OPS, "Ferrari F40", "1.2", "Europa 80"), 1);   /* Yes is where it starts */
    CHECK_EQ_INT(fk.darkened, DIALOG_DARKEN);
    CHECK(fk.panels > 0);
    /* too wide for one line at 1/1 or 5/6: two, the install's and the question */
    const Drawn *line = drawn("Installed: Ferrari F40 1.2"), *question = drawn("Add to Europa 80?");
    CHECK(line != NULL && line->y == DIALOG_TWO_Y0 && line->num == line->den);
    CHECK(question != NULL && question->y == DIALOG_TWO_Y1);
    CHECK(line != NULL && line->x == DIALOG_CX - f_width(NULL, line->text, line->num, line->den) / 2);   /* centred */
    CHECK(drawn(DIALOG_THIRD_PARTY) != NULL && drawn(DIALOG_THIRD_PARTY)->y == DIALOG_TWO_NOTE_Y);
    CHECK(drawn(DIALOG_THIRD_PARTY) != NULL && drawn(DIALOG_THIRD_PARTY)->style == DIALOG_DIM);
    CHECK(drawn("Yes") != NULL && drawn("Yes")->style == DIALOG_YELLOW);
    CHECK(drawn("No") != NULL && drawn("No")->style == DIALOG_CREAM);
    CHECK_EQ_INT(fk.frames_lit, 2);   /* the panel's frame and Yes's */
    CHECK_EQ_INT(fk.closed, 1);
    static const int right_enter[] = { DIALOG_KEY_RIGHT, DIALOG_KEY_ENTER };
    reset(right_enter, 2);
    CHECK_EQ_INT(content_install_prompt(&OPS, "Ferrari F40", "1.2", "Europa 80"), 0);
    CHECK(drawn("No") != NULL && drawn("No")->style == DIALOG_YELLOW);
    reset(enter, 1);   /* short enough: one line */
    content_install_prompt(&OPS, "F40", "1", "E");
    CHECK(drawn("Installed: F40 1 - Add to E?") != NULL && drawn("Installed: F40 1 - Add to E?")->y == DIALOG_LINE_Y);
    CHECK(drawn(DIALOG_THIRD_PARTY) != NULL && drawn(DIALOG_THIRD_PARTY)->y == DIALOG_NOTE_Y);
    static const int round[] = { DIALOG_KEY_LEFT, DIALOG_KEY_LEFT, DIALOG_KEY_ENTER };   /* Left from Yes goes round to No, and back */
    reset(round, 3);
    CHECK_EQ_INT(content_install_prompt(&OPS, "F40", "1", "E"), 1);
    reset(NULL, 0);   /* Escape: No */
    CHECK_EQ_INT(content_install_prompt(&OPS, "F40", "1", "E"), 0);
    static const int letters[] = { 'n' };
    reset(letters, 1);
    CHECK_EQ_INT(content_install_prompt(&OPS, "F40", "1", "E"), 0);
    static const int yes[] = { 'Y' };
    reset(yes, 1);
    CHECK_EQ_INT(content_install_prompt(&OPS, "F40", "1", "E"), 1);
    static const int held_right[] = { DIALOG_KEY_RIGHT, DIALOG_KEY_RIGHT, DIALOG_KEY_ENTER };
    reset(held_right, 3);   /* Right held two frames moves once: No */
    fk.held = 1;
    CHECK_EQ_INT(content_install_prompt(&OPS, "F40", "1", "E"), 0);
    static const int still_held[] = { DIALOG_KEY_ENTER, 0, DIALOG_KEY_RIGHT, 0, DIALOG_KEY_ENTER };
    reset(still_held, 5);   /* the Enter still held from the screen before counts only once let go: Right, Enter take No */
    fk.held = 1, fk.gap = 0;
    CHECK_EQ_INT(content_install_prompt(&OPS, "F40", "1", "E"), 0);
    reset(NULL, 0);   /* the button still held as it opens, over Yes: not a click; then Escape */
    fk.click_at = 0, fk.click_x = DIALOG_YES_X0 + 10, fk.click_y = DIALOG_BUTTON_Y0 + 10;
    CHECK_EQ_INT(content_install_prompt(&OPS, "F40", "1", "E"), 0);
    CHECK_EQ_INT(fk.closed, 1);
    /* the pointer: a click on No, a click on Yes */
    reset(NULL, 0);
    fk.click_at = 4, fk.click_x = DIALOG_NO_X0 + 10, fk.click_y = DIALOG_BUTTON_Y0 + 10;
    CHECK_EQ_INT(content_install_prompt(&OPS, "F40", "1", "E"), 0);
    reset(NULL, 0);
    fk.click_at = 4, fk.click_x = DIALOG_YES_X0 + 10, fk.click_y = DIALOG_BUTTON_Y0 + 10;
    CHECK_EQ_INT(content_install_prompt(&OPS, "F40", "1", "E"), 1);
    static const int then_enter[] = { DIALOG_KEY_ENTER };   /* a click off the buttons does nothing: Enter takes Yes after it */
    reset(then_enter, 1);
    fk.click_at = 4, fk.click_x = 20, fk.click_y = 20;
    CHECK_EQ_INT(content_install_prompt(&OPS, "F40", "1", "E"), 1);
    /* the pointer over No lights it: Enter then takes No */
    reset(then_enter, 1);
    fk.click_x = DIALOG_NO_X0 + 1, fk.click_y = DIALOG_BUTTON_Y1;
    /* never held, only there */
    CHECK_EQ_INT(content_install_prompt(&OPS, "F40", "1", "E"), 0);
    CHECK(drawn("No") != NULL && drawn("No")->style == DIALOG_YELLOW);
}

static void test_the_words(void)
{
    static const int enter[] = { DIALOG_KEY_ENTER };
    /* a mod's own words as the main face draws them right (text_words): a UTF-8 character dropped, ',' kept */
    reset(enter, 1);
    content_install_prompt(&OPS, "Caf\xc3\xa9, Racer", "2.0\t", "Min\xc3\xa9");
    CHECK(drawn("Installed: Caf, Racer 2.0  - Add to Min?") != NULL);   /* one line at 5/6 */
    /* a name too long even on a line of its own: drawn smaller, then cut ("..."), never wider than the panel's text */
    reset(enter, 1);
    content_install_prompt(&OPS, "A name far too long to be shown on one line at all", "10.0.1", "Edition name");
    const Drawn *cut = NULL;
    for (int i = 0; i < fk.drawn_count; i++)
        if (fk.drawn[i].y == DIALOG_TWO_Y0) cut = &fk.drawn[i];
    CHECK(cut != NULL && f_width(NULL, cut->text, cut->num, cut->den) <= DIALOG_TEXT_W);
    CHECK(cut != NULL && strncmp(cut->text, "Installed: A name far", 21) == 0 && strstr(cut->text, "... 10.0.1") != NULL);
    CHECK(cut != NULL && cut->num < cut->den);
    CHECK(drawn("Add to Edition name?") != NULL);
    /* the standard edition: nothing asked, the line and OK */
    reset(enter, 1);
    CHECK_EQ_INT(content_install_prompt(&OPS, "Ferrari F40", "1.2", NULL), 1);
    CHECK(drawn("Installed: Ferrari F40 1.2") != NULL);
    CHECK(drawn(DIALOG_THIRD_PARTY) != NULL);
    CHECK(f_width(NULL, DIALOG_THIRD_PARTY, drawn(DIALOG_THIRD_PARTY)->num, drawn(DIALOG_THIRD_PARTY)->den) <= DIALOG_TEXT_W);
    CHECK(drawn("OK") != NULL && drawn("Yes") == NULL && drawn("No") == NULL);
    reset(NULL, 0);
    CHECK_EQ_INT(content_install_prompt(&OPS, "Ferrari F40", "1.2", NULL), 1);   /* Escape takes it too */
}

static void test_the_name_box(void)
{
    char name[EDITION_NAME_MAX + 1] = "Europa 80";
    reset(NULL, 0);
    fk.field_result = 1;
    snprintf(fk.field_gives, sizeof fk.field_gives, "%s", "  Mi\xc3\xb1" "a Ed ");
    CHECK_EQ_INT(edition_name_box(&OPS, "Rename the edition", name), 1);
    CHECK(strcmp(fk.field_start, "Europa 80") == 0);   /* the field starts on the name */
    CHECK_EQ_INT(fk.field_max, EDITION_NAME_MAX);
    CHECK_EQ_INT(fk.field_x, DIALOG_CX);
    CHECK(strcmp(name, "Mia Ed") == 0);                /* cleaned (the n with a tilde dropped) and trimmed */
    CHECK(drawn("Rename the edition") != NULL && drawn("Rename the edition")->y == DIALOG_LINE_Y);
    CHECK_EQ_INT(fk.darkened, DIALOG_DARKEN);
    CHECK_EQ_INT(fk.closed, 1);
    reset(NULL, 0);   /* Escape: as it was */
    fk.field_result = 0;
    CHECK_EQ_INT(edition_name_box(&OPS, "Rename the edition", name), 0);
    CHECK(strcmp(name, "Mia Ed") == 0);
    reset(NULL, 0);   /* nothing left once cleaned: as it was */
    fk.field_result = 1;
    snprintf(fk.field_gives, sizeof fk.field_gives, "%s", " \xc3\xa9\xc3\xa9 ");
    CHECK_EQ_INT(edition_name_box(&OPS, "Name the new edition", name), 0);
    CHECK(strcmp(name, "Mia Ed") == 0);
    reset(NULL, 0);   /* a new edition: the field starts empty when the name is */
    char fresh[EDITION_NAME_MAX + 1] = "";
    fk.field_result = 1;
    snprintf(fk.field_gives, sizeof fk.field_gives, "%s", "Twelve chars");
    CHECK_EQ_INT(edition_name_box(&OPS, "Name the new edition", fresh), 1);
    CHECK(strcmp(fk.field_start, "") == 0);
    CHECK(strcmp(fresh, "Twelve chars") == 0);
}

int main(void)
{
    test_the_prompt();
    test_the_words();
    test_the_name_box();
    return TEST_REPORT();
}
