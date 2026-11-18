// The top view panel: the road's centre line as its nodes, the control points, drag / pan / zoom, drawn with ImDrawList (viewport.cpp).
#ifndef TNFS_TRACK_EDITOR_VIEWPORT_H
#define TNFS_TRACK_EDITOR_VIEWPORT_H

extern "C" {
#include "app.h"
}

void viewport_panel(App *a);

#endif
