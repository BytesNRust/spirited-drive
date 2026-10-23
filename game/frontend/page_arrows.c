/* port: see page_arrows.h. */
#include "game/frontend/page_arrows.h"

#include "formats/fsh.h"
#include "game/frontend/item_art.h"
#include "game/frontend/title_art.h"
#include "game/sys/port_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* the track select art whose footer carries the triangles; every track's file has the same four shapes */
#define ARROW_ART "tracksel/citybst.qfs"
static const char *const DIM[PAGE_ARROWS] = { "lard", "rard" };
static const char *const LIT[PAGE_ARROWS] = { "larl", "rarl" };

int page_arrows_mask(const uint8_t *dim, const uint8_t *lit, int w, int h, const uint8_t palette[768], uint8_t *mask)
{
    if (w <= 0 || h <= 0 || w > PAGE_ARROW_MAX || h > PAGE_ARROW_MAX) return 0;
    const int mw = w + 2 * PAGE_ARROW_PAD, mh = h + 2 * PAGE_ARROW_PAD;
    uint8_t cut[PAGE_ARROW_MAX * PAGE_ARROW_MAX];
    item_art_mask_from_pair(dim, lit, w, palette, 0, 0, w - 1, h - 1, cut);
    memset(mask, 0, (size_t)mw * mh);
    for (int y = 0; y < h; y++) memcpy(mask + (y + PAGE_ARROW_PAD) * mw + PAGE_ARROW_PAD, cut + y * w, (size_t)w);
    return 1;
}

/* a bitmap shape of the file (its pixels in `*out`) */
static int bitmap(Fsh *f, const char *tag, FshEntry *out)
{
    uint32_t i;
    return fsh_find(f, tag, &i) == FSH_OK && fsh_entry(f, i, out) == FSH_OK && out->type == FSH_TYPE_BITMAP &&
           out->payload_size >= (size_t)out->width * out->height;
}

int page_arrows_load(ConfigBgndRead read, PageArrowMasks *m)
{
    size_t size = 0;
    uint8_t *const data = read(ARROW_ART, &size);
    uint8_t palette[768];
    TitleArtScreen screen;   /* the shapes are drawn in the palette of the file's "bgnd" */
    Fsh f;
    int ok = data != NULL && title_art_screen(data, size, palette, &screen) && fsh_open(&f, data, size) == FSH_OK;
    for (int a = 0; ok && a < PAGE_ARROWS; a++) {
        FshEntry dim, lit;
        ok = bitmap(&f, DIM[a], &dim) && bitmap(&f, LIT[a], &lit) && dim.width == lit.width &&
             dim.height == lit.height &&
             page_arrows_mask(dim.payload, lit.payload, dim.width, dim.height, palette, m->mask[a]);
        if (ok) m->w[a] = dim.width + 2 * PAGE_ARROW_PAD, m->h[a] = dim.height + 2 * PAGE_ARROW_PAD;
    }
    free(data);
    return ok;
}

const PageArrowMasks *page_arrows_shared(ConfigBgndRead read)
{
    static PageArrowMasks masks;
    static int tried, ok;
    if (!tried) {
        tried = 1;
        ok = page_arrows_load(read, &masks);
        if (!ok) port_log("page arrows: not on the CD, drawn as text\n");
    }
    return ok ? &masks : NULL;
}

enum { CREAM = 0 };

int page_nav_lit(const PageNavOps *o, int page, int pages)
{
    const int left = page > 0, right = page < pages - 1;
    if (!o->in_rect(o->ctx, PAGE_NAV_X0, PAGE_NAV_X1, PAGE_NAV_Y0, PAGE_NAV_Y1)) return 0;
    if (left && right) return o->in_rect(o->ctx, PAGE_NAV_X0, PAGE_NAV_CENTRE, PAGE_NAV_Y0, PAGE_NAV_Y1) ? 1 : 2;
    return left ? 1 : right ? 2 : 0;
}

void page_nav_draw(const PageNavOps *o, int page, int pages, int lit)
{
    char text[16];
    const int left = page > 0, right = page < pages - 1;
    int32_t lw, lh, rw, rh;
    const int arrows = o->arrow != NULL && o->arrow_size != NULL && o->arrow_size(o->ctx, 0, &lw, &lh) &&
                       o->arrow_size(o->ctx, 1, &rw, &rh);
    if (!arrows) {
        snprintf(text, sizeof text, "%s%d/%d%s", left ? "< " : "", page + 1, pages, right ? " >" : "");
        o->text(o->ctx, CREAM, text, PAGE_NAV_CENTRE - o->text_width(o->ctx, text) / 2, PAGE_NAV_TEXT_Y);
        return;
    }
    snprintf(text, sizeof text, "%d/%d", page + 1, pages);
    o->text(o->ctx, CREAM, text, PAGE_NAV_CENTRE - o->text_width(o->ctx, text) / 2, PAGE_NAV_TEXT_Y);
    if (left) o->arrow(o->ctx, 0, lit == 1, page_nav_arrow_x(PAGE_ARROW_LEFT, lw), PAGE_NAV_ARROW_Y);
    if (right) o->arrow(o->ctx, 1, lit == 2, page_nav_arrow_x(PAGE_ARROW_RIGHT, rw), PAGE_NAV_ARROW_Y);
}
