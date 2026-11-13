/* Full-screen art pages of the front end: EA logo and title. */
#include "game/frontend/frontend.h"

#include "game/frontend/menu_audio.h"

#include "formats/fsh.h"
#include "game/sys/input.h"
#include "game/sys/palette.h"
#include "game/sys/pointer.h"
#include "game/sys/port_log.h"
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

/* 00466f68's wait: the music serviced (0046417c) when it plays, a mouse button ([004c6194] into 00524792, the
 * pointer active) or space / enter / escape skip */
static int wait_for_skip(int ticks, int music)
{
    timer_set_deadline(ticks);
    int key_armed = 0, buttons_armed = 0;   /* port (fresh press, title.c): what was held before the screen counts once let go */
    while (!timer_deadline_passed()) {
        if (music) menu_audio_music_service();
        int buttons = 0;
        if (pointer_active()) pointer_sync_from_os(), buttons = pointer_buttons_unseen();
        const int key = input_get_key();
        key_armed |= key == 0, buttons_armed |= buttons == 0;
        if ((is_skip_key(key) && key_armed) || (buttons != 0 && buttons_armed)) return 1;
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
        port_log("show_screen: cannot show %s\n", name);  /* the original aborts (00477a40) */
        resource_free(data);
        return 0;
    }

    video_blit_at_own_position(&background);
    video_flip();
    if (mode != SCREEN_INSTANT) video_fade_in(0, PALETTE_COLORS, rgb);

    int skipped = 0;
    if (mode == SCREEN_WAIT) {
        port_log("mark:screen=%s\n", name);   /* port: a headless walk can wait for it and measure it */
        const uint32_t start = timer_ticks();
        skipped = wait_for_skip(music ? WAIT_TICKS_MUSIC : WAIT_TICKS, music);
        port_log("mark:hold screen=%s ticks=%u skipped=%d\n", name, (unsigned)(timer_ticks() - start), skipped);
        video_fade_out(0, PALETTE_COLORS);
    }
    resource_free(data);
    return skipped;
}
