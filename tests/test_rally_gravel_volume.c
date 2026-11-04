/* Rolling on rally gravel (rally_gravel_shoulder) plays the tyre sound at half the volume; a skid or the shoulder keeps the original.
 * What is asserted is the volume the mixer is handed, not the skid fed to race_sound_tyres: the front tyre's volume is skid * loudness / 128 but the rear's is the loudness alone,
 * so the skid floor halved only the front (the owner heard no change from the rear). */
#include "game/race/race_sound.h"
#include "game/race/rally_gravel.h"
#include "test.h"

#include <string.h>

enum { FRONT_ROAD = 0x2a, FRONT_OFF = 0x26, REAR_ROAD = 0x2b, REAR_OFF = 0x28 };

typedef struct { int started, volume[8], sound[8]; } Fake;

static int32_t voice_start(void *user, int32_t sound, int32_t pan, uint8_t volume, uint8_t pitch)
{
    Fake *f = user;
    (void)pan, (void)pitch;
    if (f->started < 8) f->sound[f->started] = sound, f->volume[f->started] = volume;
    return f->started++;
}
static void voice_set(void *user, int32_t voice, int32_t v) { (void)user, (void)voice, (void)v; }

static RaceState race;
static Car car;
static Fake fake;

static void sound(void *user, int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f)
{
    (void)user;
    race_sound_play(&race, a, b, c, d, e, f);
}

/* one tick of race_sound_tyres at 25 m/s; the volume of the voice started with `sound_a` or `sound_b`, -1 if none */
static int volume_of(int gravel, int shoulder_sound, int32_t skid, int on_shoulder, int sound_a, int sound_b)
{
    memset(&race, 0, sizeof race);
    memset(&car, 0, sizeof car);
    memset(&fake, 0, sizeof fake);
    for (int i = 0; i < RACE_VOICES; i++) race.voices[i].voice = -1, race.voices[i].sound = -1;
    for (int i = 0; i < RACE_CARS; i++) race.cars[i] = &car;
    race.entries[0].sound_volume = 0x7f;
    race.sounds.user = &fake;
    race.sounds.voice_start = voice_start;
    race.sounds.voice_volume = voice_set;
    race.sounds.voice_pitch = voice_set;
    race.sounds.voice_pan = voice_set;
    race.sounds.sound = sound;
    race.unknown_4c5f98 = -1;
    race.unknown_4bc6f0 = 1;
    race.port_rally_gravel = gravel, race.port_rally_shoulder_sound = shoulder_sound;
    car.speed = 25 << 16, car.skid_front = skid, car.skid_rear = skid;
    car.node_flag_4aa = on_shoulder;
    race_sound_tyres(&race);
    for (int i = 0; i < fake.started && i < 8; i++)
        if (fake.sound[i] == sound_a || fake.sound[i] == sound_b) return fake.volume[i];
    return -1;
}

int main(void)
{
    const int rear_slide = volume_of(RALLY_GRAVEL_COMPACT, 1, 0x70000, 0, REAR_ROAD, REAR_OFF);   /* a real slide on gravel */
    const int rear_roll = volume_of(RALLY_GRAVEL_COMPACT, 1, 0, 0, REAR_ROAD, REAR_OFF);          /* rolling */
    CHECK_EQ_INT(rear_slide, 126);   /* the loudness at distance 1: 127 less one step */
    CHECK_EQ_INT(rear_roll, rear_slide / 2);                                                                  /* half the original volume */
    CHECK_EQ_INT(volume_of(RALLY_GRAVEL_COMPACT, 1, 0x70000, 1, REAR_ROAD, REAR_OFF), rear_slide);  /* the shoulder (a node flag): untouched */
    CHECK_EQ_INT(volume_of(RALLY_GRAVEL_COMPACT, 0, 0, 0, REAR_ROAD, REAR_OFF), -1);              /* the switch off: the exe's silence */
    CHECK_EQ_INT(volume_of(RALLY_GRAVEL_OFF, 1, 0, 0, REAR_ROAD, REAR_OFF), -1);

    /* the front: its volume is skid * loudness / 128, and the roll's skid is the floor, 37 at 25 m/s (half the first version's 75) */
    const int front_slide = volume_of(RALLY_GRAVEL_COMPACT, 1, 0x70000, 0, FRONT_ROAD, FRONT_OFF);
    const int front_roll = volume_of(RALLY_GRAVEL_COMPACT, 1, 0, 0, FRONT_ROAD, FRONT_OFF);
    CHECK_EQ_INT(front_slide, 127);   /* the big skid saturates the level */
    CHECK_EQ_INT(front_roll, 37 * 126 / 128);
    CHECK_EQ_INT(rally_gravel_roll_distance(0x200000), 0x200000);
    CHECK_EQ_INT(rally_gravel_roll_distance(0), 0x100000);
    return TEST_REPORT();
}
