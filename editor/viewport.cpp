// The top view panel (viewport.h): world (x, z) to the panel's pixels is view.c's, the drawing is ImDrawList's, the verbs are interact.h's.
#include "viewport.h"

#include "imgui.h"
#include "stretch_colours.h"

#include <stdio.h>
#include <stdlib.h>

#include <math.h>

extern "C" {
#include "formats/track_gen.h"
#include "formats/track_trace.h"
#include "app_select.h"
#include "app_colour.h"
#include "interact.h"
#include "point_ops.h"
#include "app_points.h"
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
const float BOX_MIN_PX = 4;       // a left drag on no point shorter than this is a click there, not a box
const double END_SNAP_PX = 8;     // the last point dragged this near point 0 lands on it (snap_end)
const ImU32 PREVIEW_GREY = IM_COL32(200, 200, 200, 230), PREVIEW_TAIL = IM_COL32(150, 150, 150, 140);   // Add curve's preview (draw_curve_preview)
const ImU32 PREVIEW_RED = IM_COL32(255, 90, 90, 255), PREVIEW_AMBER = IM_COL32(255, 176, 33, 255);
const ImU32 BOX_EDGE = IM_COL32(255, 220, 0, 200), BOX_FILL = IM_COL32(255, 220, 0, 32);

struct Box {   // the box a left drag on no point draws (dep87): its first corner in the panel's pixels, while the button is held
    bool on, add;   // add: Shift was held, the points join the set
    double x, y;
};
Box g_box;

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
void draw_road(const Canvas &c, const App &a, const uint8_t *classes)
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
        // Colour by: the node's class colour, but a red limit shows through in every mode
        const ImU32 colour = classes != nullptr && !(a.flags[i] & TRACE_FLAG_RED_BITS) ? stretch_colour(a.colour_by, classes[i]) : segment_colour(a.flags[i]);
        c.list->AddLine(to_screen(c, v, p.position[0] / 65536.0, p.position[2] / 65536.0),
                        to_screen(c, v, q.position[0] / 65536.0, q.position[2] / 65536.0), colour, 3);
    }
}

void draw_points(const Canvas &c, const App &a)
{
    for (int i = 0; i < a.trace.count; ++i) {
        const ImVec2 p = to_screen(c, a.view, a.trace.points[i].x, a.trace.points[i].z);
        c.list->AddRectFilled(ImVec2(p.x - POINT_HALF, p.y - POINT_HALF), ImVec2(p.x + POINT_HALF, p.y + POINT_HALF),
                              sel_has(&a.sel, i) ? SELECTED : a.road_made ? POINT : BAD);
    }
}

// the box being drawn, from its first corner to the mouse
void draw_box(const Canvas &c)
{
    if (!g_box.on) return;
    const ImVec2 from(c.origin.x + (float)g_box.x, c.origin.y + (float)g_box.y), to = ImGui::GetIO().MousePos;
    const ImVec2 lo(fminf(from.x, to.x), fminf(from.y, to.y)), hi(fmaxf(from.x, to.x), fmaxf(from.y, to.y));
    c.list->AddRectFilled(lo, hi, BOX_FILL);
    c.list->AddRect(lo, hi, BOX_EDGE);
}

// a click's kind from the keys held: Shift a range, Cmd one more or one less (ImGui's Ctrl is Cmd on the Mac), else that one alone
SelClick click_kind(const ImGuiIO &io)
{
    return io.KeyShift ? SEL_RANGE : io.KeyCtrl ? SEL_TOGGLE : SEL_ONE;
}

// what Add curve or Add straight would make while their fields are hovered or edited: the new stretch grey (in the limit's colour under a
// radius limit) and the road after it where it would go, dimmer
void draw_curve_preview(const Canvas &c, App *a)
{
    if (a->curve.preview == 0) return;
    int n = 0, from = 0, to = 0;
    TracePoint *q = app_curve_preview(a, a->curve.preview == 2, &n, &from, &to);
    if (q == NULL) return;
    const int level = a->curve.preview == 1 ? app_curve_level(a) : 0;
    const ImU32 block = level == 2 ? PREVIEW_RED : level == 1 ? PREVIEW_AMBER : PREVIEW_GREY;
    for (int i = from; i + 1 < n; ++i)
        c.list->AddLine(to_screen(c, a->view, q[i].x, q[i].z), to_screen(c, a->view, q[i + 1].x, q[i + 1].z), i < to ? block : PREVIEW_TAIL, i < to ? 3.0f : 2.0f);
    for (int i = from + 1; i <= to; ++i) {
        const ImVec2 p = to_screen(c, a->view, q[i].x, q[i].z);
        c.list->AddRect(ImVec2(p.x - POINT_HALF, p.y - POINT_HALF), ImVec2(p.x + POINT_HALF, p.y + POINT_HALF), block);
    }
    free(q);
}

