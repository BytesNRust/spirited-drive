/* port (dep59 step 4b, T6): the CONTENT screen's dialogs: see edition_name_box.h */
#include "game/frontend/edition_name_box.h"
#include "game/frontend/text_wrap.h"

#include <stdio.h>
#include <string.h>

const char DIALOG_THIRD_PARTY[] = "Third-party mods: at your own risk";

enum { LINE_MAX = 192, WORDS_MAX = 96, CHOICE_YES = 0, CHOICE_NO = 1, CHOICE_OK = 2 };

/* the scales a line tries, the last one also the one a cut line is drawn at */
static const int32_t SCALE[][2] = { { 1, 1 }, { 5, 6 }, { 3, 4 } };
enum { SCALES = (int)(sizeof SCALE / sizeof SCALE[0]) };

typedef struct {
    char text[LINE_MAX];
    int32_t num, den;
} Fitted;

static int fits(const ContentDialogOps *o, const char *text, const int32_t *scale)
{
    return o->text_width(o->ctx, text, scale[0], scale[1]) <= DIALOG_TEXT_W;
}

/* `head` + `cut` + `tail` at the first scale it fits at; else at the smallest with `cut` shortened and "..." after it */
static Fitted fit(const ContentDialogOps *o, const char *head, const char *cut, const char *tail)
{
    Fitted f = { "", 1, 1 };
    for (int i = 0; i < SCALES; i++) {
        snprintf(f.text, sizeof f.text, "%s%s%s", head, cut, tail);
        f.num = SCALE[i][0], f.den = SCALE[i][1];
        if (fits(o, f.text, SCALE[i])) return f;
    }
    for (int n = (int)strlen(cut) - 1; n >= 0; n--) {
        snprintf(f.text, sizeof f.text, "%s%.*s...%s", head, n, cut, tail);
        if (fits(o, f.text, SCALE[SCALES - 1])) return f;
    }
    return f;   /* the head and tail alone too wide: drawn as they are */
}

static void draw_text(const ContentDialogOps *o, int32_t style, const Fitted *f, int32_t y)
{
    o->text(o->ctx, style, f->text, DIALOG_CX - o->text_width(o->ctx, f->text, f->num, f->den) / 2, y, f->num, f->den);
}

static void draw_ground(const ContentDialogOps *o)
{
    o->backdrop(o->ctx, DIALOG_DARKEN);
    o->panel(o->ctx, DIALOG_X0, DIALOG_Y0, DIALOG_X1, DIALOG_Y1);
    o->frame(o->ctx, DIALOG_X0, DIALOG_Y0, DIALOG_X1, DIALOG_Y1, 1);
}

static int32_t button_x0(int choice) { return choice == CHOICE_YES ? DIALOG_YES_X0 : choice == CHOICE_NO ? DIALOG_NO_X0 : DIALOG_OK_X0; }

static void draw_button(const ContentDialogOps *o, int choice, int lit)
{
    static const char *const WORDS[] = { "Yes", "No", "OK" };
    int32_t x0 = button_x0(choice), x1 = x0 + DIALOG_BUTTON_W - 1;
    o->frame(o->ctx, x0, DIALOG_BUTTON_Y0, x1, DIALOG_BUTTON_Y1, lit);
    Fitted f = { "", 1, 1 };
    snprintf(f.text, sizeof f.text, "%s", WORDS[choice]);
    o->text(o->ctx, lit ? DIALOG_YELLOW : DIALOG_CREAM, f.text, x0 + (DIALOG_BUTTON_W - o->text_width(o->ctx, f.text, 1, 1)) / 2,
            DIALOG_BUTTON_Y0 + 6, 1, 1);
}

/* the button under (x, y), -1 for none */
static int button_at(const int *choices, int count, int32_t x, int32_t y)
{
    if (y < DIALOG_BUTTON_Y0 || y > DIALOG_BUTTON_Y1) return -1;
    for (int i = 0; i < count; i++)
        if (x >= button_x0(choices[i]) && x < button_x0(choices[i]) + DIALOG_BUTTON_W) return i;
    return -1;
}

typedef struct {
    const ContentDialogOps *o;
    Fitted line, question, note;   /* question empty: all on one line */
    int choices[2], count;
} Prompt;

static void draw_prompt(const Prompt *p, int at)
{
    draw_ground(p->o);
    const int two = p->question.text[0] != '\0';
    draw_text(p->o, DIALOG_CREAM, &p->line, two ? DIALOG_TWO_Y0 : DIALOG_LINE_Y);
    if (two) draw_text(p->o, DIALOG_CREAM, &p->question, DIALOG_TWO_Y1);
    draw_text(p->o, DIALOG_DIM, &p->note, two ? DIALOG_TWO_NOTE_Y : DIALOG_NOTE_Y);
    for (int i = 0; i < p->count; i++) draw_button(p->o, p->choices[i], i == at);
    p->o->show(p->o->ctx);
}

