#include "game/audio/sound_driver.h"

#include <stddef.h>

static void channel_stopped(void *ctx, uint32_t slot)
{
    sound_channel_stopped(ctx, (int32_t)slot);
}

void sound_driver_wire(SoundDriver *d)
{
    for (int k = 0; k < SOUND_VOICES; k++) d->mixer.voice[k] = &d->voice[k];
    d->mixer.stopped = channel_stopped;
    d->mixer.ctx = &d->channels;
}

/* 00473c68 */
int32_t sound_stop(SoundDriver *d, int32_t handle)
{
    if (sound_handle_channel(&d->channels, handle) == NULL) return -1;
    sound_voice_stop(&d->mixer, (uint32_t)d->channels.handle[handle]); /* 00497a1c */
    return 0;
}

enum { NO_CHANNEL = -20 };

static int32_t channel_take(SoundDriver *d, uint32_t ch)
{
    d->channels.channel_cursor = ch + 1 == SOUND_CHANNELS ? 0 : ch + 1;
    return (int32_t)ch;
}

/* 00497640 */
int32_t sound_channel_pick(SoundDriver *d, uint32_t mask, int32_t priority)
{
    uint32_t ch = d->channels.channel_cursor;
    for (int n = 0; n < SOUND_CHANNELS; n++, ch = ch + 1 == SOUND_CHANNELS ? 0 : ch + 1)
        if ((mask & 1u << (ch & 31)) && !d->channels.channel[ch].busy) return channel_take(d, ch);
    ch = d->channels.channel_cursor;
    for (int n = 0; n < SOUND_CHANNELS; n++, ch = ch + 1 == SOUND_CHANNELS ? 0 : ch + 1) {
        if (!(mask & 1u << (ch & 31)) || (int32_t)d->channels.channel[ch].priority > priority) continue;
        channel_take(d, ch);
        sound_stop(d, d->channels.channel[ch].handle); /* 00497ab8: ch is 0..15 */
        return (int32_t)ch;
    }
    return NO_CHANNEL;
}

enum {
    SAMPLE_WIDE = 2,       /* +8, +9, +0xa: the value that sets the flag */
    FLAG_16BIT = 8,
    FLAG_STEREO = 4,
    FLAG_COMPRESSED = 2,
    FLAG_HEADER = 0x10
};

/* 00497784 */
void sound_sample_start(SoundDriver *d, SoundSample *s)
{
    const SoundChannel *named = &d->channels.channel[s->channel];
    uint32_t flags = (s->sample_size == SAMPLE_WIDE ? FLAG_16BIT : 0) | (s->channels == SAMPLE_WIDE ? FLAG_STEREO : 0) |
                     (s->compression == SAMPLE_WIDE ? FLAG_COMPRESSED : 0);
    if (s->type >= 0) {
        flags |= FLAG_HEADER;
        s->channel = (uint8_t)s->type;
    }
    sound_channel_mix_volume(&d->channels.channel[s->channel]); /* 00497750, inline */
    const SoundStart start = { s->data, s->frames, s->loop_start, s->loop_frames, s->rate, named->pan,
                               named->mixed_volume, flags,
                               sound_pitch_cents(named->pitch, named->range, named->pitch_base) };
    sound_voice_start(&d->voice[s->channel], &start, d->mix_rate);
}

enum { CENTRE_PAN = 0x40, TOP_LEVEL = 0x7f };

static uint8_t sound_volume(SoundDriver *d, const SoundBankSound *rec)
{
    if (rec->volume_spread == 0) return rec->volume;
    const int32_t v = sound_spread(&d->rand_seed, rec->volume_spread) + rec->volume;
    return (uint8_t)(v < 0 ? 0 : v > TOP_LEVEL ? TOP_LEVEL : v);
}

static uint8_t sound_pan(SoundDriver *d, const SoundBankSound *rec, int32_t pan)
{
    if (pan != SOUND_PAN_OWN) return (uint8_t)pan;
    if (rec->pan_spread == 0) return rec->pan;
    if (sound_spread(&d->rand_seed, rec->pan_spread) + CENTRE_PAN > TOP_LEVEL) return TOP_LEVEL;
    return (uint8_t)(sound_spread(&d->rand_seed, rec->pan_spread) + CENTRE_PAN); /* the original draws again */
}

/* 0048eed8 */
int32_t sound_play_record(SoundDriver *d, const SoundBankSound *rec, int32_t id, int32_t pan, int32_t volume,
                          int32_t pitch)
{
    const int32_t ch = sound_channel_pick(d, rec->mask, rec->priority);
    if (ch < 0) return SOUND_NO_CHANNEL;
    SoundChannel *c = &d->channels.channel[ch]; /* 00497acc: ch is 0..15 */
    SoundSample *s = rec->sample;
    if (s->magic != SOUND_EACS) return SOUND_NOT_EACS;
    c->sound = rec;
    c->last = rec->count - 1;
    c->pitch = (uint8_t)pitch;
    c->range = rec->range;
    c->priority = rec->priority;
    c->id = (uint8_t)id;
    c->sound_1c = rec->sound_1c;
    c->kind = 1;
    c->pitch_base = rec->pitch_base != 0 || rec->pitch_spread != 0
                        ? sound_spread(&d->rand_seed, rec->pitch_spread) + rec->pitch_base : 0;
    c->sound_volume = sound_volume(d, rec);
    c->pan = sound_pan(d, rec, pan);
    c->volume = (uint8_t)volume;
    c->cleared = 0;
    sound_channel_mix_volume(c);
    s->channel = (uint8_t)ch;
    const int32_t handle = sound_handle_alloc(&d->channels, ch);
    c->busy = 1;
    sound_sample_start(d, s);
    return handle;
}

enum { BAD_VOLUME = -12, NOT_BUSY = -13 };

static void channel_gains(SoundDriver *d, int32_t slot)
{
    SoundChannel *c = &d->channels.channel[slot];
    sound_channel_mix_volume(c);                                      /* 00497750 */
    d->voice[slot].gains = sound_pan_gains(c->pan, c->mixed_volume);  /* 00497e8a */
}

/* 00473b4c */
int32_t sound_set_volume(SoundDriver *d, int32_t handle, int32_t volume)
{
    SoundChannel *c = sound_handle_channel(&d->channels, handle);
    if (c == NULL) return -1;
    if (volume > TOP_LEVEL || volume < 0) return BAD_VOLUME;
    if (!c->busy) return NOT_BUSY;
    c->volume = (uint8_t)volume;
    channel_gains(d, d->channels.handle[handle]); /* 00497a1c */
    return 0;
}

enum { BAD_PAN = -14, PAN_TOP = 0xff };

/* 00473bb0 */
int32_t sound_set_pan(SoundDriver *d, int32_t handle, int32_t pan)
{
    SoundChannel *c = sound_handle_channel(&d->channels, handle);
    if (c == NULL) return -1;
    if (pan > PAN_TOP || pan < 0) return BAD_PAN;
    c->pan = (uint8_t)pan;
    channel_gains(d, d->channels.handle[handle]); /* 00497a1c */
    return 0;
}
