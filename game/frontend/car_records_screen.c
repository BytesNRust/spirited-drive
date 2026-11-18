/* See car_records_screen.h. */
#include "game/frontend/car_records_screen.h"

#include "game/frontend/added_best_art.h"
#include "game/frontend/added_gallery.h"
#include "game/frontend/car_records_view.h"
#include "game/frontend/central_glue_private.h"
#include "game/frontend/ea_lib.h"
#include "game/frontend/menu_audio.h"
#include "game/frontend/palette_put.h"
#include "game/frontend/text_wrap.h"
#include "game/sys/input.h"
#include "game/sys/pointer.h"
#include "game/sys/port_log.h"
#include "game/sys/resource.h"
#include "game/sys/text.h"
#include "game/sys/video.h"

#include <stdlib.h>
#include <string.h>

enum { TITLE_X = 12, TITLE_Y = 24, TITLE_W = 312,   /* where an added track's name stands on Best Times' art (best_times.c) */
       DONE_RIGHT = 0x82, FOOTER_TOP = 0x1b3, SOUND_PAGE = 0x47, SOUND_CHOSEN = 0x48, LET_GO_FRAMES = 30 };

typedef struct {
    int32_t bank;
    const char *name;
    uint8_t rgb[PALETTE_PUT_BYTES];
} Screen;

/* one frame of the screen into the back buffer */
static void screen_frame(const Screen *s, const CarRecordsView *v)
{
    ea_select_default();
    ea_blit_own(ea_shape(s->bank, "bgnd"));
    ea_blit_own(ea_shape(s->bank, "donl"));   /* DONE, lit: the only item, as Best Times' on entry */
    text_select(TEXT_YELLOW);
    cg_fit(NULL, s->name, TITLE_X, TITLE_Y, TITLE_W);
    car_records_view_draw(v, resource_kmh());
}

static void screen_show(const uint8_t *palette)
{
    video_set_palette(0, 256, palette);
    video_flip();
    pointer_show();
}

void car_records_screen(const char *id, const char *name)
{
    CarRecordsView *v = malloc(sizeof *v);
    TrackList l;
    char said[64];
    text_words(name != NULL ? name : "", said, sizeof said);   /* the main face: its ',' is a comma */
    Screen s = { added_best_art_records_bank(), said, { 0 } };
    if (v == NULL || s.bank == 0) {
        free(v);
        if (s.bank != 0) ea_bank_free(s.bank);
        return;
    }
    cg_records_tracks(&l);
    car_records_view_make(v, id, &l);
    const uint8_t *pal = ea_shape(s.bank, "!pal");
    uint8_t scratch[PALETTE_PUT_BYTES], black[PALETTE_PUT_BYTES];
    palette_put(s.rgb, 0, 0x100, pal + 0x10, pal[0], scratch);   /* the bank's palette: the screen's own, as every screen's */
    memset(black, 0, sizeof black);
    text_build_styles_from(s.rgb);
    if (!text_small_init()) port_log("car records: the small font is not loaded\n");
    port_log("mark:screen=car_records\n");   /* port: the headless walk waits for it */
    int last_key = input_get_key(), last_buttons = pointer_read_buttons(), done = 0, changed = 1, first = 1;
    while (!done) {
        if (changed) {
            screen_frame(&s, v);
            if (first) {   /* in from black */
                screen_show(black);
                video_fade_in(0, 256, s.rgb);
                first = 0;
            } else screen_show(s.rgb);
            port_log("mark:records_page=%d\n", v->page);
            changed = 0;
        }
        const int key = input_get_key(), buttons = pointer_read_buttons();
        const int press = key != last_key && key != 0 ? key : 0;
        if (buttons != last_buttons && (buttons & POINTER_BUTTON_SELECT)) {
            int x, y;
            pointer_position(&x, &y);
            const int step = car_records_view_hit(v, x, y);
            if (step != 0) car_records_view_step(v, step), changed = 1, menu_audio_sound(SOUND_PAGE);
            else if (x <= DONE_RIGHT && y >= FOOTER_TOP) done = 1;
        } else if (buttons != last_buttons && (buttons & POINTER_BUTTON_BACK)) done = 1;
        last_key = key, last_buttons = buttons;
        if (press == GALLERY_KEY_ESCAPE || press == GALLERY_KEY_ENTER) done = 1;
        else if (press == GALLERY_KEY_LEFT) car_records_view_step(v, -1), changed = 1, menu_audio_sound(SOUND_PAGE);
        else if (press == GALLERY_KEY_RIGHT) car_records_view_step(v, 1), changed = 1, menu_audio_sound(SOUND_PAGE);
        menu_audio_music_service();
        video_wait_vblank();
    }
    menu_audio_sound(SOUND_CHOSEN);
    video_fade_out(0, 256);   /* out to black: the page it came from fades in */
    for (int i = 0; i < LET_GO_FRAMES; i++) {   /* the press that closed it let go, so the page does not act on it */
        if (input_get_key() == 0 && (pointer_read_buttons() & POINTER_BUTTON_SELECT) == 0) break;
        menu_audio_music_service();
        video_wait_vblank();
    }
    ea_bank_free(s.bank);
    free(v);
}
