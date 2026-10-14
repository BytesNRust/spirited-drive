#include "game/frontend/menu_sound.h"

#include <stdio.h>

static int16_t short_at(const ConfigDat *c, uint32_t at)
{
    const uint8_t *b = c->image + (at - 0x00524df0);
    return (int16_t)(uint16_t)(b[0] | b[1] << 8);
}

/* 0046492c */
void menu_sound_play(MenuSound *m, const ConfigDat *c, int sound, const MenuSoundOps *ops)
{
    enum { CENTRE = 0x40, SOUND_4A = 0x4a };
    if (!m->sound_on) return;
    const int32_t voice = ops->voice_start(ops->ctx, (int16_t)sound, CENTRE, short_at(c, 0x00524e69), 0);
    if ((int16_t)sound == SOUND_4A) m->voice_4a = voice;
    else m->voice = voice;
}

static int music_allowed(const MenuSound *m, const ConfigDat *c)
{
    return !m->music_off && short_at(c, 0x00524e63) && m->sound_on;
}

/* 004644f0 */
void menu_sound_music_fade_out(const MenuSound *m, const ConfigDat *c, const MenuSoundOps *ops)
{
    if (music_allowed(m, c) && m->music_on) ops->voice_ramp(ops->ctx, m->music, 0x64, -1);
}

/* 00464534 */
void menu_sound_music_stop(const MenuSound *m, const ConfigDat *c, const MenuSoundOps *ops)
{
    if (music_allowed(m, c) && m->music_on) ops->voice_stop(ops->ctx, m->music);
}

/* 00464604 */
void menu_sound_music_fade_to(const MenuSound *m, const ConfigDat *c, int volume, const MenuSoundOps *ops)
{
    if (music_allowed(m, c)) ops->voice_ramp(ops->ctx, m->music, 0x32, (int16_t)volume);
}

/* 0046463c */
void menu_sound_music_volume(const MenuSound *m, const ConfigDat *c, int volume, const MenuSoundOps *ops)
{
    if (music_allowed(m, c)) ops->voice_ramp(ops->ctx, m->music, 0, (int16_t)volume);
}

/* 004645d8 */
void menu_sound_effects_volume(const MenuSound *m, int volume, const MenuSoundOps *ops)
{
    if (!m->sound_on) return;
    ops->voice_volume(ops->ctx, m->voice, (int16_t)volume);
    ops->voice_volume(ops->ctx, m->voice_4a, (int16_t)volume);
}

/* 00464980 */
void menu_sound_stop_4a(const MenuSound *m, const MenuSoundOps *ops)
{
    if (m->sound_on) ops->voice_stop(ops->ctx, m->voice_4a);
}

/* 00464994 */
int menu_sound_4a_done(const MenuSound *m, const MenuSoundOps *ops)
{
    if (!m->sound_on) return 0;
    if (m->voice_4a == 0) return 1;
    return ops->voice_stopped(ops->ctx, m->voice_4a) != 0;
}

/* 004649c8 */
void menu_sound_bank_free(const MenuSound *m, const MenuSoundOps *ops)
{
    if (!m->sound_on) return;
    ops->bank_unregister(ops->ctx, m->bank);
    ops->file_free(ops->ctx, m->bank_data);
}

/* 00464568 */
void menu_sound_music_end(MenuSound *m, const ConfigDat *c, const MenuSoundOps *ops)
{
    if (!music_allowed(m, c) || !m->music_on) return;
    while (!ops->voice_stopped(ops->ctx, m->music) && m->music_voiced) ops->wait(ops->ctx);
    ops->stream_close(ops->ctx, m->stream);
    m->music_on = 0, m->music_voiced = 0;
}

/* 00464428 */
void menu_sound_music_start(MenuSound *m, const ConfigDat *c, const char *dir, const char *name,
                            const MenuSoundOps *ops)
{
    if (!music_allowed(m, c)) return;
    if (m->music_on) {
        ops->voice_stop(ops->ctx, m->music);
        menu_sound_music_end(m, c, ops);
    }
    ops->drain(ops->ctx);
    snprintf(m->music_path, sizeof m->music_path, "%s%s", dir, name);
    m->stream = ops->stream_open(ops->ctx, m->small_stream ? 0x19000 : 0x7d000, 0x800, 0x20);
    ops->stream_load(ops->ctx, m->stream, m->music_path);
    ops->stream_start(ops->ctx, m->stream);
    m->music_on = 1;
}

/* 004640d0 */
void menu_sound_music_feed(MenuSound *m, const ConfigDat *c, const MenuSoundOps *ops)
{
    if (m->music_off || !short_at(c, 0x00524e63)) return;
    const int32_t size = m->small_stream ? 0x14000 : 0x64000;
    ops->wait(ops->ctx);
    if (!m->sound_on || !m->music_on) return;
    ops->ticks(ops->ctx);
    if (ops->queued(ops->ctx) >= size) return;
    int32_t ticks = 0, queued;
    do {
        ticks += ops->ticks(ops->ctx);
        m->chunk = ops->stream_read(ops->ctx, m->stream);
        if (m->chunk != 0) ops->chunk_play(ops->ctx, m->chunk);
        queued = ops->queued(ops->ctx);
    } while (((int16_t)ticks <= 0x64 || queued != 0) && queued < size);
}

/* 0046417c */
void menu_sound_music_service(MenuSound *m, const ConfigDat *c, const FrontendSession *f, const MenuSoundOps *ops)
{
    if (m->music_off) return;
    if (f->flag_52b312) ops->wait(ops->ctx);
    if (!short_at(c, 0x00524e63)) return;
    const int32_t size = m->small_stream ? 0x14000 : 0x64000;
    if (!m->sound_on || !m->music_on) return;
    m->chunk = ops->stream_read(ops->ctx, m->stream);
    if (!m->music_voiced && ops->queued(ops->ctx) > size) {
        m->music = ops->stream_voice(ops->ctx);
        ops->voice_volume(ops->ctx, m->music, short_at(c, 0x00524e67));
        m->music_voiced = 1;
    }
    if (m->chunk != 0) ops->chunk_play(ops->ctx, m->chunk);
}

/* 004648a8 */
void menu_sound_track_music_free(MenuSound *m, const ConfigDat *c, const MenuSoundOps *ops)
{
    if (!m->sound_on || !short_at(c, 0x00524e6b)) return;
    if (m->track_music) ops->block_free(ops->ctx, m->track_music);
    m->track_music = 0;
}