/* the key's answer: the button taken (its index), -1 for Escape, -2 for none; `at` moved by the arrows */
static int prompt_key(const Prompt *p, int32_t key, int *at)
{
    if (key == DIALOG_KEY_ENTER) return *at;
    if (key == DIALOG_KEY_ESCAPE) return -1;
    if ((key == DIALOG_KEY_LEFT || key == DIALOG_KEY_RIGHT) && p->count > 1) *at = (*at + 1) % p->count;
    if (p->count > 1 && (key == 'y' || key == 'Y')) return 0;
    if (p->count > 1 && (key == 'n' || key == 'N')) return 1;
    return -2;
}

/* the prompt's loop: a key on its press, a click on a button on its press, the pointer over a button lighting it. A key or the button
 * held as it opens (the press that opened it) counts only once let go: last_key and last_held start as held */
static int run_prompt(const Prompt *p)
{
    int at = 0, taken = -2;
    int32_t last_key = -1, last_held = 1;
    draw_prompt(p, at);
    while (taken == -2) {
        p->o->wait(p->o->ctx);
        int was = at;
        int32_t key = p->o->key(p->o->ctx), x = -1, y = -1;
        int32_t held = p->o->pointer(p->o->ctx, &x, &y) & 1;
        int over = button_at(p->choices, p->count, x, y);
        if (key != 0 && key != last_key && last_key != -1) taken = prompt_key(p, key, &at);
        if (last_key == -1 && key != 0) key = -1;   /* still the key held as it opened */
        if (over >= 0) at = over;
        if (taken == -2 && held && !last_held && over >= 0) taken = over;
        last_key = key, last_held = held;
        if (at != was && taken == -2) draw_prompt(p, at);
    }
    p->o->close(p->o->ctx);
    return taken < 0 ? -1 : p->choices[taken];
}

/* the prompt's words: on one line when it fits at 1/1 or 5/6 (fit's first two scales), else the install's and the question's apart */
static void prompt_words(Prompt *p, const char *name, const char *version, const char *edition)
{
    char n[WORDS_MAX], v[WORDS_MAX], e[WORDS_MAX], one[LINE_MAX];
    text_words(name ? name : "", n, sizeof n);
    text_words(version ? version : "", v, sizeof v);
    text_words(edition ? edition : "", e, sizeof e);
    snprintf(one, sizeof one, " %s - Add to %s?", v, e);
    p->line = fit(p->o, "Installed: ", n, one);
    p->question.text[0] = '\0';
    if (edition && p->line.num * 6 >= p->line.den * 5 && fits(p->o, p->line.text, (const int32_t[2]){ p->line.num, p->line.den })) return;
    snprintf(one, sizeof one, " %s", v);
    p->line = fit(p->o, "Installed: ", n, one);
    if (!edition) return;
    snprintf(one, sizeof one, "Add to %s?", e);
    p->question = fit(p->o, "", one, "");
}

int32_t content_install_prompt(const ContentDialogOps *o, const char *name, const char *version, const char *edition)
{
    Prompt p = { o, { "", 1, 1 }, { "", 1, 1 }, fit(o, "", DIALOG_THIRD_PARTY, ""), { CHOICE_YES, CHOICE_NO }, 2 };
    prompt_words(&p, name, version, edition);
    if (!edition) p.choices[0] = CHOICE_OK, p.count = 1;
    const int choice = run_prompt(&p);
    return edition ? choice == CHOICE_YES : 1;
}

/* `s` without its spaces at either end, in place */
static void trim(char *s)
{
    size_t start = strspn(s, " "), end = strlen(s);
    while (end > start && s[end - 1] == ' ') end--;
    memmove(s, s + start, end - start);
    s[end - start] = '\0';
}

int32_t edition_name_box(const ContentDialogOps *o, const char *title, char *name)
{
    char field[EDITION_NAME_MAX + 1], clean[EDITION_NAME_MAX + 1];
    snprintf(field, sizeof field, "%s", name);
    draw_ground(o);
    Fitted t = fit(o, "", title ? title : "", "");
    draw_text(o, DIALOG_YELLOW, &t, DIALOG_LINE_Y);
    o->show(o->ctx);
    int32_t taken = o->text_field(o->ctx, field, EDITION_NAME_MAX, DIALOG_CX, DIALOG_FIELD_Y);
    o->close(o->ctx);
    if (!taken) return 0;
    text_words(field, clean, sizeof clean);
    trim(clean);
    if (clean[0] == '\0') return 0;
    snprintf(name, EDITION_NAME_MAX + 1, "%s", clean);
    return 1;
}
