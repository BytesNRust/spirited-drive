/* port (dep59): an added car's gallery page run (game/frontend/added_gallery_run.h) over recording ops: the showcase's music serviced every
 * frame the page runs, the let-go wait at its end too (owner 2026-11-17: the gallery's music stopped on an added car's page); the page
 * comes up as the showcase's does (both pictures cleared, the palette black, drawn, flipped, faded in) and goes out faded, whatever way
 * it is left (owner 2026-11-17: the fade between a stock and an added page was sometimes broken). */
#include "game/frontend/added_gallery_run.h"
#include "game/sys/pointer.h"
#include "test.h"

#include <string.h>

static const int *keys_in;   /* the key each frame, the last one kept */
static int key_reads, frames, services, services_at_wait[64], shows, drawn_item = -1;

static char ops_seen[64];   /* the screen's ops in turn: C clear, B black, D draw, P palette, F flip, I fade in, O fade out */
static int ops_count;
static void op(char c)
{
    if (ops_count < (int)sizeof ops_seen - 1) ops_seen[ops_count++] = c, ops_seen[ops_count] = 0;
}
static void clear(void *ctx) { (void)ctx, op('C'); }
static void black(void *ctx) { (void)ctx, op('B'); }
static int drawn_scroll;
static void draw(void *ctx, const GalleryState *s) { (void)ctx, op('D'), shows++, drawn_item = s->item, drawn_scroll = s->scroll; }
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
static const int (*places)[2];   /* the pointer's place each frame (read once a frame), the last one kept; NULL: (0, 0) */
static int place_reads;
static void pointer(void *ctx, int *x, int *y)
{
    (void)ctx;
    *x = *y = 0;
    if (places == NULL) return;
    *x = places[place_reads][0], *y = places[place_reads][1];
    if (places[place_reads + 1][0] >= 0) place_reads++;
}
static int click(void *ctx, GalleryState *s) { (void)ctx, (void)s; return 0; }
static void music(void *ctx) { (void)ctx, services++; }
static unsigned fake_ms, step_ms;   /* the clock: step_ms more at each frame's end */
static void wait(void *ctx)
{
    (void)ctx;
    fake_ms += step_ms;
    if (frames < 64) services_at_wait[frames] = services;
    frames++;
}

static int wheel_in, lines_in, visible_in;
static int wheel(void *ctx) { (void)ctx; const int w = wheel_in; wheel_in = 0; return w; }
static unsigned now(void *ctx) { (void)ctx; return fake_ms; }
static int lines(void *ctx, int *visible) { (void)ctx; *visible = visible_in; return lines_in; }

static int applied;   /* how many times the settings were kept, and the last transmission */
static void apply(void *ctx, const GalleryState *s) { (void)ctx; applied = 10 * applied + 1 + s->transmission; }

static int shutters;   /* the panel's openings and closings: opening +1, closing +10 */
static void shutter(void *ctx, const GalleryState *s, int opening) { (void)ctx, (void)s; shutters += opening ? 1 : 10; }

static const GalleryRunOps OPS = { clear, black, draw, palette, flip, fade_in, fade_out, key, buttons, pointer, click, music, wait, wheel, now,
                                   lines, apply, shutter, NULL };

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
    GalleryState s = { GALLERY_NEXT, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    CHECK_EQ_INT(run(KEYS, &s), GALLERY_GO_NEXT);
    CHECK(frames >= 6);
    int each = 1;   /* by every frame's end the music was serviced once more: the page's frames and the let-go's */
    for (int i = 0; i < frames && i < 64; i++) each &= services_at_wait[i] == i + 1;
    CHECK(each);
}

