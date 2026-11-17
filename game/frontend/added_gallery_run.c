#include "game/frontend/added_gallery_run.h"

#include "game/sys/pointer.h"

#include <string.h>

/* the keys and the pointer's button let go (or GALLERY_RELEASE_FRAMES gone) before the page closes: a press it acted on, still held, is
 * not taken by the screen after it (the showcase takes a held button or Enter at once: a click on Previous as its Done, which sits
 * there; Enter on Next as the photo under the pointer) */
static void let_go(const GalleryRunOps *o)
{
    for (int i = 0; i < GALLERY_RELEASE_FRAMES; i++) {
        const int key = o->key(o->ctx);   /* first: it pumps the events the buttons are read from */
        if (key == 0 && (o->buttons(o->ctx) & POINTER_BUTTON_SELECT) == 0) return;
        o->music(o->ctx);
        o->wait(o->ctx);
    }
}

int added_gallery_run(const CarPieceCard *c, GalleryState *s, const GalleryRunOps *o)
{
    /* port (owner 2026-11-17): the page comes up as the showcase's does: both pictures cleared and the palette black before it is drawn,
     * then faded in, so neither the screen before it nor its own colours on that screen ever show for a frame */
    o->clear(o->ctx);
    o->black(o->ctx);
    o->draw(o->ctx, s);
    o->flip(o->ctx, s);
    o->fade_in(o->ctx);
    int last_key = -1, last_buttons = -1, result = GALLERY_STAY, changed = 0;
    while (result == GALLERY_STAY) {
        if (changed) {   /* the page drawn again: its palette (the swatch's colour) with it */
            o->draw(o->ctx, s);
            o->palette(o->ctx);
            o->flip(o->ctx, s);
            changed = 0;
        }
        const int key = o->key(o->ctx);
        const int buttons = o->buttons(o->ctx);
        int press = key != last_key && key != 0 ? key : 0;
        if (buttons != last_buttons && last_buttons >= 0) {
            if (buttons & POINTER_BUTTON_SELECT) press = o->click(o->ctx, s);
            else if (buttons & POINTER_BUTTON_BACK) press = GALLERY_KEY_ESCAPE;
        }
        last_key = key, last_buttons = buttons;
        if (press != 0) {
            const GalleryState was = *s;
            result = added_gallery_input(c, s, press);
            changed = memcmp(&was, s, sizeof was) != 0;
        }
        o->music(o->ctx);   /* port (owner 2026-11-17): the showcase's music plays on here, as on the showcase's own pages */
        o->wait(o->ctx);
    }
    let_go(o);
    o->fade_out(o->ctx);   /* out as the showcase goes out, whatever comes next */
    return result;
}
