#include "game/sys/pointer.h"

#include "formats/fsh.h"
#include "game/sys/resource.h"
#include "game/sys/timer.h"
#include "game/sys/video.h"
#include "platform/platform.h"

#include <stdio.h>
#include <string.h>

enum {
    PRESS_TICKS = 20,        /* 00441814: pressed look for 0.2 s */
    IDLE_TICKS_AFTER = 12000 /* then a 120 s deadline */
};

static uint8_t *data;
static Fsh fsh;
static FshEntry sprite;          /* ptr0 */
static int x = 320, y = 240;     /* 004c6110 / 004c6114 */
static int saved_x, saved_y;     /* 00524794 / 00524796 (00441858) */
static int active;               /* 00524790 */
static unsigned os_buttons;

int pointer_load(int variant)
{
    pointer_free();
    size_t size;
    data = resource_load_art(variant ? "gmouse.fsh" : "mouse.fsh", &size);
    uint32_t index;
    if (data == NULL || fsh_open(&fsh, data, size) != FSH_OK || fsh_find(&fsh, "ptr0", &index) != FSH_OK
        || fsh_entry(&fsh, index, &sprite) != FSH_OK) {
        fprintf(stderr, "pointer: cannot load %s\n", variant ? "gmouse.fsh" : "mouse.fsh");
        pointer_free();
        return 0;
    }
    return 1;
}

void pointer_free(void)
{
    video_set_overlay(NULL, 0, 0);
    resource_free(data);
    data = NULL;
    active = 0;
}

void pointer_sync_from_os(void)
{
    int os_x, os_y;
    if (platform_mouse(&os_x, &os_y, &os_buttons)) {
        x = os_x;
        y = os_y;
    }
}

void pointer_move_to(int px, int py)
{
    x = px;
    y = py;
    saved_x = x;
    saved_y = y;
}

int pointer_in_rect(int x0, int x1, int y0, int y1)
{
    return x0 <= x && x <= x1 && y0 <= y && y <= y1;
}

void pointer_activate_if_moved(void)
{
    if (!active && (os_buttons != 0 || saved_x != x || saved_y != y)) active = 1;
}

int pointer_active(void)
{
    return active;
}

static void draw(void)
{
    video_set_overlay(data != NULL ? &sprite : NULL, x, y);
    saved_x = x;
    saved_y = y;
}

void pointer_show(void)
{
    draw();
    active = 1;
}

int pointer_read_buttons(void)
{
    if (x != saved_x || y != saved_y) draw();
    if (os_buttons & PLATFORM_MOUSE_LEFT) return POINTER_BUTTON_SELECT;
    if (os_buttons & PLATFORM_MOUSE_RIGHT) return POINTER_BUTTON_BACK;
    return POINTER_BUTTON_NONE;
}

void pointer_click_feedback(void)
{
    /* TODO(004416cc): the original redraws the pointer in its pressed state. */
    timer_set_deadline(PRESS_TICKS);
    while (!timer_deadline_passed()) {
        platform_poll_events();
        platform_sleep_ms(1);
        /* TODO(0046417c): service streamed music. */
    }
    timer_set_deadline(IDLE_TICKS_AFTER);
}
