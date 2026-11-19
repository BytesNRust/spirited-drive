#include "game/frontend/added_gallery_run.h"

#include "game/frontend/gallery_layout.h"
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

/* port (owner 2026-11-17): the item under the pointer lit, as the showcase lights its own, each frame the pointer moves (a box the page does
 * not visit, a fixed page's or Color while it is off, is not); 1 when the item changed */
static int follow(const CarPieceCard *c, GalleryState *s, int x, int y)
{
    int step;
    (void)c;
    const int item = gallery_hit(x, y, &step);
    if (s->setup || item < 0 || item == s->item) return 0;   /* the panel open: the pointer lights nothing under it */
    if (item == GALLERY_PHOTOS && s->photos < 2) return 0;   /* a photo that is alone has nothing to step */
    s->item = item;
    return 1;
}

/* port (PR B): the wheel over (or with the keys on) the description, and its automatic scroll; 1 when the scroll changed */
static int description(const GalleryRunOps *o, GalleryState *s, GalleryScroll *scroll, int x, int y)
{
    int step, visible = 0;
    const int lines = o->lines(o->ctx, &visible), on = gallery_hit(x, y, &step) == GALLERY_HISTORY, wheel = o->wheel(o->ctx);
    const int was = s->scroll;
    if (wheel != 0 && (on || s->item == GALLERY_HISTORY)) {
        s->scroll -= wheel * GALLERY_SCROLL_WHEEL_PX;   /* up is towards the top */
        added_gallery_scroll_touch(scroll, o->now(o->ctx));
    }
    added_gallery_scroll_auto(scroll, o->now(o->ctx), &s->scroll, lines, visible, on || s->item == GALLERY_HISTORY);
    return s->scroll != was;
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
    int last_key = -1, last_buttons = -1, result = GALLERY_STAY, changed = 0, last_x = 0, last_y = 0;
    GalleryScroll scroll = { 0, 0 };
    int lit = s->item;
    o->pointer(o->ctx, &last_x, &last_y);
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
        int x, y;
        o->pointer(o->ctx, &x, &y);
        if (x != last_x || y != last_y) changed |= follow(c, s, x, y);
        last_x = x, last_y = y;
        if (press != 0) {
            const GalleryState was = *s;
            result = added_gallery_input(c, s, press);
            changed |= memcmp(&was, s, sizeof was) != 0;
            if (was.scroll != s->scroll) added_gallery_scroll_touch(&scroll, o->now(o->ctx));   /* by hand: the clock starts over */
            if (was.setup != s->setup) o->shutter(o->ctx, s, s->setup);
            if (was.transmission != s->transmission || was.abs != s->abs || was.traction != s->traction) o->apply(o->ctx, s);
            if (was.setup && s->setup && (was.paint != s->paint || was.skin != s->skin || was.transmission != s->transmission || was.abs != s->abs ||
                                          was.traction != s->traction))
                o->chosen(o->ctx);   /* a value of the open panel changed: Options' click, as its rows give when chosen */
        }
        changed |= description(o, s, &scroll, x, y);
        if (s->item != lit && !s->setup) o->moved(o->ctx);   /* as the CD's page sounds a tile lit */
        lit = s->item;
        o->music(o->ctx);   /* port (owner 2026-11-17): the showcase's music plays on here, as on the showcase's own pages */
        o->wait(o->ctx);
    }
    let_go(o);
    o->fade_out(o->ctx);   /* out as the showcase goes out, whatever comes next */
    return result;
}
