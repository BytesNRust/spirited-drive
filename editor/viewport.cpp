// The top view panel (viewport.h): world (x, z) to the panel's pixels is view.c's, the drawing is ImDrawList's, the verbs are interact.h's.
#include "viewport.h"

#include "imgui.h"

#include <math.h>

extern "C" {
#include "formats/track_gen.h"
#include "formats/track_trace.h"
#include "interact.h"
}

namespace {

const double GRID_STEP = 50;   // units between grid lines
const float POINT_HALF = 4;    // a control point's square, half its side in pixels
const double GRID_LINES_MAX = 400;   // a view with more lines than this (zoomed far out) has no grid
const double TIP_REACH = 10;   // pixels from a flagged node that bring its tip
const ImU32 GRID = IM_COL32(36, 40, 46, 255), BACKGROUND = IM_COL32(18, 20, 24, 255), ROAD = IM_COL32(200, 204, 212, 255),
            BAD = IM_COL32(255, 70, 70, 255), WARN = IM_COL32(255, 176, 32, 255), POINT = IM_COL32(90, 160, 255, 255), SELECTED = IM_COL32(255, 220, 0, 255),
            TARMAC = IM_COL32(112, 116, 124, 255);   // a tarmac shoulder (a road key): asphalt grey beside the road
const double EDGE_UNIT = 0.125;   // metres from the centre to the road's edge per unit of the node's width byte (VERGE_UNIT, game/race/car_walls.c)
const float TARMAC_MIN_PX = 4;    // a tarmac line stays this far from the road's line however far out the view is

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

// the colour of the road at a node: red when it breaks a red limit, amber over an amber one, else the road's
ImU32 segment_colour(TraceFlags flags)
{
    if (flags & TRACE_FLAG_RED_BITS) return BAD;
    return (flags & TRACE_FLAG_WARN_BITS) ? WARN : ROAD;
}

// is a node's side a tarmac shoulder (the road key's surface, as the generator writes it)?
bool tarmac_side(const TrackGenNode &n, int side)
{
    return (n.side[side].has & TRACK_GEN_SIDE_TARMAC) && n.side[side].tarmac;
}

// a tarmac shoulder: an asphalt-grey line beside the road at its edge, on each side whose segment starts tarmac (the game changes a side's surface at a
// segment's first node, grip and picture), at least TARMAC_MIN_PX from the road's line
void draw_tarmac(const Canvas &c, const App &a)
{
    for (int i = 1; i < a.node_count; ++i) {
        const TrackGenNode &p = a.nodes[i - 1], &q = a.nodes[i];
        const double px = p.position[0] / 65536.0, pz = p.position[2] / 65536.0, qx = q.position[0] / 65536.0, qz = q.position[2] / 65536.0;
        const double dx = qx - px, dz = qz - pz, len = sqrt(dx * dx + dz * dz);
        if (len <= 0) continue;
        for (int side = TRACK_GEN_LEFT; side <= TRACK_GEN_RIGHT; ++side) {
            if (!tarmac_side(p, side)) continue;
            const double toward = side == TRACK_GEN_RIGHT ? 1 : -1, off = p.width * EDGE_UNIT;   // right of a heading along +z is +x
            const double nx = toward * dz / len, nz = -toward * dx / len;
            const ImVec2 a0 = to_screen(c, a.view, px, pz), b0 = to_screen(c, a.view, qx, qz);
            ImVec2 o = to_screen(c, a.view, px + nx * off, pz + nz * off);
            float ox = o.x - a0.x, oy = o.y - a0.y;
            const float olen = sqrtf(ox * ox + oy * oy);
            if (olen < TARMAC_MIN_PX) {   // zoomed far out: the edge is under the road's line, so the line goes TARMAC_MIN_PX out the same way
                const ImVec2 s = to_screen(c, a.view, px + nx, pz + nz);
                const float sx = s.x - a0.x, sy = s.y - a0.y, slen = sqrtf(sx * sx + sy * sy);
                if (slen <= 0) continue;
                ox = sx / slen * TARMAC_MIN_PX, oy = sy / slen * TARMAC_MIN_PX;
            }
            c.list->AddLine(ImVec2(a0.x + ox, a0.y + oy), ImVec2(b0.x + ox, b0.y + oy), TARMAC, 3);
        }
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
    draw_tarmac(c, a);
    for (int i = 1; i < a.node_count; ++i) {
        const TrackGenNode &p = a.nodes[i - 1], &q = a.nodes[i];
        c.list->AddLine(to_screen(c, v, p.position[0] / 65536.0, p.position[2] / 65536.0),
                        to_screen(c, v, q.position[0] / 65536.0, q.position[2] / 65536.0), segment_colour(a.flags[i]), 3);
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
    static double grab_x, grab_z;   // where the point was from the mouse when it was taken: the point keeps that distance, it does not jump
    const ImGuiIO &io = ImGui::GetIO();
    const double mx = io.MousePos.x - origin.x, my = io.MousePos.y - origin.y;
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        a->selected = view_pick(&a->view, a->trace.points, a->trace.count, mx, my, INTERACT_PICK_REACH);
        dragged = a->selected;
        if (dragged >= 0) {
            double x, z;
            view_to_world(&a->view, mx, my, &x, &z);
            grab_x = a->trace.points[dragged].x - x;
            grab_z = a->trace.points[dragged].z - z;
        }
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) || dragged >= a->trace.count || dragged != a->selected) dragged = -1;   // released, or the point is another now
    if (dragged >= 0 && io.MouseDelta.x * io.MouseDelta.x + io.MouseDelta.y * io.MouseDelta.y > 0) {
        double x, z;
        view_to_world(&a->view, mx, my, &x, &z);
        a->trace.points[a->selected].x = x + grab_x;
        a->trace.points[a->selected].z = z + grab_z;
        a->dirty = 1;
    }
    if (ImGui::IsItemActive() && (ImGui::IsMouseDown(ImGuiMouseButton_Right) || ImGui::IsMouseDown(ImGuiMouseButton_Middle))) {
        a->view.cx -= io.MouseDelta.x / a->view.scale;
        a->view.cz += io.MouseDelta.y / a->view.scale;
    }
    if (ImGui::IsItemHovered() && io.MouseWheel != 0) interact_zoom(a, mx, my, io.MouseWheel > 0 ? VIEW_ZOOM_STEP : 1 / VIEW_ZOOM_STEP);
}

// the nearest node of the road, within TIP_REACH pixels of the mouse; -1 when none
int node_near(const Canvas &c, const App &a, double mx, double my)
{
    int best = -1;
    double best_d = TIP_REACH * TIP_REACH;
    for (int i = 1; a.road_made && i < a.node_count; ++i) {
        const ImVec2 p = to_screen(c, a.view, a.nodes[i].position[0] / 65536.0, a.nodes[i].position[2] / 65536.0);
        const double dx = p.x - c.origin.x - mx, dy = p.y - c.origin.y - my, d = dx * dx + dy * dy;
        if (d < best_d) best_d = d, best = i;
    }
    return best;
}

// the tip over a node: its place on the road (metres from the start, its turn) and, over a red or amber stretch, what the colour means and that it
// is the spacing of the points that makes it
void draw_tooltip(const Canvas &c, const App &a)
{
    const ImGuiIO &io = ImGui::GetIO();
    if (io.MouseDown[0]) return;   // a drag is on: no tip over the point being moved
    const int node = node_near(c, a, io.MousePos.x - c.origin.x, io.MousePos.y - c.origin.y);
    if (node < 0) return;
    char turn[16];
    track_trace_turn_text(track_trace_signed_turn(a.nodes, node), turn, sizeof turn);
    ImGui::BeginTooltip();
    ImGui::Text("node %d: %.0f m from the start, turn %s", node, a.node_dist != NULL ? a.node_dist[node] : 0.0, turn);
    const bool left = tarmac_side(a.nodes[node], TRACK_GEN_LEFT), right = tarmac_side(a.nodes[node], TRACK_GEN_RIGHT);
    if (left || right) ImGui::Text("shoulder: asphalt %s", left && right ? "both sides" : left ? "left" : "right");
    if (a.flags[node] & TRACE_FLAG_RED_BITS) {
        if (a.flags[node] & (TRACE_FLAG_TURN | TRACE_FLAG_RADIUS)) {
            ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "too tight: the road is not built");
            ImGui::TextUnformatted("points too close for this turn: space them out or smooth them");
        }
        if (a.flags[node] & TRACE_FLAG_WALL_FAR) {
            ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "barrier too far out (over %g m from the centre): the road is not built", TRACE_WALL_FAR_M);
            ImGui::TextUnformatted("bring the barrier nearer the road, or narrow the road here");
        }
        if (a.flags[node] & TRACE_FLAG_BANK_LIMIT)
            ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "banks %.1f\xc2\xb0, over %g\xc2\xb0: the road is not built", fabs(a.nodes[node].bank), TRACE_BANK_MAX_DEG);
    } else {
        if (a.flags[node] & (TRACE_FLAG_TURN_WARN | TRACE_FLAG_RADIUS_WARN))
            ImGui::TextColored(ImVec4(1.0f, 0.69f, 0.13f, 1.0f), "very tight curve: check the terrain in the game, the AI brakes");
        if (a.flags[node] & TRACE_FLAG_WIDTH_WARN)
            ImGui::TextColored(ImVec4(1.0f, 0.69f, 0.13f, 1.0f), "width changes too fast (over %g m a node): lengthen the ramp", TRACE_WIDTH_STEP_WARN_M);
        if (a.flags[node] & TRACE_FLAG_SIDE_WARN)
            ImGui::TextColored(ImVec4(1.0f, 0.69f, 0.13f, 1.0f), "a shoulder or a barrier moves too fast (over %g m a node): spread it over more points",
                               TRACE_SIDE_STEP_WARN_M);
        if (a.flags[node] & TRACE_FLAG_BANK_WARN)
            ImGui::TextColored(ImVec4(1.0f, 0.69f, 0.13f, 1.0f), "banks %.1f\xc2\xb0, over %g\xc2\xb0: check the edge and the car in the game", fabs(a.nodes[node].bank),
                               TRACE_BANK_WARN_DEG);
    }
    ImGui::EndTooltip();
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
            a->canvas_x = origin.x, a->canvas_y = origin.y;
            a->view.width = (int)size.x;
            a->view.height = (int)size.y;
            if (++frames == 3 || a->fit_pending) {   // fit the trace to the panel once, when its size has settled, and when another project was opened
                interact_fit(a);
                a->fit_pending = 0;
            }
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
            if (ImGui::IsItemHovered()) draw_tooltip(c, *a);
        }
    }
    ImGui::End();
}
