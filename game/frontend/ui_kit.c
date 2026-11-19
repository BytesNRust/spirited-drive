/* port: see ui_kit.h. */
#include "game/frontend/ui_kit.h"

#include "game/frontend/added_card.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

enum { CREAM = 0, LABEL_MAX = 32, FIT_MAX = 256 };

void ui_arrow_mask(int right, uint8_t mask[UI_ARROW_MASK_W * UI_ARROW_MASK_H])
{
    enum { W = UI_ARROW_MASK_W, H = UI_ARROW_MASK_H };
    memset(mask, 0, (size_t)W * H);
    for (int r = 0; r < UI_ARROW_INK_H; r++) {
        const int up = r < UI_ARROW_INK_H - 1 - r ? r : UI_ARROW_INK_H - 1 - r;
        for (int c = 0; c <= up; c++) {
            const int x = right ? c : UI_ARROW_INK_W - 1 - c;
            mask[(UI_ARROW_PAD + r) * W + UI_ARROW_PAD + x] = 15;
        }
    }
    /* the outline: the pixels beside the fill (not its corners: a slope is one pixel thick) */
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            if (mask[y * W + x] == 0 &&
                ((x > 0 && mask[y * W + x - 1] == 15) || (x < W - 1 && mask[y * W + x + 1] == 15) ||
                 (y > 0 && mask[(y - 1) * W + x] == 15) || (y < H - 1 && mask[(y + 1) * W + x] == 15)))
                mask[y * W + x] = UI_ARROW_OUTLINE;
    /* and the base's column runs two rows past the fill at each end */
    const int base = right ? UI_ARROW_PAD - 1 : UI_ARROW_PAD + UI_ARROW_INK_W;
    for (int y = UI_ARROW_PAD - 2; y < UI_ARROW_PAD + UI_ARROW_INK_H + 2; y++)
        if (mask[y * W + base] == 0) mask[y * W + base] = UI_ARROW_OUTLINE;
}

const UiPagerPlace UI_PAGER_FOOT = { PAGE_NAV_CENTRE, UI_PAGER_TEXT_Y, PAGE_NAV_X0, PAGE_NAV_X1 };

UiPagerLayout ui_pager_layout_at(const UiPagerPlace *at, int text_w, int right_w)
{
    UiPagerLayout l;
    l.text_x = at->centre - text_w / 2;
    /* the masks' own padding is blank too: a triangle's ink starts UI_ARROW_PAD columns into its mask */
    l.left_x = l.text_x + UI_PAGER_BEARING_LEFT - UI_PAGER_GAP_LEFT - UI_ARROW_INK_W - UI_ARROW_PAD;
    l.right_x = l.text_x + text_w - UI_PAGER_BEARING_RIGHT + UI_PAGER_GAP_RIGHT - UI_ARROW_PAD;
    l.fits = l.left_x >= at->x0 + 2 && l.right_x <= at->x1 - 2 - right_w;
    return l;
}

UiPagerLayout ui_pager_layout(int text_w, int right_w) { return ui_pager_layout_at(&UI_PAGER_FOOT, text_w, right_w); }

UiSelectorLayout ui_selector_layout(int centre, int line_w)
{
    UiSelectorLayout l;
    l.left_x = centre - line_w / 2 - UI_PAGER_GAP_LEFT - UI_ARROW_INK_W - UI_ARROW_PAD;
    l.right_x = centre - line_w / 2 + line_w + UI_PAGER_GAP_RIGHT - UI_ARROW_PAD;
    return l;
}

typedef struct {
    UiMeasure measure;
    void *ctx;
    UiSize size;
} FitAt;

static int fit_measure(void *ctx, const char *text)
{
    const FitAt *const at = ctx;
    return at->measure(at->ctx, text, at->size.num, at->size.den);
}

