/* port: see page_arrows.h. */
#include "game/frontend/page_arrows.h"
#include "game/frontend/ui_kit.h"

int page_nav_lit(const PageNavOps *o, int page, int pages)
{
    (void)page;
    if (pages < 2 || !o->in_rect(o->ctx, PAGE_NAV_X0, PAGE_NAV_X1, PAGE_NAV_Y0, PAGE_NAV_Y1)) return 0;
    return o->in_rect(o->ctx, PAGE_NAV_X0, PAGE_NAV_CENTRE, PAGE_NAV_Y0, PAGE_NAV_Y1) ? 1 : 2;   /* both arrows always: the box's halves */
}

void page_nav_draw(const PageNavOps *o, int page, int pages, int lit)
{
    ui_page_count(o, page, pages, lit);
}
