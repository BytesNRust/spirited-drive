/* port: see ui_kit.h. */
#include "game/frontend/ui_kit.h"

#include <stdio.h>
#include <string.h>

enum { CREAM = 0, LABEL_MAX = 32 };

void ui_arrow_mask(int right, uint8_t mask[UI_ARROW_MASK_W * UI_ARROW_MASK_H])
{
    memset(mask, 0, (size_t)UI_ARROW_MASK_W * UI_ARROW_MASK_H);
    for (int r = 0; r < UI_ARROW_TRI_H; r++) {
        const int up = r < UI_ARROW_TRI_H - 1 - r ? r : UI_ARROW_TRI_H - 1 - r;
        const int width = up + 1 < UI_ARROW_CORE_W ? up + 1 : UI_ARROW_CORE_W;
        for (int c = 0; c < width; c++) {
            const int x = right ? c : UI_ARROW_CORE_W - 1 - c;
            mask[(UI_ARROW_PAD + (right ? 1 : 0) + r) * UI_ARROW_MASK_W + UI_ARROW_PAD + x] = 15;
        }
    }
}

UiPagerLayout ui_pager_layout(int text_w, int right_w)
{
    UiPagerLayout l;
    l.text_x = PAGE_NAV_CENTRE - text_w / 2;
    /* the masks' own padding is blank too: a triangle's ink starts UI_ARROW_PAD columns into its mask */
    l.left_x = l.text_x + UI_PAGER_TEXT_BEARING - UI_PAGER_GAP_LEFT - UI_ARROW_INK_W - UI_ARROW_PAD;
    l.right_x = l.text_x + text_w - UI_PAGER_TEXT_BEARING + UI_PAGER_GAP_RIGHT - UI_ARROW_PAD;
    l.fits = l.left_x >= PAGE_NAV_X0 + 2 && l.right_x <= PAGE_NAV_X1 - 2 - right_w;
    return l;
}

void ui_pager(const PageNavOps *o, const char *label, int has_left, int has_right, int lit)
{
    int32_t lw, lh, rw, rh;
    const int arrows = o->arrow != NULL && o->arrow_size != NULL && o->arrow_size(o->ctx, 0, &lw, &lh) &&
                       o->arrow_size(o->ctx, 1, &rw, &rh);
    char text[LABEL_MAX];
    const UiPagerLayout l = arrows ? ui_pager_layout(o->text_width(o->ctx, label), rw) : (UiPagerLayout){ 0 };
    if (!arrows || !l.fits) {
        snprintf(text, sizeof text, "%s%s%s", has_left ? "< " : "", label, has_right ? " >" : "");
        o->text(o->ctx, CREAM, text, PAGE_NAV_CENTRE - o->text_width(o->ctx, text) / 2, PAGE_NAV_TEXT_Y);
        return;
    }
    o->text(o->ctx, CREAM, label, l.text_x, PAGE_NAV_TEXT_Y);
    if (has_left) o->arrow(o->ctx, 0, lit == 1, l.left_x, PAGE_NAV_ARROW_MID - lh / 2);
    if (has_right) o->arrow(o->ctx, 1, lit == 2, l.right_x, PAGE_NAV_ARROW_MID - rh / 2);
}

void ui_page_count(const PageNavOps *o, int page, int pages, int lit)
{
    char label[LABEL_MAX];
    snprintf(label, sizeof label, "%d/%d", page + 1, pages);
    ui_pager(o, label, page > 0, page < pages - 1, lit);
}
