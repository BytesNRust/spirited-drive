// The top view panel (viewport.h): world (x, z) to the panel's pixels is view.c's, the drawing is ImDrawList's, the verbs are interact.h's.
#include "viewport.h"

#include "imgui.h"

#include <math.h>

extern "C" {
#include "formats/track_gen.h"
#include "interact.h"
}

namespace {

const double GRID_STEP = 50;   // units between grid lines
const float POINT_HALF = 4;    // a control point's square, half its side in pixels
const double GRID_LINES_MAX = 400;   // a view with more lines than this (zoomed far out) has no grid
const ImU32 GRID = IM_COL32(36, 40, 46, 255), BACKGROUND = IM_COL32(18, 20, 24, 255), ROAD = IM_COL32(200, 204, 212, 255),
            BAD = IM_COL32(255, 70, 70, 255), POINT = IM_COL32(90, 160, 255, 255), SELECTED = IM_COL32(255, 220, 0, 255);

struct Canvas {
    ImVec2 origin;   // the panel's top-left, in window pixels
    ImDrawList *list;
};

ImVec2 to_screen(const Canvas &c, const View &v, double x, double z)
{
    double sx, sy;
    view_to_screen(&v, x, z, &sx, &sy);
    return ImVec2(c.origin.x + (float)sx, c.origin.y + (float)sy);
}

void draw_grid(const Canvas &c, const View &v)
{
    double min_x, max_z, max_x, min_z;
    view_to_world(&v, 0, 0, &min_x, &max_z);
    view_to_world(&v, v.width, v.height, &max_x, &min_z);
    const double first_x = floor(min_x / GRID_STEP), last_x = floor(max_x / GRID_STEP);
    const double first_z = floor(min_z / GRID_STEP), last_z = floor(max_z / GRID_STEP);
    // a count, not a running sum: far from the origin a double does not grow by a step; and a view of absurd size draws no grid
    if (!(last_x - first_x < GRID_LINES_MAX) || !(last_z - first_z < GRID_LINES_MAX)) return;
    for (long long i = (long long)first_x; i <= (long long)last_x; ++i) {
        const double x = (double)i * GRID_STEP;
        ImVec2 a = to_screen(c, v, x, max_z), b = to_screen(c, v, x, min_z);
        a.x = b.x = floorf(a.x) + 0.5f;   // on a pixel's centre: a line between two pixels is drawn with a step in it
        c.list->AddLine(a, b, GRID);
    }
    for (long long i = (long long)first_z; i <= (long long)last_z; ++i) {
        const double z = (double)i * GRID_STEP;
        ImVec2 a = to_screen(c, v, min_x, z), b = to_screen(c, v, max_x, z);
        a.y = b.y = floorf(a.y) + 0.5f;
        c.list->AddLine(a, b, GRID);
    }
}

// the road: its nodes joined, a node breaking a limit drawn red; with no road (the trace cannot make one), the control polygon in red
void draw_road(const Canvas &c, const App &a)
{
    const View &v = a.view;
    if (!a.road_made) {
        for (int i = 1; i < a.trace.count; ++i)
            c.list->AddLine(to_screen(c, v, a.trace.points[i - 1].x, a.trace.points[i - 1].z),
                            to_screen(c, v, a.trace.points[i].x, a.trace.points[i].z), BAD, 2);
        return;
    }
    for (int i = 1; i < a.node_count; ++i) {
        const TrackGenNode &p = a.nodes[i - 1], &q = a.nodes[i];
        c.list->AddLine(to_screen(c, v, p.position[0] / 65536.0, p.position[2] / 65536.0),
                        to_screen(c, v, q.position[0] / 65536.0, q.position[2] / 65536.0), a.flags[i] ? BAD : ROAD, 3);
    }
}

void draw_points(const Canvas &c, const App &a)
{
    for (int i = 0; i < a.trace.count; ++i) {
        const ImVec2 p = to_screen(c, a.view, a.trace.points[i].x, a.trace.points[i].z);
        c.list->AddRectFilled(ImVec2(p.x - POINT_HALF, p.y - POINT_HALF), ImVec2(p.x + POINT_HALF, p.y + POINT_HALF),
                              i == a.selected ? SELECTED : a.road_made ? POINT : BAD);
    }
}

// what the mouse does on the canvas: a click takes a point and the button held drags it, the right or middle button pans, the wheel zooms
void handle_mouse(App *a, const ImVec2 &origin)
{
    static int dragged = -1;   // the point the left button took, -1 when none
    const ImGuiIO &io = ImGui::GetIO();
    const double mx = io.MousePos.x - origin.x, my = io.MousePos.y - origin.y;
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        a->selected = view_pick(&a->view, a->trace.points, a->trace.count, mx, my, INTERACT_PICK_REACH);
        dragged = a->selected;
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) || dragged >= a->trace.count || dragged != a->selected) dragged = -1;   // released, or the point is another now
    if (dragged >= 0 && io.MouseDelta.x * io.MouseDelta.x + io.MouseDelta.y * io.MouseDelta.y > 0) {
        double x, z;
        view_to_world(&a->view, mx, my, &x, &z);
        a->trace.points[a->selected].x = x;
        a->trace.points[a->selected].z = z;
        a->dirty = 1;
    }
    if (ImGui::IsItemActive() && (ImGui::IsMouseDown(ImGuiMouseButton_Right) || ImGui::IsMouseDown(ImGuiMouseButton_Middle))) {
        a->view.cx -= io.MouseDelta.x / a->view.scale;
        a->view.cz += io.MouseDelta.y / a->view.scale;
    }
    if (ImGui::IsItemHovered() && io.MouseWheel != 0) interact_zoom(a, mx, my, io.MouseWheel > 0 ? VIEW_ZOOM_STEP : 1 / VIEW_ZOOM_STEP);
}

}   // namespace

void viewport_panel(App *a)
{
    static int frames = 0;   // the dock layout gives the panel its size from the third frame on
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    const bool open = ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    if (open) {
        const ImVec2 size = ImGui::GetContentRegionAvail();
        if (size.x >= 1 && size.y >= 1) {
            const ImVec2 origin = ImGui::GetCursorScreenPos();
            a->view.width = (int)size.x;
            a->view.height = (int)size.y;
            if (++frames == 3) interact_fit(a);   // fit the trace to the panel once, when its size has settled
            ImGui::InvisibleButton("canvas", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
            handle_mouse(a, origin);
            app_recompute(a);   // what the mouse and the keys just changed, so the road is drawn with its points
            const Canvas c = { origin, ImGui::GetWindowDrawList() };
            c.list->PushClipRect(origin, ImVec2(origin.x + size.x, origin.y + size.y), true);
            c.list->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), BACKGROUND);
            draw_grid(c, a->view);
            draw_road(c, *a);
            draw_points(c, *a);
            c.list->PopClipRect();
        }
    }
    ImGui::End();
}
