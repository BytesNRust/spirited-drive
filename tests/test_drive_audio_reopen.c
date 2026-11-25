/* A sound device that is missing at the start and appears later: drive_audio opens it then, once for each device that appears, starting the mixer as at the start and calling the owner's hook
 * (a screen's or a race's: volumes, banks). The platform is a fake here: platform_audio_open fails until `device_present`, platform_audio_device_added reports the events a test makes. */
#include "viewer/drive_audio.h"
#include "platform/platform.h"
#include "test.h"

#include <stdbool.h>
#include <string.h>

static bool device_present;
static int open_calls, close_calls;
static int added_events;

bool platform_audio_open(int rate, PlatformAudioFill fill, void *user)
{
    (void)rate, (void)fill, (void)user;
    open_calls++;
    return device_present;
}
void platform_audio_close(void) { close_calls++; }
void platform_audio_lock(void) {}
void platform_audio_unlock(void) {}
bool platform_audio_device_added(void)
{
    if (added_events == 0) return false;
    added_events--;
    return true;
}
uint8_t *platform_read_file(const char *path, size_t *size)
{
    (void)path;
    *size = 0;
    return NULL;
}
void platform_free_file(uint8_t *data) { (void)data; }
void platform_sleep_ms(uint32_t ms) { (void)ms; }
uint32_t platform_ticks_ms(void) { return 0; }

static int hook_calls;
static void hook(void *user) { (*(int *)user)++, hook_calls++; }

int main(void)
{
    int hook_user = 0;

    /* the start with no device: not open, and nothing happens until a device appears */
    device_present = false;
    CHECK_EQ_INT(drive_audio_device_open("data"), 0);
    drive_audio_on_reopen(hook, &hook_user);
    CHECK_EQ_INT(open_calls, 1);
    drive_audio_pump();
    drive_audio_pump();
    CHECK_EQ_INT(open_calls, 1);   /* no event, no try */
    CHECK_EQ_INT(hook_calls, 0);

    /* a device appears but cannot be opened: one try, no loop, the hook not called */
    added_events = 1;
    CHECK_EQ_INT(drive_audio_retry(), 0);
    CHECK_EQ_INT(open_calls, 2);
    drive_audio_pump();
    drive_audio_pump();
    CHECK_EQ_INT(open_calls, 2);   /* the event was taken: no second try until the next */
    CHECK_EQ_INT(hook_calls, 0);

    /* another device appears and opens: the mixer starts, the hook runs once */
    device_present = true;
    added_events = 1;
    CHECK_EQ_INT(drive_audio_retry(), 1);
    CHECK_EQ_INT(open_calls, 3);
    CHECK_EQ_INT(hook_calls, 1);
    CHECK_EQ_INT(hook_user, 1);

    /* open now: further events change nothing */
    added_events = 3;
    CHECK_EQ_INT(drive_audio_retry(), 0);
    drive_audio_pump();
    CHECK_EQ_INT(open_calls, 3);
    CHECK_EQ_INT(hook_calls, 1);

    /* closing ends the wait and the hook: a later device does not reopen what nobody asked for */
    const int closed_before = close_calls;
    drive_audio_close();
    CHECK(close_calls > closed_before);
    added_events = 1;
    device_present = true;
    CHECK_EQ_INT(drive_audio_retry(), 0);
    CHECK_EQ_INT(open_calls, 3);

    /* a start with a device never calls the hook for it */
    hook_calls = 0, added_events = 0;
    CHECK_EQ_INT(drive_audio_device_open("data"), 1);
    CHECK_EQ_INT(hook_calls, 0);
    drive_audio_close();
    return TEST_REPORT();
}
