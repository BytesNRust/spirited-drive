/* The name field (text_entry_edit, 004626d0) and a pad: a button of the menus' joystick (0043d4c0's key, 0x20 or 0xd)
 * accepts the box with the name typed so far, as Enter does, and a button already down when the box opens does
 * nothing until it has been let go (a press must be fresh: the click that opened the box must not close it). */
#include "game/frontend/text_entry.h"
#include "test.h"

#include <string.h>

enum { STEPS = 32, ENTER = 0xd, ESCAPE = 0x1b, PAD_BUTTON = 0x20 };

typedef struct {
    TextEntryState *st;
    int32_t pad[STEPS];  /* the joystick key at the loop's iteration i (0 past the end) */
    int32_t key[STEPS];  /* the key the box reads at iteration i */
    int polls, reads;
} Fake;

static int32_t width(void *c, const char *t) { (void)c; return (int32_t)strlen(t) * 8; }
static int32_t pic_new(void *c, int32_t w, int32_t h, int32_t f) { (void)c, (void)w, (void)h, (void)f; return 1; }
static int32_t pic_shape(void *c, int32_t p) { (void)c; return p; }
static void sel(void *c, int32_t p) { (void)c, (void)p; }
static void none(void *c) { (void)c; }
static void grab(void *c, int32_t s, int32_t x, int32_t y) { (void)c, (void)s, (void)x, (void)y; }
static void blit(void *c, int32_t s, int32_t x, int32_t y) { (void)c, (void)s, (void)x, (void)y; }
static void blit_own(void *c, int32_t s) { (void)c, (void)s; }
static void show(void *c, int32_t s, int32_t x, int32_t y) { (void)c, (void)s, (void)x, (void)y; }
static void style(void *c, int32_t s) { (void)c, (void)s; }
static void shadowed(void *c, const char *t, int32_t x, int32_t y) { (void)c, (void)t, (void)x, (void)y; }
static int32_t pending(void *c) { (void)c; return 0; }
static int32_t buttons(void *c) { (void)c; return 0; }
static int32_t ticks(void *c) { (void)c; return 20; }
static void pic_free(void *c, int32_t p) { (void)c, (void)p; }
static void joy(void *c)
{
    Fake *f = c;
    f->st->joystick_key = f->polls < STEPS ? f->pad[f->polls] : 0;
    f->polls++;
}
static int32_t key_read(void *c)
{
    Fake *f = c;
    const int32_t k = f->reads < STEPS ? f->key[f->reads] : ESCAPE;   /* a script that runs out cancels, so no test hangs */
    f->reads++;
    return k;
}

static int32_t run(Fake *f, char *text)
{
    TextEntryState st = { 1, 0, 0, 0 };
    f->st = &st;
    const TextEntryOps ops = { width, pic_new, pic_shape, sel, none, none, grab, none, blit, blit_own, show, style,
                               shadowed, pending, none, none, buttons, joy, key_read, ticks, pic_free, none, none, f };
    return text_entry_edit(text, 8, 0x140, 0xe6, 1, 0, &st, &ops);
}

int main(void)
{
    char text[TEXT_ENTRY_TYPED];
    Fake f;

    /* a fresh pad press with a name typed: accepted, the name kept */
    memset(&f, 0, sizeof f);
    f.pad[3] = PAD_BUTTON;
    strcpy(text, "AB");
    CHECK_EQ_INT(run(&f, text), 1);
    CHECK(strcmp(text, "AB") == 0);
    CHECK_EQ_INT(f.polls, 4);   /* on the press, not before */

    /* the button of the other kind (Enter's 0xd) the same */
    memset(&f, 0, sizeof f);
    f.pad[2] = ENTER;
    strcpy(text, "AB");
    CHECK_EQ_INT(run(&f, text), 1);

    /* a button already down as the box opens is no press: it must be let go and pressed again */
    memset(&f, 0, sizeof f);
    f.pad[0] = f.pad[1] = f.pad[2] = PAD_BUTTON;
    f.pad[6] = PAD_BUTTON;
    strcpy(text, "AB");
    CHECK_EQ_INT(run(&f, text), 1);
    CHECK_EQ_INT(f.polls, 7);

    /* with no name (no letter or digit) the press does what Enter does: nothing; the box stays up */
    memset(&f, 0, sizeof f);
    f.pad[2] = PAD_BUTTON;
    f.key[6] = ESCAPE;
    text[0] = 0;
    CHECK_EQ_INT(run(&f, text), 0);
    CHECK(f.polls >= 7);

    /* the keyboard as it was: Enter accepts, Escape cancels, a pad that is not touched changes neither */
    memset(&f, 0, sizeof f);
    f.key[2] = ENTER;
    strcpy(text, "AB");
    CHECK_EQ_INT(run(&f, text), 1);
    memset(&f, 0, sizeof f);
    f.key[2] = ESCAPE;
    strcpy(text, "AB");
    CHECK_EQ_INT(run(&f, text), 0);

    /* the pad's cancel (menu_pad_cancel: the second button gives Escape): it cancels as Escape does, the text kept */
    memset(&f, 0, sizeof f);
    f.pad[3] = ESCAPE;
    strcpy(text, "AB");
    CHECK_EQ_INT(run(&f, text), 0);
    CHECK(strcmp(text, "AB") == 0);
    CHECK_EQ_INT(f.polls, 4);
    /* and with no name too (a cancel needs no letter) */
    memset(&f, 0, sizeof f);
    f.pad[2] = ESCAPE;
    text[0] = 0;
    CHECK_EQ_INT(run(&f, text), 0);
    CHECK_EQ_INT(f.polls, 3);

    return TEST_REPORT();
}
