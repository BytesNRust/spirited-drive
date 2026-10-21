/* 00464568 waits for the music's voice to stop; 00464534 is what stops it. The Options audio screen's SONG item calls the
 * two in that order, and with the wait wired to both the screen froze (a voice nobody cut never stops). */
#include "game/race/race_music.h"
#include "test.h"

#include <string.h>

typedef struct {
    int stopped_voice;
    int services;
    int closed;
} Fake;

enum { SPIN_LIMIT = 1000 };

static void service(void *ctx)
{
    Fake *f = ctx;
    f->services++;
    if (f->services >= SPIN_LIMIT) f->stopped_voice = 1; /* the test's way out of a spin */
}
static int32_t stopped(void *ctx, int32_t handle) { (void)handle; return ((Fake *)ctx)->stopped_voice; }
static void stop(void *ctx, int32_t handle) { (void)handle; ((Fake *)ctx)->stopped_voice = 1; }
static void close_reader(void *ctx, uint32_t reader) { (void)reader; ((Fake *)ctx)->closed++; }

static RaceMusic playing(void)
{
    RaceMusic m;
    memset(&m, 0, sizeof m);
    m.setting = 1, m.sound_on = 1, m.open = 1, m.started = 1, m.handle = 7;
    return m;
}

int main(void)
{
    RaceMusicStreamOps ops;
    memset(&ops, 0, sizeof ops);
    ops.service = service, ops.stopped = stopped, ops.stop = stop, ops.close = close_reader;

    /* the voice cut first: the wait ends at once and the reader closes */
    Fake cut = { 0, 0, 0 };
    RaceMusic m = playing();
    ops.ctx = &cut;
    ops.stop(ops.ctx, m.handle);
    race_music_stop(&ops, &m);
    CHECK_EQ_INT(cut.services, 0);
    CHECK_EQ_INT(cut.closed, 1);
    CHECK_EQ_INT(m.open, 0);
    CHECK_EQ_INT(m.started, 0);

    /* the voice never cut: the wait spins until something else stops it (here the test's limit) */
    Fake uncut = { 0, 0, 0 };
    m = playing();
    ops.ctx = &uncut;
    race_music_stop(&ops, &m);
    CHECK_EQ_INT(uncut.services, SPIN_LIMIT);

    /* a track never started has nothing to wait for */
    Fake idle = { 0, 0, 0 };
    m = playing();
    m.started = 0;
    ops.ctx = &idle;
    race_music_stop(&ops, &m);
    CHECK_EQ_INT(idle.services, 0);
    CHECK_EQ_INT(idle.closed, 1);

    return TEST_REPORT();
}
