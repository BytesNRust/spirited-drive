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
static int range_x0, range_y0, range_x1 = VIDEO_WIDTH, range_y1 = VIDEO_HEIGHT;  /* 0052cdac, 0052cdb0, 0052cda4, 0052cda0 */

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

/* 004772f8's clamp: the low bound first, the high one winning */
static int clamp_to(int v, int lo, int hi)
{
    const int up = v > lo ? v : lo;
    return up < hi ? up : hi;
}

static void clamp(void)
{
    ptr.x = clamp_to(ptr.x, range_x0, range_x1);
    ptr.y = clamp_to(ptr.y, range_y0, range_y1);
}

void pointer_range(int x0, int y0, int x1, int y1)
{
    range_x0 = x0, range_y0 = y0, range_x1 = x1, range_y1 = y1;
    clamp();
}

void pointer_sync_from_os(void)
{
    int os_x, os_y;
    if (platform_mouse(&os_x, &os_y, &os_buttons)) {
        ptr.x = os_x;
        ptr.y = os_y;
        clamp();
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

void pointer_start(void)
{
    active_set(1);
    os_buttons = 0;
}

int pointer_active(void)
{
    return ptr.active;
}

static void (*wait_service)(void);  /* 0046417c, the music, while the click waits */

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

void pointer_on_wait(void (*service)(void)) { wait_service = service; }

void pointer_click_feedback(void)
{
    /* 004416cc(1, 1) and (0, 1): a forced redraw on each side of the wait; its first argument is not read, the
     * pointer looks the same pressed */
    draw();
    timer_set_deadline(PRESS_TICKS);
    while (!timer_deadline_passed()) {
        input_pump();
        if (wait_service != NULL) wait_service();  /* 0046417c */
    }
    draw();
    timer_set_deadline(IDLE_TICKS_AFTER);
}

void pointer_position(int *x, int *y)
{
    *x = clamp_to(ptr.x, range_x0, range_x1), *y = clamp_to(ptr.y, range_y0, range_y1);  /* 00477140's */
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