// the last point dragged within END_SNAP_PX of point 0 (not itself moving) lands on it: the end on the start, as a circuit closes (the game
// takes no circuit from a mod yet: the road is then refused for two points on one place, the readout says what a circuit needs)
void snap_end(App *a, int dragged)
{
    const int n = a->trace.count;
    if (dragged != n - 1 || n < 3 || sel_has(&a->sel, 0)) return;
    const TracePoint p0 = a->trace.points[0], end = a->trace.points[n - 1];
    if (hypot(p0.x - end.x, p0.z - end.z) * a->view.scale < END_SNAP_PX) app_sel_move(a, p0.x - end.x, p0.z - end.z);
}

// while a point is dragged: its length to the point before, its turn and the radius there; for the last point how far the end is from the start
void drag_readout(const App &a, int i)
{
    const PointReadout r = pts_readout(a.trace.points, a.trace.count, i);
    char turn[16];
    track_trace_turn_text(r.turn_deg, turn, sizeof turn);
    ImGui::BeginTooltip();
    if (i > 0) ImGui::Text("point %d: %.1f m from point %d", i, r.to_prev_m, i - 1);
    if (i > 0 && i < a.trace.count - 1) {
        if (r.radius_m > 0) ImGui::Text("turn %s, radius %.0f m", turn, r.radius_m);
        else ImGui::Text("turn %s", turn);
    }
    if (i == a.trace.count - 1 && a.trace.count >= 3) {
        char gap[160];
        pts_end_gap_text(pts_end_gap(a.trace.points, a.trace.count), a.road_made ? a.node_count : 0, gap, sizeof gap);
        ImGui::TextUnformatted(gap);
    }
    ImGui::EndTooltip();
}

// the left button let go of a box: the points inside it selected, or (hardly moved: a click on no point) nothing selected
void end_box(App *a, double mx, double my)
{
    g_box.on = false;
    if (fabs(mx - g_box.x) < BOX_MIN_PX && fabs(my - g_box.y) < BOX_MIN_PX) {
        if (!g_box.add) app_sel_clear(a);
        return;
    }
    double x0, z0, x1, z1;
    view_to_world(&a->view, g_box.x, g_box.y, &x0, &z0);
    view_to_world(&a->view, mx, my, &x1, &z1);
    app_sel_box(a, x0, z0, x1, z1, g_box.add);
}

int node_near(const Canvas &c, const App &a, double mx, double my);

