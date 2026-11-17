/* port (dep59): an added car's gallery page run (game/frontend/added_gallery_run.h) over recording ops: the showcase's music serviced every
 * frame the page runs, the let-go wait at its end too (owner 2026-11-17: the gallery's music stopped on an added car's page); the page
 * comes up as the showcase's does (both pictures cleared, the palette black, drawn, flipped, faded in) and goes out faded, whatever way
 * it is left (owner 2026-11-17: the fade between a stock and an added page was sometimes broken). */
#include "game/frontend/added_gallery_run.h"
#include "game/sys/pointer.h"
#include "test.h"

#include <string.h>

static const int *keys_in;   /* the key each frame, the last one kept */
static int key_reads, frames, services, services_at_wait[64], shows;

static char ops_seen[64];   /* the screen's ops in turn: C clear, B black, D draw, P palette, F flip, I fade in, O fade out */
static int ops_count;
static void op(char c)
{
    if (ops_count < (int)sizeof ops_seen - 1) ops_seen[ops_count++] = c, ops_seen[ops_count] = 0;
}
static void clear(void *ctx) { (void)ctx, op('C'); }
static void black(void *ctx) { (void)ctx, op('B'); }
static void draw(void *ctx, const GalleryState *s) { (void)ctx, (void)s, op('D'), shows++; }
static void palette(void *ctx) { (void)ctx, op('P'); }
static void flip(void *ctx, const GalleryState *s) { (void)ctx, (void)s, op('F'); }
static void fade_in(void *ctx) { (void)ctx, op('I'); }
static void fade_out(void *ctx) { (void)ctx, op('O'); }
static int key(void *ctx)
{
    (void)ctx;
    const int k = keys_in[key_reads];
    if (keys_in[key_reads + 1] != -1) key_reads++;
    return k;
}
static int buttons(void *ctx) { (void)ctx; return POINTER_BUTTON_NONE; }
static int click(void *ctx, GalleryState *s) { (void)ctx, (void)s; return 0; }
static void music(void *ctx) { (void)ctx, services++; }
static void wait(void *ctx)
{
    (void)ctx;
    if (frames < 64) services_at_wait[frames] = services;
    frames++;
}

static const GalleryRunOps OPS = { clear, black, draw, palette, flip, fade_in, fade_out, key, buttons, click, music, wait, NULL };

static int run(const int *keys, GalleryState *s)
{
    CarPieceCard c;
    memset(&c, 0, sizeof c);
    c.skins = 1;
    keys_in = keys, key_reads = frames = services = shows = ops_count = 0;
    ops_seen[0] = 0;
    memset(services_at_wait, 0, sizeof services_at_wait);
    return added_gallery_run(&c, s, &OPS);
}

static void test_the_music_plays_on(void)
{
    /* three frames with no key, Enter on Next, then Enter held two frames (the let-go wait), then nothing */
    static const int KEYS[] = { 0, 0, 0, GALLERY_KEY_ENTER, GALLERY_KEY_ENTER, GALLERY_KEY_ENTER, 0, -1 };
    GalleryState s = { GALLERY_NEXT, 0, 0, 0 };
    CHECK_EQ_INT(run(KEYS, &s), GALLERY_GO_NEXT);
    CHECK(frames >= 6);
    int each = 1;   /* by every frame's end the music was serviced once more: the page's frames and the let-go's */
    for (int i = 0; i < frames && i < 64; i++) each &= services_at_wait[i] == i + 1;
    CHECK(each);
}

static void test_the_fades(void)
{
    /* opened, Right on Skin (a skin more: drawn again, its palette at once), Escape */
    static const int KEYS[] = { 0, GALLERY_KEY_RIGHT, 0, GALLERY_KEY_ESCAPE, 0, -1 };
    GalleryState s = { GALLERY_SKIN, 0, 0, 0 };
    CarPieceCard c;
    memset(&c, 0, sizeof c);
    c.skins = 2;
    keys_in = KEYS, key_reads = frames = services = shows = ops_count = 0;
    ops_seen[0] = 0;
    CHECK_EQ_INT(added_gallery_run(&c, &s, &OPS), GALLERY_LEAVE);
    CHECK(strcmp(ops_seen, "CBDFIDPFO") == 0);
    {   /* Next taken: faded out too */
        static const int NEXT[] = { GALLERY_KEY_ENTER, 0, -1 };
        GalleryState n = { GALLERY_NEXT, 0, 0, 0 };
        CHECK_EQ_INT(run(NEXT, &n), GALLERY_GO_NEXT);
        CHECK(strcmp(ops_seen, "CBDFIO") == 0);
    }
}

int main(void)
{
    test_the_music_plays_on();
    test_the_fades();
    return TEST_REPORT();
}
