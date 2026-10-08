/* 00465f4c: modal Yes/No box over the current screen. A 340x100 box at
 * (150, 190): the screen behind darkened (0048263c with 0x9000), a red
 * double outline, the title centred in red, "Yes" and "No" ("Ja"/"Nein")
 * with the selection in yellow. No is selected first. */
#include "game/frontend/dialog.h"

#include "game/sys/input.h"
#include "game/sys/palette.h"
#include "game/sys/pointer.h"
#include "game/sys/resource.h"
#include "game/sys/text.h"
#include "game/sys/video.h"

#include <string.h>

enum {
    BOX_X = 150, BOX_Y = 190, BOX_W = 340, BOX_H = 100,
    SHADE = 0x9000,
    CHOICE_YES = 0, CHOICE_NO = 1,
    SC_Y = 0x15, SC_J = 0x24, SC_N = 0x31
};

/* Option text (box-relative 60,60 and 250,60) and pointer targets (00461f4c). */
static const int OPTION_X[2] = { BOX_X + 60, BOX_X + 250 };
static const int OPTION_Y = BOX_Y + 60;
static const int POINTER_X[2] = { 225, 420 };
static const int POINTER_Y = 260;

typedef struct {
    int selected;      /* uVar12 */
    int redraw;        /* local_24 */
    int key_armed;     /* local_28 */
    int button_armed;  /* local_30[2] */
    int buttons;       /* 00524792 */
    int done;
    int result;        /* local_20 */
} Dialog;

static const char *option_label(int option)
{
    if (option == CHOICE_YES) return resource_german() ? "Ja" : "Yes";
    return resource_german() ? "Nein" : "No";
}

static void draw_box(const char *title)
{
    uint8_t pal[PALETTE_BYTES], usable[256], shade[256];
    video_get_palette(pal);
    memset(usable, 1, sizeof usable);
    usable[254] = usable[255] = 0;
    palette_shade_table(pal, usable, SHADE, shade);
    video_shade_rect(BOX_X, BOX_Y, BOX_W, BOX_H, shade);
    /* TODO(004827f0): outline geometry read from a partial decompile. */
    const uint8_t red = (uint8_t)palette_nearest(pal, usable, 0xFF, 0x00, 0x00);
    video_frame_rect(BOX_X + 2, BOX_Y + 2, BOX_X + BOX_W - 3, BOX_Y + BOX_H - 3, red);
    video_frame_rect(BOX_X + 3, BOX_Y + 3, BOX_X + BOX_W - 4, BOX_Y + BOX_H - 4, red);
    text_build_styles();
    text_select(TEXT_RED);
    text_draw_centered(title, BOX_X + 170, BOX_Y + 20);
}

static void draw_options(int selected)
{
    for (int option = CHOICE_YES; option <= CHOICE_NO; option++) {
        text_select(option == selected ? TEXT_YELLOW : TEXT_CREAM);
        text_draw_shadowed(option_label(option), OPTION_X[option], OPTION_Y);
    }
    video_flip();
    pointer_show();
}

static void follow_pointer(Dialog *d)
{
    if (pointer_in_rect(190, 260, 245, 280) && d->selected != CHOICE_YES) {
        d->selected = CHOICE_YES;
        d->redraw = 1;
    } else if (pointer_in_rect(380, 460, 245, 280) && d->selected != CHOICE_NO) {
        d->selected = CHOICE_NO;
        d->redraw = 1;
    }
}

static void finish(Dialog *d, int result)
{
    /* TODO(0046492c): select sound 0x48. */
    d->done = 1;
    d->result = result;
}

static void handle_key(Dialog *d, int key)
{
    if (key == KEY_SPACE || key == KEY_ENTER || (d->buttons != POINTER_BUTTON_NONE && d->button_armed)) {
        pointer_click_feedback();
        if (d->key_armed) finish(d, d->buttons != POINTER_BUTTON_BACK && d->selected == CHOICE_YES);
    } else if (key == KEY_NONE || !d->key_armed) {
        if (key == KEY_NONE) d->key_armed = 1;
    } else {
        if ((key == SC_Y && !resource_german()) || (key == SC_J && resource_german())) finish(d, 1);
        if (key == KEY_ESCAPE || key == SC_N) finish(d, 0);
        if (key == KEY_LEFT || key == KEY_RIGHT) {
            d->selected ^= 1;
            pointer_move_to(POINTER_X[d->selected], POINTER_Y);
            d->redraw = 1;
        }
        d->key_armed = 0;
    }
}

int dialog_yes_no(const char *title)
{
    uint8_t saved[BOX_W * BOX_H];
    Dialog d = { CHOICE_NO, 1, 0, 0, 0, 0, 0 };
    video_copy_front_to_back();
    video_read_rect(BOX_X, BOX_Y, BOX_W, BOX_H, saved);
    if (!pointer_active()) pointer_move_to(POINTER_X[CHOICE_NO], POINTER_Y);
    draw_box(title);

    while (input_get_key() != KEY_NONE) {
        /* TODO(0046417c): service streamed music. */
    }
    while (!d.done) {
        follow_pointer(&d);
        if (d.redraw) {
            draw_options(d.selected);  /* TODO(0046492c): move sound 0x47 */
            d.redraw = 0;
        }
        if (pointer_active()) d.buttons = pointer_read_buttons();
        if (d.buttons == POINTER_BUTTON_NONE) d.button_armed = 1;
        pointer_activate_if_moved();
        handle_key(&d, input_get_key());
    }
    video_write_rect(BOX_X, BOX_Y, BOX_W, BOX_H, saved);
    video_flip();
    pointer_show();
    return d.result;
}