// what the mouse does on the canvas: a click takes a point (Shift a range, Cmd one more), the button held drags every selected point; a left
// drag on no point draws a box that selects; the right or middle button pans, the wheel zooms
void handle_mouse(App *a, const ImVec2 &origin)
{
    static int dragged = -1;   // the point the left button took, -1 when none
    static int collapse = -1;   // a plain click on a point of a larger set: the set becomes that point when the button is let go without a drag
    static double grab_x, grab_z;   // where the point was from the mouse when it was taken: the points keep that distance, they do not jump
    const ImGuiIO &io = ImGui::GetIO();
    const double mx = io.MousePos.x - origin.x, my = io.MousePos.y - origin.y;
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        const int pick = view_pick(&a->view, a->trace.points, a->trace.count, mx, my, INTERACT_PICK_REACH);
        const SelClick how = click_kind(io);
        collapse = -1;
        const int road = pick < 0 && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && a->colour_by != TRACE_COLOUR_LIMITS ? node_near(Canvas{ origin, nullptr }, *a, mx, my) : -1;
        if (road >= 0 && app_select_stretch(a, road, io.KeyShift || io.KeyCtrl)) g_box.on = false;   // a double-click on the road: its stretch (Colour by), added with Shift or Cmd, not a box
        else if (pick < 0) g_box = { true, io.KeyShift || io.KeyCtrl, mx, my };   // Shift or Cmd: the box adds to the set
        else if (how == SEL_ONE && sel_has(&a->sel, pick) && app_sel_count(a) > 1) a->selected = pick, collapse = pick;   // the set stays for a drag
        else app_sel_click(a, pick, how);
        dragged = pick >= 0 && sel_has(&a->sel, pick) ? pick : -1;
        if (dragged >= 0) {
            double x, z;
            view_to_world(&a->view, mx, my, &x, &z);
            grab_x = a->trace.points[dragged].x - x;
            grab_z = a->trace.points[dragged].z - z;
        }
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {   // let go
        if (g_box.on) end_box(a, mx, my);
        if (collapse >= 0 && collapse < a->trace.count) app_sel_click(a, collapse, SEL_ONE);
        collapse = dragged = -1;
    }
    if (dragged >= a->trace.count || (dragged >= 0 && !sel_has(&a->sel, dragged))) dragged = -1;   // the point is gone or no longer selected
    if (dragged >= 0 && io.MouseDelta.x * io.MouseDelta.x + io.MouseDelta.y * io.MouseDelta.y > 0) {
        double x, z;
        view_to_world(&a->view, mx, my, &x, &z);
        app_sel_move(a, x + grab_x - a->trace.points[dragged].x, z + grab_z - a->trace.points[dragged].z);   // the group, by the taken point's move
        snap_end(a, dragged);
        collapse = -1;
    }
    if (dragged >= 0 && ImGui::IsMouseDown(ImGuiMouseButton_Left) && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) drag_readout(*a, dragged);
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

const char *const COLOUR_BY_NAMES[TRACE_COLOUR_MODES] = { "Limits", "Curve", "Bank", "Width", "Lanes", "Shoulder", "Fence" };
const float COMBO_MARGIN = 8;   // from the canvas' top-left corner

// the Colour by combo over the canvas' top-left corner; true while the mouse is on it. Its tooltip is the legend: what each colour means in the mode.
bool colour_by_combo(App *a, const ImVec2 &origin)
{
    ImGui::SetCursorScreenPos(ImVec2(origin.x + COMBO_MARGIN, origin.y + COMBO_MARGIN));
    ImGui::SetNextItemWidth(ImGui::CalcTextSize("Colour by: Shoulder").x + 2 * ImGui::GetStyle().FramePadding.x + ImGui::GetFrameHeight());
    char shown[48];
    snprintf(shown, sizeof shown, "Colour by: %s", COLOUR_BY_NAMES[a->colour_by]);
    if (ImGui::BeginCombo("##colourby", shown)) {
        for (int m = 0; m < TRACE_COLOUR_MODES; ++m)
            if (ImGui::Selectable(COLOUR_BY_NAMES[m], m == a->colour_by)) app_set_colour_by(a, m);
        ImGui::EndCombo();
    }
    const bool hovered = ImGui::IsItemHovered() || ImGui::IsItemActive();
    if (ImGui::IsItemHovered() && !ImGui::IsPopupOpen("##colourby") && ImGui::BeginTooltip()) {
        if (a->colour_by == TRACE_COLOUR_LIMITS) {
            ImGui::TextUnformatted("Red and amber mark what Build refuses or warns about.");
            ImGui::TextUnformatted("Pick a mode to colour the road by it; double-click the road then selects that stretch.");
        } else {
            ImGui::Text("The road coloured by %s. Red still marks a limit Build refuses.", COLOUR_BY_NAMES[a->colour_by]);
            ImGui::TextUnformatted("Double-click the road to select its stretch.");
            for (int k = 0; k < track_trace_class_count((TraceColourBy)a->colour_by); ++k) {
                ImGui::ColorButton("##swatch", ImColor(stretch_colour(a->colour_by, k)), ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoDragDrop,
                                   ImVec2(ImGui::GetTextLineHeight(), ImGui::GetTextLineHeight()));
                ImGui::SameLine();
                ImGui::TextUnformatted(track_trace_class_name((TraceColourBy)a->colour_by, k));
            }
        }
        ImGui::EndTooltip();
    }
    return hovered;
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
            ImGui::SetNextItemAllowOverlap();   // the Colour by combo sits over the canvas
            ImGui::InvisibleButton("canvas", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
            handle_mouse(a, origin);
            app_recompute(a);   // what the mouse and the keys just changed, so the road is drawn with its points
            const Canvas c = { origin, ImGui::GetWindowDrawList() };
            c.list->PushClipRect(origin, ImVec2(origin.x + size.x, origin.y + size.y), true);
            c.list->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), BACKGROUND);
            draw_grid(c, a->view);
            draw_road(c, *a, app_colour_classes(a));
            draw_points(c, *a);
            draw_box(c);
            draw_curve_preview(c, a);
            c.list->PopClipRect();
            const bool over_canvas = ImGui::IsItemHovered();
            const bool over_combo = colour_by_combo(a, origin);
            if (over_canvas && !over_combo) draw_tooltip(c, *a);
        }
    }
    ImGui::End();
}
