#include "game/race/race_music.h"

enum { FILL_TICKS = 100, VOLUME_STEPS = 50 };

static int32_t ahead(const RaceMusic *m) { return m->small ? RACE_MUSIC_AHEAD_SMALL : RACE_MUSIC_AHEAD; }

/* 00464240 */
void race_music_pump(const RaceMusicStreamOps *ops, RaceMusic *m)
{
    if (m->unknown_52b31e != 0) return;
    if (m->playback != 0) ops->service(ops->ctx);
    if (m->setting == 0 || m->sound_on == 0 || m->open == 0) return;
    m->block = ops->next(ops->ctx, m->reader);
    if (m->started == 0 && ops->queued(ops->ctx) > ahead(m)) {
        m->handle = ops->start(ops->ctx);
        ops->set_volume(ops->ctx, m->handle, m->volume);
        m->started = 1;
    }
    if (m->block != 0) ops->block(ops->ctx, m->block);
}

/* 004640d0 */
void race_music_fill(const RaceMusicStreamOps *ops, RaceMusic *m)
{
    if (m->unknown_52b31e != 0 || m->setting == 0) return;
    ops->service(ops->ctx);
    if (m->sound_on == 0 || m->open == 0) return;
    ops->ticks(ops->ctx);
    int32_t queued = ops->queued(ops->ctx), ticks = 0;
    while (queued < ahead(m)) {
        ticks = (int32_t)((uint32_t)ticks + (uint32_t)ops->ticks(ops->ctx));
        m->block = ops->next(ops->ctx, m->reader);
        if (m->block != 0) ops->block(ops->ctx, m->block);
        queued = ops->queued(ops->ctx);
        if ((int16_t)ticks > FILL_TICKS && queued == 0) return;
    }
}

static int music_on(const RaceMusic *m) { return m->unknown_52b31e == 0 && m->setting != 0 && m->sound_on != 0; }

/* 00464568 */
void race_music_stop(const RaceMusicStreamOps *ops, RaceMusic *m)
{
    if (!music_on(m) || m->open == 0) return;
    while (!ops->stopped(ops->ctx, m->handle) && m->started != 0) ops->service(ops->ctx);
    ops->close(ops->ctx, m->reader);
    m->open = 0;
    m->started = 0;
}

/* 00464428 */
void race_music_play(const RaceMusicStreamOps *ops, RaceMusic *m, const char *name)
{
    if (!music_on(m)) return;
    if (m->open != 0) {
        ops->stop(ops->ctx, m->handle);
        race_music_stop(ops, m);
    }
    ops->settle(ops->ctx);
    ops->path(ops->ctx, name);
    m->reader = ops->open(ops->ctx, m->small ? RACE_MUSIC_RING_SMALL : RACE_MUSIC_RING);
    ops->request(ops->ctx, m->reader);
    ops->reset(ops->ctx, m->reader);
    m->open = 1;
}

/* 00464604 */
void race_music_volume(const RaceMusicStreamOps *ops, const RaceMusic *m, int16_t volume)
{
    if (music_on(m)) ops->ramp(ops->ctx, m->handle, VOLUME_STEPS, volume);
}

/* 0046463c */
void race_music_volume_now(const RaceMusicStreamOps *ops, const RaceMusic *m, int16_t volume)
{
    if (music_on(m)) ops->ramp(ops->ctx, m->handle, 0, volume);
}