int ui_fit(const char *text, int max_w, UiFitMode mode, const UiSize *sizes, int count, UiMeasure measure, void *ctx, char *out, int size,
           UiSize *used)
{
    if (size <= 0) return 0;
    out[0] = '\0';
    if (count <= 0 || text == NULL) return 0;
    char whole[FIT_MAX];
    FitAt at = { measure, ctx, sizes[0] };
    if (!added_card_name_fit(text, INT_MAX, fit_measure, &at, whole, sizeof whole)) return 0;   /* text_words' bytes, trimmed; nothing left: empty */
    for (int i = 0; i < (mode == UI_FIT_SHRINK ? count : 1); i++) {
        at.size = sizes[i];
        if (measure(ctx, whole, at.size.num, at.size.den) <= max_w) {
            snprintf(out, (size_t)size, "%s", whole);
            if (used != NULL) *used = at.size;
            return 1;
        }
    }
    if (used != NULL) *used = at.size;   /* the last size tried: cut there, by the cards' own rule */
    return added_card_name_fit(whole, max_w, fit_measure, &at, out, size);
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

static void word_draw(const PageNavOps *o, const UiPagerPlace *at, const char *text, int x)
{
    if (!scaled_ok(o)) {
        o->text(o->ctx, CREAM, text, x, at->text_y + (PAGE_NAV_TEXT_Y - UI_PAGER_TEXT_Y));
        return;
    }
    (o->clean ? o->scaled.clean : o->scaled.plain)(o->ctx, CREAM, text, x, at->text_y, UI_PAGER_TEXT_NUM, UI_PAGER_TEXT_DEN);
}

void ui_pager_at(const PageNavOps *o, const UiPagerPlace *at, const char *label, int has_left, int has_right, int lit)
{
    int32_t lw, lh, rw, rh;
    const int arrows = o->arrow != NULL && o->arrow_size != NULL && o->arrow_size(o->ctx, 0, &lw, &lh) &&
                       o->arrow_size(o->ctx, 1, &rw, &rh);
    char text[LABEL_MAX];
    const UiPagerLayout l = arrows ? ui_pager_layout_at(at, word_width(o, label), rw) : (UiPagerLayout){ 0 };
    if (!arrows || !l.fits) {
        snprintf(text, sizeof text, "%s%s%s", has_left ? "< " : "", label, has_right ? " >" : "");
        word_draw(o, at, text, at->centre - word_width(o, text) / 2);
        return;
    }
    const int mid = at->text_y + UI_PAGER_MIDDLE_BELOW_TOP;
    word_draw(o, at, label, l.text_x);
    if (has_left) o->arrow(o->ctx, 0, lit == 1, l.left_x, mid - lh / 2);
    if (has_right) o->arrow(o->ctx, 1, lit == 2, l.right_x, mid - rh / 2);
}

void ui_pager(const PageNavOps *o, const char *label, int has_left, int has_right, int lit)
{
    ui_pager_at(o, &UI_PAGER_FOOT, label, has_left, has_right, lit);
}

void ui_page_count_at(const PageNavOps *o, const UiPagerPlace *at, int page, int pages, int lit)
{
    char label[LABEL_MAX];
    snprintf(label, sizeof label, "%d/%d", page + 1, pages);
    ui_pager_at(o, at, label, pages > 1, pages > 1, lit);   /* the CD's pagers wrap: both arrows always */
}

void ui_page_count(const PageNavOps *o, int page, int pages, int lit)
{
    char label[LABEL_MAX];
    snprintf(label, sizeof label, "%d/%d", page + 1, pages);
    ui_pager(o, label, pages > 1, pages > 1, lit);
}

UiFootLayout ui_foot_layout(UiFootWord left, UiFootWord right, const UiFootWord *mid, int mid_n, int gap, int margin, int width)
{
    UiFootLayout l = { margin - left.lead, width - margin - right.w + right.trail, { 0 } };
    const int from = margin + (left.w - left.lead - left.trail), to = width - margin - (right.w - right.lead - right.trail);   /* the room's ink edges */
    int group = mid_n > 1 ? gap * (mid_n - 1) : 0;
    for (int i = 0; i < mid_n; i++) group += mid[i].w - mid[i].lead - mid[i].trail;
    int ink = from + (to - from - group) / 2;
    if (ink < from) ink = from;
    for (int i = 0; i < mid_n && i < UI_FOOT_MID_MAX; i++) {
        l.mid_x[i] = ink - mid[i].lead;
        ink += mid[i].w - mid[i].lead - mid[i].trail + gap;
    }
    return l;
}
