/* port (dep59): an added car's gallery page run (game/frontend/added_gallery_run.h) over recording ops: the showcase's music serviced every
 * frame the page runs, the let-go wait at its end too (owner 2026-11-17: the gallery's music stopped on an added car's page). */
#include "game/frontend/added_gallery_run.h"
#include "game/sys/pointer.h"
#include "test.h"

#include <string.h>

static const int *keys_in;   /* the key each frame, the last one kept */
static int key_reads, frames, services, services_at_wait[64], shows;

static void show(void *ctx, const GalleryState *s) { (void)ctx, (void)s, shows++; }
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

static const GalleryRunOps OPS = { show, key, buttons, click, music, wait, NULL };

static int run(const int *keys, GalleryState *s)
{
    CarPieceCard c;
    memset(&c, 0, sizeof c);
    c.skins = 1;
    keys_in = keys, key_reads = frames = services = shows = 0;
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

int main(void)
{
    test_the_music_plays_on();
    return TEST_REPORT();
}