static void test_the_fades(void)
{
    /* opened, Enter on SETUP (the panel open: drawn again, its palette at once), Escape (it closes: drawn again), Escape (it leaves) */
    static const int KEYS[] = { 0, GALLERY_KEY_ENTER, 0, GALLERY_KEY_ESCAPE, 0, GALLERY_KEY_ESCAPE, 0, -1 };
    GalleryState s = { GALLERY_SETUP, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    CarPieceCard c;
    memset(&c, 0, sizeof c);
    c.skins = 2;
    keys_in = KEYS, key_reads = frames = services = shows = ops_count = 0;
    ops_seen[0] = 0;
    CHECK_EQ_INT(added_gallery_run(&c, &s, &OPS), GALLERY_LEAVE);
    CHECK(strcmp(ops_seen, "CBDFIDPFDPFO") == 0);
    {   /* Next taken: faded out too */
        static const int NEXT[] = { GALLERY_KEY_ENTER, 0, -1 };
        GalleryState n = { GALLERY_NEXT, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
        CHECK_EQ_INT(run(NEXT, &n), GALLERY_GO_NEXT);
        CHECK(strcmp(ops_seen, "CBDFIO") == 0);
    }
}

/* owner 2026-11-17: the item under the pointer lit as the showcase lights its own (the CD's boxes), each frame the pointer moves; the
 * keys' item kept while it stays */
static void test_the_pointer_lights(void)
{
    /* the pointer still on the picture, moved onto Next's arrow (the CD's box 360..420 x 430..480), then onto Done (0..130), then still */
    static const int AT[][2] = { { 200, 200 }, { 200, 200 }, { 390, 455 }, { 390, 455 }, { 60, 470 }, { 60, 470 }, { -1, -1 } };
    static const int KEYS[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, GALLERY_KEY_UP, 0, 0, 0, GALLERY_KEY_ESCAPE, 0, -1 };
    GalleryState s = { GALLERY_SKIN, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    places = AT, place_reads = 0, drawn_item = -1;
    CarPieceCard c;
    memset(&c, 0, sizeof c);
    c.skins = 1;
    keys_in = KEYS, key_reads = frames = services = shows = ops_count = 0;
    ops_seen[0] = 0;
    CHECK_EQ_INT(added_gallery_run(&c, &s, &OPS), GALLERY_LEAVE);
    CHECK(strchr(ops_seen + 5, 'D') != NULL);   /* drawn again after it opened: the pointer's item lit */
    /* Done under the pointer, then Up by the keys (Next) with the pointer still on Done: the keys' item stays */
    CHECK_EQ_INT(drawn_item, GALLERY_NEXT);
    {   /* a fixed page (the Warrior's): its boxes are not lit by the pointer */
        static const int SKIN[][2] = { { 10, 10 }, { 400, 200 }, { -1, -1 } };
        static const int ESC[] = { 0, 0, 0, GALLERY_KEY_ESCAPE, 0, -1 };
        GalleryState w = { GALLERY_NEXT, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0 };
        places = SKIN, place_reads = 0, drawn_item = -1;
        keys_in = ESC, key_reads = 0;
        added_gallery_run(&c, &w, &OPS);
        CHECK_EQ_INT(drawn_item, GALLERY_NEXT);
    }
    places = NULL;
}

/* PR B: the description scrolls by itself (a clock of 1 s a frame: 3 s at the top, then a line each 2 s), stops under the pointer, and the
 * wheel over it scrolls it by hand */
static void test_the_description_scrolls(void)
{
    static const int KEYS[] = { 0, 0, 0, 0, 0, 0, 0, 0, GALLERY_KEY_ESCAPE, 0, -1 };
    CarPieceCard c;
    memset(&c, 0, sizeof c);
    c.skins = 1;
    lines_in = 8, visible_in = 3, step_ms = 1000, fake_ms = 0, wheel_in = 0;
    GalleryState s = { GALLERY_NEXT, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    places = NULL;
    keys_in = KEYS, key_reads = frames = services = shows = ops_count = 0;
    CHECK_EQ_INT(added_gallery_run(&c, &s, &OPS), GALLERY_LEAVE);
    CHECK(drawn_scroll >= 2);   /* by itself, with the pointer away */
    static const int OVER[][2] = { { 500, 300 }, { -1, -1 } };   /* on History's tile */
    s = (GalleryState){ GALLERY_NEXT, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    places = OVER, place_reads = 0, fake_ms = 0, drawn_scroll = -1;
    keys_in = KEYS, key_reads = frames = services = shows = ops_count = 0;
    CHECK_EQ_INT(added_gallery_run(&c, &s, &OPS), GALLERY_LEAVE);
    CHECK(drawn_scroll <= 0);   /* the pointer on it: it stays */
    s = (GalleryState){ GALLERY_NEXT, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    places = OVER, place_reads = 0, fake_ms = 0, wheel_in = -2, drawn_scroll = -1;   /* two notches down over it */
    keys_in = KEYS, key_reads = frames = services = shows = ops_count = 0;
    CHECK_EQ_INT(added_gallery_run(&c, &s, &OPS), GALLERY_LEAVE);
    CHECK_EQ_INT(drawn_scroll, 2);
    wheel_in = 4;   /* four up: the top, never above it */
    s = (GalleryState){ GALLERY_NEXT, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0 };
    places = OVER, place_reads = 0, fake_ms = 0;
    keys_in = KEYS, key_reads = frames = services = shows = ops_count = 0;
    added_gallery_run(&c, &s, &OPS);
    CHECK_EQ_INT(drawn_scroll, 0);
    places = NULL, step_ms = 0, lines_in = 0;
}

/* PR C: a setting changed in the SETUP panel is kept at once (the apply op, with the page's values), the panel's other keys are not */
static void test_the_settings_are_kept(void)
{
    /* Enter opens the panel (on Transmission: a piece with no skins and no masks), Right makes it manual, Right again automatic, Escape x2 */
    static const int KEYS[] = { 0, GALLERY_KEY_ENTER, 0, GALLERY_KEY_RIGHT, 0, GALLERY_KEY_RIGHT, 0, GALLERY_KEY_ESCAPE, 0, GALLERY_KEY_ESCAPE, 0, -1 };
    GalleryState s = { GALLERY_SETUP, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    CarPieceCard c;
    memset(&c, 0, sizeof c);
    c.skins = 1;
    applied = shutters = 0, places = NULL;
    keys_in = KEYS, key_reads = frames = services = shows = ops_count = 0;
    CHECK_EQ_INT(added_gallery_run(&c, &s, &OPS), GALLERY_LEAVE);
    CHECK_EQ_INT(shutters, 11);   /* the panel opened once and closed once */
    CHECK_EQ_INT(applied, 21);   /* first manual (1 + 1), then automatic (1 + 0) */
    CHECK_EQ_INT(s.transmission, 0);
}

int main(void)
{
    test_the_settings_are_kept();
    test_the_description_scrolls();
    test_the_pointer_lights();
    test_the_music_plays_on();
    test_the_fades();
    return TEST_REPORT();
}
