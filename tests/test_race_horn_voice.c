/* A traffic car's horn (sounds 0x3f..0x41, looped in the CD's banks) is held on channel 4 for car_sound_ticks and cut
 * when they run out (race_sound_held_car) -- but only while the channel's record still names the horn. A
 * tyre or crash sound on channel 4 meanwhile starts a second voice and takes the record over; the horn's voice then
 * loops to the race's end, and a tyre's loop the horn takes the channel from the same way. port_stop_replaced cuts the
 * voice a channel's record loses to another sound. */
#include "game/race/race_sound.h"
#include "test.h"

#include <string.h>

enum { VOICES = 64, HORN = 0x3f, TYRE_FRONT = 0x2a };

typedef struct {
    int next;
    int sound[VOICES];
    int live[VOICES];
} Fake;

static int32_t voice_start(void *user, int32_t sound, int32_t pan, uint8_t volume, uint8_t pitch)
{
    Fake *f = user;
    (void)pan, (void)volume, (void)pitch;
    f->sound[f->next] = sound, f->live[f->next] = 1;
    return f->next++;
}
static void voice_set(void *user, int32_t voice, int32_t v) { (void)user, (void)voice, (void)v; }
/* voice -1 is "no voice" (a start that failed, or none yet): stopped, nothing to cut */
static void voice_stop(void *user, int32_t voice) { if (voice >= 0 && voice < VOICES) ((Fake *)user)->live[voice] = 0; }
static int32_t voice_stopped(void *user, int32_t voice) { return voice < 0 || voice >= VOICES || ((Fake *)user)->live[voice] == 0; }

static RaceState race;
static Car car;
static Fake fake;

static int horns_alive(void)
{
    int n = 0;
    for (int i = 0; i < fake.next; i++) n += fake.live[i] && fake.sound[i] >= HORN && fake.sound[i] <= 0x41;
    return n;
}

static void setup(int port_stop)
{
    memset(&race, 0, sizeof race);
    memset(&car, 0, sizeof car);
    memset(&fake, 0, sizeof fake);
    for (int i = 0; i < RACE_VOICES; i++) race.voices[i].voice = -1, race.voices[i].sound = -1;
    race.entries[0].sound_volume = 0x7f;
    race.sounds.user = &fake;
    race.sounds.voice_start = voice_start;
    race.sounds.voice_volume = voice_set;
    race.sounds.voice_pitch = voice_set;
    race.sounds.voice_pan = voice_set;
    race.sounds.voice_stop = voice_stop;
    race.sounds.voice_stopped = voice_stopped;
    race.cars[2] = &car;
    car.x = 0x100000, car.z = 0x100000;
    race.car_sound_slot = 2, race.car_sound_kind = 0, race.car_sound_ticks = 6;
    race.port_stop_replaced = port_stop;
}

static int run(int port_stop)
{
    setup(port_stop);
    race_sound_held_car(&race);   /* the Doppler factor needs a second call to sound */
    race_sound_held_car(&race);
    CHECK_EQ_INT(horns_alive(), 1);
    race_sound_play(&race, 4, 4, 1, 0x400000, 0x10000, 0);   /* the front tyre's sound on channel 4 */
    CHECK_EQ_INT(race.voices[4].sound, TYRE_FRONT);
    for (int i = 0; i < 12; i++) race_sound_held_car(&race);
    return horns_alive();
}

/* the other way round: the tyre's loop on channel 4 and a horn taking the channel */
static int skids_alive(int port_stop)
{
    setup(port_stop);
    race_sound_play(&race, 4, 4, 1, 0x400000, 0x10000, 0);
    race_sound_held_car(&race);
    race_sound_held_car(&race);
    int n = 0;
    for (int i = 0; i < fake.next; i++) n += fake.live[i] && fake.sound[i] == TYRE_FRONT;
    return n;
}

int main(void)
{
    CHECK_EQ_INT(skids_alive(0), 1);   /* the exe: the tyre's voice is left looping under the horn */
    CHECK_EQ_INT(skids_alive(1), 0);

    CHECK_EQ_INT(run(0), 1);   /* the exe: the horn's voice is left looping */
    CHECK_EQ_INT(run(1), 0);   /* the port cuts it when the tyre sound takes the channel */

    /* a horn left alone is cut at the end of its ticks in both */
    setup(1);
    race_sound_held_car(&race);
    race_sound_held_car(&race);
    for (int i = 0; i < 12; i++) race_sound_held_car(&race);
    CHECK_EQ_INT(horns_alive(), 0);
    return TEST_REPORT();
}
