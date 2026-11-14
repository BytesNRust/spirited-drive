/* the music's stream kept from the stops of handles stored for other sounds: a speech queue never played (its two
 * handles still 0) and the front end's voices freed with their bank must not cut the music when it took handle 0
 * (the menus' music lost on the way into Control Central when the intro movies were off) */
#include "tests/test.h"

#include "game/audio/sound_driver.h"
#include "game/audio/sound_queue.h"

#include <string.h>

static SoundDriver driver;

/* the driver open, the music's stream on channel 0 with the first handle, as 0048db70 leaves it */
static int32_t music_on_first_handle(SoundDriver *d)
{
    memset(d, 0, sizeof *d);
    sound_driver_wire(d);
    sound_channels_reset(&d->channels);
    d->channels.open = 1;
    sound_driver_init(d, 0x5622);
    SoundChannel *c = &d->channels.channel[0];
    c->busy = 1, c->kind = SOUND_KIND_STREAM;
    d->stream.channel = 0;
    return sound_handle_alloc(&d->channels, 0);
}

/* a queue set up and never played stops nothing but its own sounds */
static void test_queue_stop_keeps_the_music(void)
{
    const int32_t music = music_on_first_handle(&driver);
    CHECK_EQ_INT(music, 0);
    SoundQueue q;
    memset(&q, 0, sizeof q);
    CHECK_EQ_INT(sound_queue_setup(&driver, &q, 4, 5, 0x7f), 0);
    sound_queue_stop(&driver, &q);
    CHECK(!sound_handle_stopped(&driver.channels, music));
    CHECK_EQ_INT(q.ready, 0);
}

/* a stop by kind: the stream's handle passed as a sample's or a queue's stops nothing; as a stream's it stops */
static void test_stop_by_kind(void)
{
    const int32_t music = music_on_first_handle(&driver);
    CHECK_EQ_INT(sound_stop_kind(&driver, music, SOUND_KIND_SAMPLE), -1);
    CHECK_EQ_INT(sound_stop_kind(&driver, music, SOUND_KIND_QUEUE), -1);
    CHECK(!sound_handle_stopped(&driver.channels, music));
    CHECK_EQ_INT(sound_handle_kind(&driver.channels, music), SOUND_KIND_STREAM);
    CHECK_EQ_INT(sound_stop_kind(&driver, music, SOUND_KIND_STREAM), 0);
    CHECK(sound_handle_stopped(&driver.channels, music));
    CHECK_EQ_INT(sound_handle_kind(&driver.channels, music), -1);
}

/* a sample's handle still stops as before */
static void test_sample_stops(void)
{
    music_on_first_handle(&driver);
    SoundChannel *c = &driver.channels.channel[3];
    c->busy = 1, c->kind = SOUND_KIND_SAMPLE;
    const int32_t voice = sound_handle_alloc(&driver.channels, 3);
    CHECK_EQ_INT(sound_stop_kind(&driver, voice, SOUND_KIND_SAMPLE), 0);
    CHECK(sound_handle_stopped(&driver.channels, voice));
    CHECK(!sound_handle_stopped(&driver.channels, 0));
}

int main(void)
{
    test_queue_stop_keeps_the_music();
    test_stop_by_kind();
    test_sample_stops();
    return TEST_REPORT();
}
