/* The force-feedback stick's effects (race_forces.h). */
#include "game/race/race_forces.h"

#include "game/race/race_camera.h"

enum { JOLT_GAP = 0x19, FROM_SIDE = 0x2d, FROM_FRONT = 0x13b, BY_IMPULSE = 5, BUFFETING = 0x50 };

/* the impulses each step stays below, and its strength and time */
static const int32_t BELOW[BY_IMPULSE - 1] = { 0x2000, 0x5000, 0x10000, 0x100000 };
static const int32_t STRENGTH[BY_IMPULSE] = { 0x50, 0x55, 0x5a, 0x5f, 0x64 };
static const int32_t TIME[BY_IMPULSE] = { 100, 0x96, 200, 0xf0, 0x118 };

/* 0043cc84 */
void race_jolt(RaceForces *f, int32_t clock, int32_t viewed, int32_t player_a, int32_t x, int32_t impulse,
               const RaceSounds *sounds)
{
    if (clock >= f->jolt_at && (int32_t)((uint32_t)f->jolt_at + JOLT_GAP) > clock) return;
    f->jolt_at = clock;
    int step = 0;
    while (step < BY_IMPULSE - 1 && impulse >= BELOW[step]) step++;
    const int32_t from = x > 0 && player_a != viewed ? FROM_SIDE : FROM_FRONT;
    sounds->tone(sounds->user, STRENGTH[step], from, TIME[step]);
}

/* 0043d120 */
void race_buffet(RaceState *race, int32_t on)
{
    if (race->force_off != 0 && on == 0) race_forces_clear(race);
    if (race->force_off == 0 && on != 0) {
        race_forces_clear(race);
        race->force_off = 1;
        if (race->sounds.buffeting != NULL) race->sounds.buffeting(race->sounds.user, BUFFETING);
    }
}

/* port (replay_no_ffb) */
int race_forces_replay_silent(const RaceState *race, int replay_no_ffb) { return replay_no_ffb != 0 && race != NULL && race->replay != 0; }
