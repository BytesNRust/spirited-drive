/* port: see page_arrows.h. */
#include "game/frontend/page_arrows.h"
#include "game/frontend/ui_kit.h"

int page_nav_lit(const PageNavOps *o, int page, int pages)
{
    const int left = page > 0, right = page < pages - 1;
    if (!o->in_rect(o->ctx, PAGE_NAV_X0, PAGE_NAV_X1, PAGE_NAV_Y0, PAGE_NAV_Y1)) return 0;
    if (left && right) return o->in_rect(o->ctx, PAGE_NAV_X0, PAGE_NAV_CENTRE, PAGE_NAV_Y0, PAGE_NAV_Y1) ? 1 : 2;
    return left ? 1 : right ? 2 : 0;
}

void page_nav_draw(const PageNavOps *o, int page, int pages, int lit)
{
    ui_page_count(o, page, pages, lit);
}
