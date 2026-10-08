/* 004376ac: the "Game Mode" menu (control/gamemode.qfs): Single player,
 * Modem/serial, Network, Exit. Selection follows the pointer over four
 * horizontal bands; Up/Down move the pointer to the next option; Enter,
 * Space or the left button choose; 45 s without input returns to the intro.
 * Field comments name the original's locals. */
#include "game/frontend/frontend.h"

#include "game/frontend/dialog.h"

#include "formats/fsh.h"
#include "game/sys/input.h"
#include "game/sys/palette.h"
#include "game/sys/pointer.h"
#include "game/sys/resource.h"
#include "game/sys/timer.h"
#include "game/sys/video.h"

#include <stdio.h>

enum {
    OPTION_SINGLE = 0,
    OPTION_MODEM = 1,
    OPTION_NETWORK = 2,
    OPTION_EXIT = 3,
    OPTION_COUNT = 4,
    IDLE_TIMEOUT = 4500,   /* 0x1194 ticks = 45 s */
    KEY_REPEAT_TICKS = 25
};

/* Pointer bands (00463f80 calls) and option centres (00432134). */
static const int BAND_TOP[OPTION_COUNT] = { 0, 178, 254, 330 };
static const int BAND_BOTTOM[OPTION_COUNT] = { 177, 253, 329, 480 };
static const int CENTRE_X[OPTION_COUNT] = { 320, 320, 320, 320 };
static const int CENTRE_Y[OPTION_COUNT] = { 145, 218, 290, 367 };
static const char LIT[OPTION_COUNT][5] = { "sinl", "modl", "netl", "exil" };  /* 004afaf8 */

typedef struct {
    uint8_t *data;
    Fsh fsh;
    uint8_t palette[PALETTE_BYTES];
    int selected;      /* local_1c */
    int previous;      /* local_2c */
    int changed;       /* local_28 */
    int redraw;        /* local_30 */
    int fade_in;       /* local_38 */
    int key_armed;     /* local_34 */
    int held_ticks;    /* local_3c */
    int button_armed;  /* local_40 */
    int buttons;       /* 00524792 */
    int done;          /* local_24 */
    int result;        /* local_20 */
} Menu;

static int draw_entry(const Menu *m, const char *name)
{
    uint32_t index;
    FshEntry e;
    if (fsh_find(&m->fsh, name, &index) != FSH_OK || fsh_entry(&m->fsh, index, &e) != FSH_OK) return 0;
    video_blit_at_own_position(&e);
    return 1;
}

static void follow_pointer(Menu *m)
{
    for (int band = 0; band < OPTION_COUNT; band++) {
        if (pointer_in_rect(0, 640, BAND_TOP[band], BAND_BOTTOM[band]) && m->selected != band) {
            m->previous = m->selected;
            m->selected = band;
            m->changed = 1;
            return;
        }
    }
}

static void redraw(Menu *m)
{
    video_clear_back();
    draw_entry(m, "bgnd");
    draw_entry(m, LIT[m->selected]);
    video_flip();
    pointer_show();
    if (m->fade_in) {
        video_fade_in(0, PALETTE_COLORS, m->palette);
        m->fade_in = 0;
        /* TODO(network): "Connection Terminated" after a dropped net game. */
    }
    timer_set_deadline(IDLE_TIMEOUT);
    m->changed = 0;
    m->redraw = 0;
}

static void choose(Menu *m)
{
    /* TODO(0046492c): select sound 0x48. */
    switch (m->selected) {
    case OPTION_SINGLE:
        /* TODO(004667fc): "Please Enter Name" dialog; needs FFN fonts. */
        m->done = 1;
        m->result = 0;
        break;
    case OPTION_MODEM:   /* TODO(00453000): modem / serial setup */
    case OPTION_NETWORK: /* TODO(00442514, 00454b78): network */
        m->redraw = 1;   /* as when no network service is found */
        timer_set_deadline(IDLE_TIMEOUT);
        break;
    default:
        m->done = 1;
        m->result = 1;
        break;
    }
}

