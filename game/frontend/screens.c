/* Full-screen art pages of the front end: EA logo and title. */
#include "game/frontend/frontend.h"

#include "formats/fsh.h"
#include "game/sys/input.h"
#include "game/sys/palette.h"
#include "game/sys/resource.h"
#include "game/sys/timer.h"
#include "game/sys/video.h"

#include <stdio.h>

enum {
    WAIT_TICKS = 500,          /* 5 s at 100 Hz */
    WAIT_TICKS_MUSIC = 300
};

static int is_skip_key(int key)
{
    return key == KEY_SPACE || key == KEY_ENTER || key == KEY_ESCAPE;
}

/* Finds "!pal" and "bgnd" the way 004732dd does (exact names). */
static int find_art(const Fsh *fsh, FshEntry *palette, FshEntry *background)
{
    uint32_t pal_index, bg_index;
    return fsh_find(fsh, "!pal", &pal_index) == FSH_OK && fsh_entry(fsh, pal_index, palette) == FSH_OK
        && fsh_find(fsh, "bgnd", &bg_index) == FSH_OK && fsh_entry(fsh, bg_index, background) == FSH_OK
        && background->type == FSH_TYPE_BITMAP;
}

static int wait_for_skip(int ticks)
{
    timer_set_deadline(ticks);
    while (!timer_deadline_passed()) {
        /* TODO(0046417c): service streamed music; TODO: joystick buttons (004c6194). */
        if (is_skip_key(input_get_key())) return 1;
    }
    return 0;
}

int show_screen(const char *name, int mode, int music)
{
    if (mode != SCREEN_INSTANT) {
        video_clear_back();
        video_flip();
    }
    size_t size;
    uint8_t *data = resource_load_art(name, &size);
    Fsh fsh;
    FshEntry pal_entry, background;
    uint8_t rgb[PALETTE_BYTES];
    if (data == NULL || fsh_open(&fsh, data, size) != FSH_OK || !find_art(&fsh, &pal_entry, &background)
        || fsh_palette_rgb(&pal_entry, rgb) != FSH_OK) {
        fprintf(stderr, "show_screen: cannot show %s\n", name);  /* the original aborts (00477a40) */
        resource_free(data);
        return 0;
    }

    video_blit_at_own_position(&background);
    video_flip();
    if (mode != SCREEN_INSTANT) video_fade_in(0, PALETTE_COLORS, rgb);

    int skipped = 0;
    if (mode == SCREEN_WAIT) {
        skipped = wait_for_skip(music ? WAIT_TICKS_MUSIC : WAIT_TICKS);
        video_fade_out(0, PALETTE_COLORS);
    }
    resource_free(data);
    return skipped;
}
