/* The race's music and the voices' mute, unmute and off (race_sound.h). */
#include "game/race/race_sound_private.h"

void race_sound_setup(RaceState *race, void (*music_volume)(void *user, int16_t volume), void *user)
{
    RaceEntry *entry = &race->entries[race_slot(race->player)];  /* port guard: the original indexes unchecked */
    if (race->unknown_52b31e != 0 && race->unknown_52b30e == 0) entry->sound_volume = 0;
    if (entry->sound_volume == 0) return;
    music_volume(user, (int16_t)entry->music_volume);
    for (int i = 0; i < RACE_VOICES; i++) race->voices[i].voice = race->voices[i].sound = -1;
}

/* 00443768 */
void race_sound_mute(RaceState *race, void (*music_now)(void *user, int16_t volume), void *user)
{
    for (int i = 0; i < RACE_VOICES; i++)
        if (race->sounds.voice_volume != NULL) race->sounds.voice_volume(race->sounds.user, race->voices[i].voice, 0);
    music_now(user, 0);
    race->sound_silent = 1;
}

/* 0044379c */
void race_sound_unmute(RaceState *race, void (*music_volume)(void *user, int16_t volume), void *user)
{
    race->sound_silent = 0;
    music_volume(user, (int16_t)race->entries[race_slot(race->player)].music_volume);  /* port guard: race_slot */
}

/* 004437c8 */
void race_sound_voices_off(RaceState *race, void (*music_now)(void *user, int16_t volume), void *user)
{
    for (int i = 0; i < RACE_VOICES; i++)
        if (race->sounds.voice_volume != NULL) race->sounds.voice_volume(race->sounds.user, race->voices[i].voice, 0);
    music_now(user, 0);
}

/* 004437f0 */
void race_sound_music_back(RaceState *race, void (*music_volume)(void *user, int16_t volume), void *user)
{
    if (race->sound_silent == 0) music_volume(user, (int16_t)race->entries[race_slot(race->player)].music_volume);  /* port guard: race_slot */
}

enum { MUSIC_ANY = 1, MUSIC_FIRST_HALF = 2, MUSIC_SECOND_HALF = 3, MUSIC_FIXED = 4, MUSIC_LAST = 15, MUSIC_TRACKS = 12,
       MUSIC_HALF = 6 };

void race_music_start(const RaceMusicOps *ops, int16_t music)
{
    int32_t track;
    switch (music) {
    case MUSIC_ANY: track = ops->rand_below(ops->ctx, MUSIC_TRACKS); break;
    case MUSIC_FIRST_HALF: track = ops->rand_below(ops->ctx, MUSIC_HALF); break;
    case MUSIC_SECOND_HALF: track = ops->rand_below(ops->ctx, MUSIC_HALF) + MUSIC_HALF; break;
    default:
        if (music < MUSIC_FIXED || music > MUSIC_LAST) return;
        track = music - MUSIC_FIXED;
    }
    ops->play(ops->ctx, track);
}

void race_music_pick(const RaceMusicOps *ops, int16_t music, int16_t *small)
{
    int32_t track;
    *small = 0;
    switch (music) {
    case MUSIC_ANY: track = ops->rand_below(ops->ctx, MUSIC_TRACKS); break;
    case MUSIC_FIRST_HALF: case 4: case 5: case 6: case 7: case 8: case 9:
        track = ops->rand_below(ops->ctx, MUSIC_HALF);
        break;
    case MUSIC_SECOND_HALF: case 10: case 11: case 12: case 13: case 14: case MUSIC_LAST:
        track = ops->rand_below(ops->ctx, MUSIC_HALF) + MUSIC_HALF;
        break;
    default: return;
    }
    ops->play(ops->ctx, track);
}

enum { MUSIC_NONE = 5 };  /* 0045638c's track not played */

void race_music_by_idle(const RaceMusicOps *ops, int16_t music, int32_t idle)
{
    int32_t track;
    switch (music) {
    case 0: track = MUSIC_NONE; break;
    case MUSIC_ANY: track = idle % MUSIC_TRACKS; break;
    case MUSIC_FIRST_HALF: case 4: case 5: case 6: case 7: case 8: case 9: track = idle % MUSIC_HALF; break;
    case MUSIC_SECOND_HALF: case 10: case 11: case 12: case 13: case 14: case MUSIC_LAST:
        track = idle % MUSIC_HALF + MUSIC_HALF;
        break;
    default: return;  /* port guard: edx as it came */
    }
    if (track == MUSIC_NONE || track < 0) return;  /* port guard: track < 0 */
    ops->play(ops->ctx, track);
}

static int chosen_song(int16_t music, int32_t port_menu_song)
{
    return port_menu_song != 0 && music >= MUSIC_FIXED && music <= MUSIC_LAST;
}

void race_music_menu_pick(const RaceMusicOps *ops, int16_t music, int16_t *small, int32_t port_menu_song)
{
    if (!chosen_song(music, port_menu_song)) {
        race_music_pick(ops, music, small);
        return;
    }
    *small = 0;
    race_music_start(ops, music);
}

void race_music_menu_by_idle(const RaceMusicOps *ops, int16_t music, int32_t idle, int32_t port_menu_song)
{
    if (chosen_song(music, port_menu_song)) race_music_start(ops, music);   /* also rock6, which 0045638c's track 5 skips */
    else race_music_by_idle(ops, music, idle);
}
