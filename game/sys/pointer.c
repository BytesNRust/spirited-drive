#include "game/sys/pointer.h"

#include "formats/fsh.h"
#include "game/sys/input.h"
#include "game/sys/pointer_state.h"
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
static PointerState ptr = { .x = 320, .y = 240 };
static unsigned os_buttons;
static int16_t *bound;           /* pointer_bind_active's copy */

static void active_set(int16_t active)
{
    ptr.active = active;
    if (bound != NULL) *bound = active;
}

int16_t *pointer_active_word(void)
{
    return &ptr.active;
}

int16_t *pointer_bind_active(int16_t *copy)
{
    int16_t *const was = bound;
    bound = copy;
    if (bound != NULL) *bound = ptr.active;
    return was;
}

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
    active_set(0);
}

void pointer_sync_from_os(void)
{
    int os_x, os_y;
    if (platform_mouse(&os_x, &os_y, &os_buttons)) {
        ptr.x = os_x;
        ptr.y = os_y;
    }
}

void pointer_move_to(int px, int py)
{
    ptr.x = px;
    ptr.y = py;
    ptr.mark_x = ptr.x;
    ptr.mark_y = ptr.y;
}

int pointer_in_rect(int x0, int x1, int y0, int y1)
{
    return pointer_state_in_rect(&ptr, x0, x1, y0, y1);
}

void pointer_activate_if_moved(void)
{
    ptr.buttons = (int16_t)os_buttons;  /* the original's 00524792; the port reads the OS buttons */
    pointer_state_activate_if_moved(&ptr);
    active_set(ptr.active);
}

static void off_poll(void *ctx)
{
    (void)ctx;
    pointer_sync_from_os();
}

static void off_hide(void *ctx)
{
    (void)ctx;
    video_set_overlay(NULL, 0, 0);
}

void pointer_off(void)
{
    const PointerOffOps ops = { NULL, off_poll, off_hide };
    pointer_state_off(&ptr, &ops);
    active_set(ptr.active);
}

int pointer_active(void)
{
    return ptr.active;
}

static void draw(void)
{
    video_set_overlay(data != NULL ? &sprite : NULL, ptr.x, ptr.y);
    ptr.mark_x = ptr.x;
    ptr.mark_y = ptr.y;
}

void pointer_show(void)
{
    draw();
    active_set(1);
}

static int buttons_now(void)
{
    if (os_buttons & PLATFORM_MOUSE_LEFT) return POINTER_BUTTON_SELECT;
    if (os_buttons & PLATFORM_MOUSE_RIGHT) return POINTER_BUTTON_BACK;
    return POINTER_BUTTON_NONE;
}

int pointer_read_buttons(void)
{
    if (ptr.x != ptr.mark_x || ptr.y != ptr.mark_y) draw();
    return buttons_now();
}

int pointer_buttons_unseen(void) { return buttons_now(); }

void pointer_click_feedback(void)
{
    /* TODO(004416cc): the original redraws the pointer in its pressed state. */
    timer_set_deadline(PRESS_TICKS);
    while (!timer_deadline_passed()) {
        input_pump();
        /* TODO(0046417c): service streamed music. */
    }
    timer_set_deadline(IDLE_TICKS_AFTER);
}

void pointer_position(int *x, int *y)
{
    *x = ptr.x, *y = ptr.y;
}

const uint8_t *pointer_shape(int which)
{
    enum { ENTRY_HEADER = 16 };
    const char name[4] = { 'p', 't', 'r', (char)('0' + which) };
    uint32_t index;
    FshEntry e;
    if (data == NULL || which < 0 || which > 2 || fsh_find(&fsh, name, &index) != FSH_OK
        || fsh_entry(&fsh, index, &e) != FSH_OK)
        return NULL;
    return e.payload - ENTRY_HEADER;
}

void pointer_mark(void)
{
    ptr.mark_x = (int16_t)ptr.x;
    ptr.mark_y = (int16_t)ptr.y;
}
