#include "game/frontend/intro.h"

#include <stdio.h>

enum { REPLAY_PLAYING = 3, COLOURS = 0x100, ROTATION = 3 };

/* 00441abc's replay: after the first time, the next of REPLAY1..3.RPL when it exists */
static int16_t next_replay(IntroState *s, const IntroOps *ops)
{
    int16_t stopped = 0;
    if (!*s->replay_seen) {
        *s->replay_seen = 1;
        return 0;
    }
    char path[INTRO_PATH];
    ops->window(ops->ctx, 0x20, 0xc8, 0xc0, 0x120);
    *s->replay_flag = REPLAY_PLAYING;
    *s->replay = (int16_t)((*s->replay + 1) % ROTATION);
    snprintf(s->replay_name, INTRO_REPLAY_NAME, "REPLAY%d.RPL", *s->replay + 1);
    snprintf(path, sizeof path, "gamedata\\replay\\%s", s->replay_name);
    if (ops->file_exists(ops->ctx, path)) stopped = (int16_t)ops->replay(ops->ctx, 2, path);
    *s->replay_flag = 0;
    ops->window(ops->ctx, 0, 0, 0x280, 0x1e0);
    return stopped;
}

/* black, cleared on both pages */
static void black(const uint8_t pal[PALETTE_PUT_BYTES], const IntroOps *ops)
{
    ops->palette_set(ops->ctx, 0, COLOURS, pal);
    ops->clear_both(ops->ctx);
}

/* 00441abc */
int intro_show(IntroState *s, const IntroOps *ops)
{
    uint8_t pal[PALETTE_PUT_BYTES];
    ops->pointer_hide(ops->ctx);
    palette_put_clear(pal);
    ops->drain(ops->ctx);
    if (s->intro_movies) {
        int16_t stopped = next_replay(s, ops);
        ops->video_reset(ops->ctx);
        black(pal, ops);
        ops->mode_set(ops->ctx, 0x140, 0xc8, 8);
        ops->video_reset(ops->ctx);
        black(pal, ops);
        if (!stopped) {
            snprintf(s->movie_path, INTRO_PATH, "%sea.tgv", s->movie_dir);
            stopped = (int16_t)ops->movie(ops->ctx, s->movie_path);
        }
        if (!stopped) {
            if (!*s->attract_seen) {
                *s->attract_seen = 1;
                *s->attract = (int16_t)ops->random_pick(ops->ctx);
            } else {
                *s->attract = (int16_t)((*s->attract + 1) % ROTATION);
            }
            snprintf(s->movie_path, INTRO_PATH, "%sattract%d.tgv", s->movie_dir, *s->attract + 1);
            stopped = (int16_t)ops->movie(ops->ctx, s->movie_path);
        }
        if (!stopped) {
            snprintf(s->movie_path, INTRO_PATH, "%stitle.tgv", s->movie_dir);
            ops->movie(ops->ctx, s->movie_path);
        }
    } else if (!(int16_t)ops->screen_show(ops->ctx, "eatitle.qfs", 0, 0)) {
        next_replay(s, ops);
    }
    black(pal, ops);
    ops->mode_set(ops->ctx, 0x280, 0x1e0, 8);
    ops->video_reset(ops->ctx);
    black(pal, ops);
    return 1;
}
