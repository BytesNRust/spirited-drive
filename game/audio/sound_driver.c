#include "game/audio/sound_driver.h"

#include <stddef.h>

static void channel_stopped(void *ctx, uint32_t slot)
{
    sound_channel_stopped(ctx, (int32_t)slot);
}

void sound_driver_wire(SoundDriver *d)
{
    for (int k = 0; k < SOUND_VOICES; k++) {
        d->mixer.voice[k] = &d->voice[k];
        d->voice[k].buffer = d->decoded;
        d->voice[k].stream = &d->stream;
    }
    d->mixer.stopped = channel_stopped;
    d->mixer.ctx = &d->channels;
}

/* 00497bb5 */
int32_t sound_driver_init(SoundDriver *d, uint32_t rate)
{
    sound_driver_wire(d);
    if (!d->mixer_ready) {
        d->mixer_ready = 1;
        for (int k = SOUND_VOICES - 1; k >= 0; k--) d->mixer.voice[k]->on = 0;
    }
    d->mix_rate = rate;
    return SOUND_VOICES;
}

/* 00473c68 */
int32_t sound_stop(SoundDriver *d, int32_t handle)
{
    const SoundChannel *c = sound_handle_channel(&d->channels, handle);
    if (c == NULL) return -1;
    sound_voice_stop(&d->mixer, (uint32_t)d->channels.handle[handle]); /* 00497a1c */
    if (c->kind == SOUND_KIND_STREAM) sound_stream_drop(&d->stream);
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
    c->kind = SOUND_KIND_SAMPLE;
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

enum { NOT_A_SAMPLE = -15, BAD_PITCH = -16 };

/* 00473c08 */
int32_t sound_set_pitch(SoundDriver *d, int32_t handle, int32_t pitch)
{
    const SoundChannel *c = sound_handle_channel(&d->channels, handle);
    if (c == NULL) return -1;
    if (c->kind != SOUND_KIND_SAMPLE) return NOT_A_SAMPLE;
    if (pitch < 0 || pitch > TOP_LEVEL) return BAD_PITCH;
    MixVoice *v = &d->voice[d->channels.handle[handle]]; /* 00497a1c */
    v->step = sound_voice_step(v->rate, sound_pitch_cents(pitch, c->range, c->pitch_base), d->mix_rate); /* 00497f05 */
    return 0;
}

enum { NO_SOUND = -19, SOUND_IDS = 0x80 };

/* 0048eea0 */
int32_t sound_play(SoundDriver *d, int32_t id, int32_t pan, int32_t volume, int32_t pitch)
{
    if (id < 0 || id >= SOUND_IDS || d->registry[id] == NULL) return NO_SOUND;
    return sound_play_record(d, d->registry[id], id, pan, volume, pitch);
}

enum {
    EACS_TAG = 0x53434145,   /* 'EACS' */
    STREAM_HEADER = 8,       /* the '1SNh' block's EACS header */
    STREAM_DATA = 0x28,      /* its samples */
    STREAM_PRIORITY = 0x65,
    STREAM_CENTRE = 0x40,
    STREAM_COMPRESSED = 2,
    STREAM_NO_SOUND = 0xff
};

/* 0048db70 */
int32_t sound_stream_header(SoundDriver *d, uint32_t block)
{
    SoundStream *s = &d->stream;
    const SoundRing *r = &s->ring;
    const uint32_t h = block + STREAM_HEADER;
    const int32_t ch = sound_ring_get(r, h) == EACS_TAG ? (int32_t)sound_ring_get(r, h + 8) >> 24 : -1;
    if (ch < 0 || ch >= SOUND_CHANNELS) {
        s->state = 0;
        return -1;
    }
    SoundChannel *c = &d->channels.channel[ch];
    if (c->busy == 1) sound_stop(d, c->handle); /* 004979e8: the channel's handle */
    s->handle = sound_handle_alloc(&d->channels, ch);
    if (s->handle < 0) return -1;
    c->sound = NULL, c->id = STREAM_NO_SOUND, c->last = -1, c->pitch = STREAM_CENTRE, c->range = 0, c->pitch_base = 0;
    c->busy = 1, c->priority = STREAM_PRIORITY, c->cleared = 0, c->kind = SOUND_KIND_STREAM, c->pan = STREAM_CENTRE;
    c->sound_volume = *sound_ring_at(r, h + 0x1d);
    c->volume = (uint8_t)s->volume;
    const uint32_t frame_bytes = (uint32_t)*sound_ring_at(r, h + 8) * *sound_ring_at(r, h + 9);
    sound_ring_put(r, h + 0x18, block + STREAM_DATA);
    const uint32_t bytes = sound_ring_get(r, block + 4) - STREAM_DATA;
    s->channel = ch;
    s->header = h;
    sound_ring_put(r, h + 0x10, 0xffffffffu); /* no loop */
    s->frame_bytes = (int32_t)frame_bytes;
    sound_ring_put(r, h + 0x14, 0);
    sound_ring_put(r, h + 0xc, frame_bytes == 0 ? 0 : bytes / frame_bytes); /* the original faults on 0 */
    s->played = 0;
    s->rate = sound_ring_get(r, h + 4);
    s->frame_scale = *sound_ring_at(r, h + 0xa) == STREAM_COMPRESSED ? 4 : 1;
    sound_stream_append(s, block); /* when the stream has not failed */
    return 0;
}

enum {
    TAG_DATA = 0x644e5331,   /* '1SNd' */
    TAG_END = 0x654e5331,    /* '1SNe' */
    TAG_HEADER = 0x684e5331, /* '1SNh' */
    TAG_LOOP = 0x6c4e5331,   /* '1SNl' */
    LOOP_OFFSET = 8
};

/* 0048df3c */
int32_t sound_stream_block(SoundDriver *d, uint32_t block)
{
    SoundStream *s = &d->stream;
    switch (sound_ring_get(&s->ring, block)) {
    case TAG_DATA: return sound_stream_append(s, block);
    case TAG_END:
        sound_ring_put(&s->ring, block, SOUND_BLOCK_END);
        sound_stream_append(s, block);
        return 0;
    case TAG_HEADER: return sound_stream_header(d, block);
    case TAG_LOOP:
        if (s->loop != NULL) s->loop(s->loop_ctx, s->file, sound_ring_get(&s->ring, block + LOOP_OFFSET)); /* 0048ab4c */
        sound_ring_put(&s->ring, block, SOUND_BLOCK_FREE); /* 0048bde8 */
        return 0;
    default: return -1;
    }
}

enum { STREAM_SET_UP = 3, STREAM_PLAYING = 2 };

/* The stream's EACS header in its ring as 00497784 reads it */
static SoundSample ring_sample(const SoundRing *r, uint32_t h)
{
    const uint8_t *b = sound_ring_at(r, h);
    return (SoundSample){ sound_ring_get(r, h), sound_ring_get(r, h + 4), b[8], b[9], b[0xa], (int8_t)b[0xb],
                          sound_ring_get(r, h + 0xc), sound_ring_get(r, h + 0x10), sound_ring_get(r, h + 0x14),
                          sound_ring_at(r, sound_ring_get(r, h + 0x18)), b[0x1c] };
}

/* 0048dedc */
int32_t sound_stream_start(SoundDriver *d)
{
    SoundStream *s = &d->stream;
    if (d->channels.open == 0 || s->channel < 0) return -1; /* 00492324: 004c7b54 */
    if (s->state == STREAM_SET_UP) {
        s->played = 0;
        sound_channel_mix_volume(&d->channels.channel[s->channel]); /* 00497750 */
        SoundSample h = ring_sample(&s->ring, s->header);
        sound_sample_start(d, &h); /* 00497784 */
        *sound_ring_at(&s->ring, s->header + 0x1c) = h.channel;
        s->state = STREAM_PLAYING;
    }
    return s->handle;
}
