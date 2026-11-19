/* port: see ui_kit.h. */
#include "game/frontend/ui_kit.h"

#include <stdio.h>
#include <string.h>

enum { CREAM = 0, LABEL_MAX = 32 };

void ui_arrow_mask(int right, uint8_t mask[UI_ARROW_MASK_W * UI_ARROW_MASK_H])
{
    memset(mask, 0, (size_t)UI_ARROW_MASK_W * UI_ARROW_MASK_H);
    for (int r = 0; r < UI_ARROW_INK_H; r++) {
        const int up = r < UI_ARROW_INK_H - 1 - r ? r : UI_ARROW_INK_H - 1 - r;
        for (int c = 0; c <= up; c++) {
            const int x = right ? c : UI_ARROW_INK_W - 1 - c;
            mask[(UI_ARROW_PAD + r) * UI_ARROW_MASK_W + UI_ARROW_PAD + x] = 15;
        }
    }
}

UiPagerLayout ui_pager_layout(int text_w, int right_w)
{
    UiPagerLayout l;
    l.text_x = PAGE_NAV_CENTRE - text_w / 2;
    /* the masks' own padding is blank too: a triangle's ink starts UI_ARROW_PAD columns into its mask */
    l.left_x = l.text_x + UI_PAGER_BEARING_LEFT - UI_PAGER_GAP_LEFT - UI_ARROW_INK_W - UI_ARROW_PAD;
    l.right_x = l.text_x + text_w - UI_PAGER_BEARING_RIGHT + UI_PAGER_GAP_RIGHT - UI_ARROW_PAD;
    l.fits = l.left_x >= PAGE_NAV_X0 + 2 && l.right_x <= PAGE_NAV_X1 - 2 - right_w;
    return l;
}

/* the count's words at the buttons' size when the screen can draw them so, else at 1/1 through its plain text slots */
static int scaled_ok(const PageNavOps *o)
{
    return o->scaled.width != NULL && (o->clean ? o->scaled.clean : o->scaled.plain) != NULL;
}

static int word_width(const PageNavOps *o, const char *text)
{
    return scaled_ok(o) ? o->scaled.width(o->ctx, text, UI_PAGER_TEXT_NUM, UI_PAGER_TEXT_DEN) : o->text_width(o->ctx, text);
}

static void word_draw(const PageNavOps *o, const char *text, int x)
{
    if (!scaled_ok(o)) {
        o->text(o->ctx, CREAM, text, x, PAGE_NAV_TEXT_Y);
        return;
    }
    (o->clean ? o->scaled.clean : o->scaled.plain)(o->ctx, CREAM, text, x, UI_PAGER_TEXT_Y, UI_PAGER_TEXT_NUM, UI_PAGER_TEXT_DEN);
}

void ui_pager(const PageNavOps *o, const char *label, int has_left, int has_right, int lit)
{
    int32_t lw, lh, rw, rh;
    const int arrows = o->arrow != NULL && o->arrow_size != NULL && o->arrow_size(o->ctx, 0, &lw, &lh) &&
                       o->arrow_size(o->ctx, 1, &rw, &rh);
    char text[LABEL_MAX];
    const UiPagerLayout l = arrows ? ui_pager_layout(word_width(o, label), rw) : (UiPagerLayout){ 0 };
    if (!arrows || !l.fits) {
        snprintf(text, sizeof text, "%s%s%s", has_left ? "< " : "", label, has_right ? " >" : "");
        word_draw(o, text, PAGE_NAV_CENTRE - word_width(o, text) / 2);
        return;
    }
    word_draw(o, label, l.text_x);
    if (has_left) o->arrow(o->ctx, 0, lit == 1, l.left_x, PAGE_NAV_ARROW_MID - lh / 2);
    if (has_right) o->arrow(o->ctx, 1, lit == 2, l.right_x, PAGE_NAV_ARROW_MID - rh / 2);
}

void ui_page_count(const PageNavOps *o, int page, int pages, int lit)
{
    char label[LABEL_MAX];
    snprintf(label, sizeof label, "%d/%d", page + 1, pages);
    ui_pager(o, label, page > 0, page < pages - 1, lit);
}
