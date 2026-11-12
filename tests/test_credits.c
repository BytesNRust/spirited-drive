/* The credits' five pages (004166e0): one click is one page. The port reads the pointer's button before it pumps the events, and nothing pumps
 * them during a page's fades, so the click that left a page was read again, still down, as the next one began: two pages for one click (the owner's
 * bug). With the port's switch a page counts the buttons only once it has seen them up; without it, as the exe, they stay counted from page to page. */
#include "game/frontend/credits.h"
#include "test.h"

#include <string.h>

typedef struct {
    const int32_t *buttons;   /* what each read of the pointer gives, 0 after the list */
    int reads, count;
    int pages;                /* the fades in: one a page */
    int pages_while_scripted; /* the pages begun before the list was spent */
    int32_t *idle;
} Fake;

static uint8_t pal[0x400];

static int32_t load_art(void *ctx, const char *name) { (void)ctx, (void)name; return 1; }
static const uint8_t *shape(void *ctx, int32_t bank, const char *tag) { (void)ctx, (void)bank, (void)tag; return pal; }
static void palette_set(void *ctx, uint8_t *rgb, int32_t first, int32_t count, const uint8_t *src, int32_t mode)
{
    (void)ctx, (void)first, (void)count, (void)src, (void)mode;
    memset(rgb, 0, CREDITS_PALETTE);
}
static void none(void *ctx) { (void)ctx; }
static void select_picture(void *ctx, int32_t picture) { (void)ctx, (void)picture; }
static void blit_own(void *ctx, const uint8_t *s) { (void)ctx, (void)s; }
static int32_t picture_shape(void *ctx, int32_t picture) { (void)ctx, (void)picture; return 0; }
static void blit_at(void *ctx, int32_t s, int32_t x, int32_t y) { (void)ctx, (void)s, (void)x, (void)y; }
static void fade_in(void *ctx, int32_t first, int32_t count, const uint8_t *rgb)
{
    (void)first, (void)count, (void)rgb;
    Fake *f = ctx;
    f->pages++;
    if (f->reads < f->count) f->pages_while_scripted = f->pages;
}
static void music(void *ctx)
{
    Fake *f = ctx;
    if (f->reads >= f->count) *f->idle = CREDITS_PAGE_TICKS;   /* the list spent: the time takes the rest */
}
static int32_t pointer_read(void *ctx)
{
    Fake *f = ctx;
    const int r = f->reads++;
    return r < f->count ? f->buttons[r] : 0;
}
static int32_t key(void *ctx) { (void)ctx; return 0; }
static void fade_out(void *ctx, int32_t first, int32_t count) { (void)ctx, (void)first, (void)count; }
static void free_bank(void *ctx, int32_t bank) { (void)ctx, (void)bank; }

/* the pages begun while the pointer gave `buttons` */
static int pages_for(const int32_t *buttons, int count, int32_t port)
{
    int32_t idle = 0, screen = 0;
    int16_t active = 1, held = 0, quit = 0;
    Fake f = { buttons, 0, count, 0, 0, &idle };
    const CreditsOps ops = { load_art, shape, palette_set, none, select_picture, blit_own, none, picture_shape, blit_at, none, none, fade_in, music,
                             pointer_read, key, fade_out, none, none, none, free_bank, none, &f, NULL };
    const CreditsState st = { &screen, &active, &held, &quit, &idle, port };
    credits_show(&st, &ops);
    CHECK_EQ_INT(f.pages, 5);
    return f.pages_while_scripted;
}

/* up, the click (down), and the next page's first read still down, its release not pumped yet; then up */
static const int32_t CLICK[] = { 0, 0, 1, 1, 0, 0, 0, 0 };
enum { CLICK_READS = sizeof CLICK / sizeof CLICK[0] };

static void one_click_is_one_page(void)
{
    CHECK_EQ_INT(pages_for(CLICK, CLICK_READS, 1), 2);
}

static void the_exe_counted_it_twice(void)
{
    CHECK_EQ_INT(pages_for(CLICK, CLICK_READS, 0), 3);
}

static void two_clicks_are_two_pages(void)
{
    static const int32_t TWO[] = { 0, 1, 1, 0, 0, 1, 1, 0, 0 };
    CHECK_EQ_INT(pages_for(TWO, sizeof TWO / sizeof TWO[0], 1), 3);
}

int main(void)
{
    one_click_is_one_page();
    the_exe_counted_it_twice();
    two_clicks_are_two_pages();
    return TEST_REPORT();
}
