/* The top view's centring (editor/view.h, dep95 step 7): the Distance strip's double-click takes the Viewport to a place on the road. The view is centred on it,
 * zoomed in to at least a working scale and never out; the place stays under the window's centre. */
#include "view.h"
#include "test.h"

int main(void)
{
    View v = { 0, 0, 0.1, 800, 400 };
    view_centre_on(&v, 120, -340, VIEW_FOCUS_SCALE);
    CHECK(v.cx == 120 && v.cz == -340 && v.scale == VIEW_FOCUS_SCALE);   /* zoomed out: it comes in to the working scale */
    double sx, sy;
    view_to_screen(&v, 120, -340, &sx, &sy);
    CHECK(sx == 400 && sy == 200);   /* the place is at the centre of the window */
    v.scale = 3.0;
    view_centre_on(&v, -5, 7, VIEW_FOCUS_SCALE);
    CHECK(v.cx == -5 && v.cz == 7 && v.scale == 3.0);   /* already closer than that: the zoom stays */
    view_centre_on(&v, 1e12, 0, VIEW_FOCUS_SCALE);
    CHECK(v.cx == 1e12);
    View none = { 0, 0, 0.5, 0, 0 };
    view_centre_on(&none, 10, 10, 99.0);
    CHECK(none.cx == 10 && none.scale == VIEW_SCALE_MAX);   /* never past the view's own largest scale */
    return TEST_REPORT();
}