static void exit_to_system(Menu *m)
{
    /* TODO(0046492c): select sound 0x48. */
    const char *title = resource_german() ? "Zur\xFC" "ck zum system ?" : "Exit to system ?";
    if (dialog_yes_no(title)) {
        m->done = 1;
        m->result = 1;
    }
}

static void move_selection(Menu *m, int step)
{
    m->previous = m->selected;
    m->selected = (m->selected + step + OPTION_COUNT) % OPTION_COUNT;
    pointer_move_to(CENTRE_X[m->selected], CENTRE_Y[m->selected]);
    m->changed = 1;
}

static void handle_key(Menu *m, int key)
{
    if (key == KEY_SPACE || key == KEY_ENTER || (m->buttons == POINTER_BUTTON_SELECT && m->button_armed)) {
        pointer_click_feedback();
        if (m->key_armed) choose(m);
        timer_ticks_since_last();
        m->button_armed = m->key_armed = m->held_ticks = 0;
    } else if (key == KEY_NONE || !m->key_armed) {
        if (key == KEY_NONE) m->key_armed = 1;
    } else {
        if (key == KEY_ESCAPE) exit_to_system(m);
        if (key == KEY_UP) move_selection(m, -1);
        if (key == KEY_DOWN) move_selection(m, +1);
        timer_set_deadline(IDLE_TIMEOUT);
        timer_ticks_since_last();
        m->key_armed = m->held_ticks = 0;
    }
}

/* Auto-repeat: a held key (other than the choose/back keys) re-arms after 0.25 s. */
static void update_repeat(Menu *m, int key)
{
    if (!m->key_armed && key != KEY_ESCAPE && key != KEY_SPACE && key != KEY_ENTER) {
        m->held_ticks += timer_ticks_since_last();
        if (m->held_ticks > KEY_REPEAT_TICKS) m->key_armed = 1;
    }
}

static void check_idle(Menu *m)
{
    if (m->fade_in || m->done) return;
    if (timer_deadline_passed()) {
        m->done = 1;
        m->result = 2;  /* back to the intro (attract mode) */
    } else if (m->buttons == POINTER_BUTTON_BACK && m->button_armed) {
        exit_to_system(m);
    }
}

static void step(Menu *m)
{
    follow_pointer(m);
    if (m->redraw) redraw(m);
    if (m->changed) {
        m->redraw = 1;  /* TODO(0046492c): move sound 0x47 when previous != selected */
        m->changed = 0;
    }
    update_repeat(m, input_get_key());
    if (pointer_active()) m->buttons = pointer_read_buttons();
    if (m->buttons == POINTER_BUTTON_NONE) m->button_armed = 1;
    pointer_activate_if_moved();
    handle_key(m, input_get_key());
    /* TODO(0046417c): service streamed music. */
    check_idle(m);
}

static int load(Menu *m)
{
    size_t size;
    uint32_t index;
    FshEntry pal;
    m->data = resource_load_art("control/gamemode.qfs", &size);
    return m->data != NULL && fsh_open(&m->fsh, m->data, size) == FSH_OK && fsh_find(&m->fsh, "!pal", &index) == FSH_OK
        && fsh_entry(&m->fsh, index, &pal) == FSH_OK && fsh_palette_rgb(&pal, m->palette) == FSH_OK;
}

int main_menu(void)
{
    Menu m = { 0 };
    m.redraw = m.fade_in = m.key_armed = m.button_armed = 1;
    pointer_move_to(CENTRE_X[0], CENTRE_Y[0]);
    /* TODO(004648dc): load front_mw.bnk menu sounds. */
    if (!load(&m)) {
        fprintf(stderr, "main_menu: cannot load control/gamemode.qfs\n");
        resource_free(m.data);
        return 1;
    }
    while (!m.done) step(&m);
    video_fade_out(0, PALETTE_COLORS);
    video_set_overlay(NULL, 0, 0);  /* 00474558 */
    resource_free(m.data);
    return m.result;
}
