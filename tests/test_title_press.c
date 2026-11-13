/* port (fresh press, title.c): the title screen takes a key or a button only once it was seen up there. The press that stopped EA's movie, still
 * held (or latched) as the title opens, leaves it its own time; a press made after it opened ends it. The owner's report: one press on EA's logo
 * also skipped the title. */
#include "game/frontend/title.h"
#include "test.h"

#include <string.h>

enum { LOOKS = 64 };
static struct {
    int key[LOOKS], buttons[LOOKS];   /* what is held at each look */
    int looks, polls, deadline_at;     /* the deadline passes at that look */
} f;

static void deadline_set(void *ctx, int32_t t) { (void)ctx, (void)t; }
static void screen_show(void *ctx, const char *file, int32_t mode, int32_t one) { (void)ctx, (void)file, (void)mode, (void)one; }
static void banner_wait(void *ctx, const char *text, int32_t t) { (void)ctx, (void)text, (void)t; }
static int32_t pointer_poll(void *ctx) { (void)ctx; return f.buttons[f.polls < LOOKS ? f.polls++ : LOOKS - 1]; }
static void music_service(void *ctx) { (void)ctx; }
static int32_t deadline_passed(void *ctx) { (void)ctx; return f.polls > f.deadline_at; }
static int32_t key(void *ctx) { (void)ctx; return f.key[f.looks < LOOKS ? f.looks++ : LOOKS - 1]; }
static void fade_out(void *ctx, int32_t first, int32_t count) { (void)ctx, (void)first, (void)count; }
static const TitleOps OPS = { deadline_set, screen_show, banner_wait, pointer_poll, music_service, deadline_passed, key, fade_out, NULL };

/* the looks the title took before it ended */
static int run(void)
{
    TitleState s = { 0, 0, 0, 0 };
    DialogPointer p = { 1, 0, 0, 0, 0, 0 };
    title_screen_show(1, &s, &p, &OPS);
    return f.polls;
}

static void reset(int deadline_at)
{
    memset(&f, 0, sizeof f);
    f.deadline_at = deadline_at;
}

/* Enter held from the movie for 5 looks, never pressed again: the title stays until its deadline */
static void test_held_key_waits_for_the_deadline(void)
{
    reset(40);
    for (int i = 0; i < 5; i++) f.key[i] = 0x1c;
    CHECK_EQ_INT(run(), 41);
}

/* the same with the left button */
static void test_held_button_waits_for_the_deadline(void)
{
    reset(40);
    for (int i = 0; i < 5; i++) f.buttons[i] = 1;
    CHECK_EQ_INT(run(), 41);
}

/* let go, then pressed again on the title: that press ends it */
static void test_new_key_ends_it(void)
{
    reset(40);
    for (int i = 0; i < 5; i++) f.key[i] = 0x1c;
    f.key[10] = 0x1c;
    CHECK_EQ_INT(run(), 11);
}

static void test_new_button_ends_it(void)
{
    reset(40);
    for (int i = 0; i < 5; i++) f.buttons[i] = 1;
    f.buttons[10] = 1;
    CHECK_EQ_INT(run(), 11);
}

/* a key held over does not hold back a button pressed fresh, nor the other way */
static void test_sources_apart(void)
{
    reset(40);
    for (int i = 0; i < 20; i++) f.key[i] = 0x1c;
    f.buttons[7] = 1;
    CHECK_EQ_INT(run(), 8);
}

/* nothing held as it opens: the first press ends it, as before */
static void test_first_press_counts(void)
{
    reset(40);
    f.key[3] = 0x39;
    CHECK_EQ_INT(run(), 4);
}

int main(void)
{
    test_held_key_waits_for_the_deadline();
    test_held_button_waits_for_the_deadline();
    test_new_key_ends_it();
    test_new_button_ends_it();
    test_sources_apart();
    test_first_press_counts();
    return TEST_REPORT();
}
