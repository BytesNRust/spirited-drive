// The Distance strip window (a tab beside Limits): the road's height profile on top (the control points' heights over the road's length: drag a point up or down),
// then one bar per road value along the road's length, from the start to the end. A bright stretch starts at a
// key that sets the value and lasts to the next key that sets it (track_trace_strip_runs); the dim first stretch is the template's, where no key has said
// anything yet. It answers "how far does this value reach". A click on a stretch selects the point it starts at; Shift-click selects the whole stretch.
#include "panels.h"

extern "C" {
#include "app_colour.h"
#include "app_select.h"
#include "stretch_colours.h"
}

#include "help_marker.h"
#include "imgui.h"

#include <algorithm>
#include <math.h>
#include <stdio.h>
#include <vector>

namespace {

const float ROW_H = 16.0f, LABEL_PAD = 8.0f, MIN_TICK_PX = 70.0f;
const float PROFILE_H = 130.0f, HANDLE_R = 4.5f, PICK_PX = 9.0f;   // the profile's height, a point's mark and how near the mouse picks it
const float LABEL_MIN_PX = 72.0f, LABEL_MAX_SHARE = 0.3f;   // the row names' column is at most this share of the window (never under the minimum)
const int CURVE_STEPS = 12;                                         // line pieces per segment of the curve

TraceGradeUnit grade_unit = TRACE_GRADE_PERCENT;   // the % / degrees switch, kept while the program runs
int dragged = -1;                                  // the point the mouse holds in the profile, -1 for none
double drag_lo = 0, drag_hi = 0;                   // the profile's range when the drag began: it does not move under the mouse

struct Channel {
    const char *label;
    unsigned bit;   // the one TRACE_KEY_ bit the row follows
};

// every road value a key sets, in the order Road profile lists them
std::vector<Channel> channels()
{
    static const char *const FIELD[TRACE_SIDE_FIELDS] = { "Shoulder", "Barrier from edge", "Barrier", "Fence" };
    static const char *const SIDE[2] = { " (L)", " (R)" };
    static char names[TRACE_SIDE_FIELDS][2][40];
    std::vector<Channel> rows = { { "Width", TRACE_KEY_WIDTH }, { "Bank", TRACE_KEY_BANK }, { "Lanes", TRACE_KEY_LANES },
                                  { "Shoulder surface", TRACE_KEY_SHOULDER_SURFACE } };
    for (int field = 0; field < TRACE_SIDE_FIELDS; ++field)
        for (int side = 0; side < 2; ++side) {
            snprintf(names[field][side], sizeof names[field][side], "%s%s", FIELD[field], SIDE[side]);
            rows.push_back({ names[field][side], TRACE_KEY_SIDE(field, side) });
        }
    rows.push_back({ "Fence height", TRACE_KEY_FENCE_HEIGHT });
    rows.push_back({ "Fence picture", TRACE_KEY_FENCE_ART });
    rows.push_back({ "Custom barrier (L)", TRACE_KEY_BARRIER_CUSTOM(0) });
    rows.push_back({ "Custom barrier (R)", TRACE_KEY_BARRIER_CUSTOM(1) });
    return rows;
}

// the ruler's step in metres: the first of 100 m .. 10 km that keeps its labels `MIN_TICK_PX` apart
double tick_step(double total, float width)
{
    static const double STEPS[] = { 100, 200, 500, 1000, 2000, 5000, 10000 };
    for (double step : STEPS)
        if (total > 0 && step / total * width >= MIN_TICK_PX) return step;
    return STEPS[6];
}

void ruler(double total, ImVec2 origin, float width)
{
    ImDrawList *draw = ImGui::GetWindowDrawList();
    const ImU32 ink = ImGui::GetColorU32(ImGuiCol_TextDisabled);
    draw->AddLine(ImVec2(origin.x, origin.y + ROW_H - 1), ImVec2(origin.x + width, origin.y + ROW_H - 1), ink);
    if (total <= 0) return;
    const double step = tick_step(total, width);
    for (double m = 0; m <= total; m += step) {
        const float x = origin.x + (float)(m / total) * width;
        char text[24];
        if (m >= 1000) snprintf(text, sizeof text, "%g km", m / 1000.0);
        else snprintf(text, sizeof text, "%g m", m);
        draw->AddLine(ImVec2(x, origin.y + ROW_H - 5), ImVec2(x, origin.y + ROW_H - 1), ink);
        draw->AddText(ImVec2(x + 2, origin.y), ink, text);
    }
}

// one row's bars: its runs over the road, laid out as the points' distances. Returns the run under the mouse (index into `runs`, -1 for none)
int bars(const std::vector<TraceStripRun> &runs, const std::vector<double> &metres, double total, ImVec2 origin, float width)
{
    ImDrawList *draw = ImGui::GetWindowDrawList();
    const ImVec4 accent = ImGui::GetStyleColorVec4(ImGuiCol_PlotHistogram);
    int hovered = -1;
    for (size_t i = 0; i < runs.size(); ++i) {
        const TraceStripRun &run = runs[i];
        const float x0 = origin.x + (float)(metres[(size_t)run.first] / total) * width;
        const float x1 = origin.x + (float)(metres[(size_t)run.last] / total) * width;
        const ImVec2 lo(x0, origin.y + 2), hi(std::max(x1, x0 + 2.0f), origin.y + ROW_H - 2);
        ImVec4 fill = run.set ? accent : ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
        if (run.set && (i % 2) == 1) fill = ImVec4(fill.x * 0.8f, fill.y * 0.8f, fill.z * 0.8f, fill.w);   // neighbours differ, so each key shows
        const bool over = ImGui::IsMouseHoveringRect(lo, hi);
        if (over) hovered = (int)i;
        draw->AddRectFilled(lo, hi, ImGui::GetColorU32(fill));
        draw->AddRect(lo, hi, ImGui::GetColorU32(ImGuiCol_WindowBg));
    }
    return hovered;
}

// the metres of the curve at fraction `frac` of segment `seg`
double metres_at(const std::vector<double> &metres, int seg, double frac)
{
    return metres[(size_t)seg] + frac * (metres[(size_t)seg + 1] - metres[(size_t)seg]);
}

// the segment a distance falls in and the fraction along it
int segment_at(const std::vector<double> &metres, double at, double *frac)
{
    int seg = 0;
    const int last = (int)metres.size() - 2;
    while (seg < last && at > metres[(size_t)seg + 1]) ++seg;
    const double run = metres[(size_t)seg + 1] - metres[(size_t)seg];
    *frac = run > 0 ? std::min(std::max((at - metres[(size_t)seg]) / run, 0.0), 1.0) : 0.0;
    return seg;
}

// the profile: the height curve over the road, a mark at each control point (the selected ones filled), a slope over the limit in red. Dragging a mark up or down
// sets that point's height. Returns nothing: it changes the app.
void profile(App *a, const std::vector<double> &metres, double total, ImVec2 origin, float width, float label)
{
    ImDrawList *draw = ImGui::GetWindowDrawList();
    const int count = a->trace.count;
    TracePoint *points = a->trace.points;
    double lo, hi;
    track_trace_profile_range(points, count, &lo, &hi);
    if (dragged >= 0) {
        lo = drag_lo;
        hi = drag_hi;
    }
    const float top = origin.y + 4, bottom = origin.y + PROFILE_H - 4;
    const ImVec2 box0(origin.x, origin.y), box1(origin.x + width, origin.y + PROFILE_H);
    draw->AddRectFilled(box0, box1, ImGui::GetColorU32(ImGuiCol_FrameBg));
    auto row_of = [&](double h) { return (float)track_trace_profile_row_of_height(h, top, bottom, lo, hi); };
    auto col_of = [&](double m) { return origin.x + (float)(m / total) * width; };
    const ImU32 ink = ImGui::GetColorU32(ImGuiCol_TextDisabled);
    if (lo < 0 && hi > 0) draw->AddLine(ImVec2(box0.x, row_of(0)), ImVec2(box1.x, row_of(0)), ink);   // the start's height
    char text[32];
    snprintf(text, sizeof text, "%.0f m", hi);
    draw->AddText(ImVec2(origin.x - label - LABEL_PAD, top - 3), ink, text);
    snprintf(text, sizeof text, "%.0f m", lo);
    draw->AddText(ImVec2(origin.x - label - LABEL_PAD, bottom - ImGui::GetFontSize() + 3), ink, text);

    a->profile_drawn = { 1, origin.x, width, top, bottom, lo, hi, total };
    draw->PushClipRect(box0, box1, true);
    const ImU32 level = ImGui::GetColorU32(ImGuiCol_PlotLines), steep = IM_COL32(230, 70, 60, 255);
    const uint8_t *classes = a->road_made ? app_colour_classes(a) : nullptr;   // Colour by: the line takes the stretch's colour, as the Viewport's road does
    for (int seg = 0; seg + 1 < count; ++seg) {
        const bool over = fabs(track_trace_segment_grade(points, metres.data(), count, seg, TRACE_GRADE_PERCENT)) > TRACK_GEN_GRADE_MAX_PERCENT;
        ImVec2 from(col_of(metres[(size_t)seg]), row_of(points[seg].h));
        for (int k = 1; k <= CURVE_STEPS; ++k) {
            const double frac = (double)k / CURVE_STEPS;
            const ImVec2 to(col_of(metres_at(metres, seg, frac)), row_of(track_trace_height_at(points, count, seg, frac)));
            ImU32 colour = level;
            if (over) colour = steep;
            else if (classes != nullptr)
                colour = stretch_colour(a->colour_by, classes[track_trace_node_at(metres_at(metres, seg, (k - 0.5) / CURVE_STEPS), total, a->node_count)]);
            draw->AddLine(from, to, colour, 2.0f);
            from = to;
        }
    }
    for (int i = 0; i < count; ++i) {
        const ImVec2 at(col_of(metres[(size_t)i]), row_of(points[i].h));
        if (sel_has(&a->sel, i)) draw->AddCircleFilled(at, HANDLE_R, IM_COL32(255, 255, 255, 255));
        else draw->AddCircle(at, HANDLE_R, level, 0, 1.5f);
    }
    draw->PopClipRect();

    ImGui::SetCursorScreenPos(origin);
    ImGui::InvisibleButton("##profile", ImVec2(width, PROFILE_H));
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    if (ImGui::IsItemActivated()) {   // pressed: the nearest mark within reach is held, else the point nearest along the road is only selected
        int near_i = -1;
        float best = PICK_PX * PICK_PX;
        for (int i = 0; i < count; ++i) {
            const float dx = col_of(metres[(size_t)i]) - mouse.x, dy = row_of(points[i].h) - mouse.y;
            if (dx * dx + dy * dy <= best) {
                best = dx * dx + dy * dy;
                near_i = i;
            }
        }
        if (near_i >= 0) {
            dragged = near_i;
            drag_lo = lo;
            drag_hi = hi;
            app_sel_click(a, near_i, ImGui::GetIO().KeyShift ? SEL_TOGGLE : SEL_ONE);
        } else {
            double frac;
            const int seg = segment_at(metres, (mouse.x - origin.x) / width * total, &frac);
            app_sel_click(a, frac < 0.5 ? seg : seg + 1, SEL_ONE);
        }
    }
    if (dragged >= 0) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
            app_set_point_height(a, dragged, track_trace_profile_height_at_row(mouse.y, top, bottom, lo, hi));
        else dragged = -1;
    } else if (ImGui::IsItemHovered()) {
        double frac;
        const int seg = segment_at(metres, (mouse.x - origin.x) / width * total, &frac);
        char slope[32];
        track_trace_grade_text(points[seg + 1].h - points[seg].h, metres[(size_t)seg + 1] - metres[(size_t)seg], grade_unit, slope, sizeof slope);
        ImGui::SetTooltip("%.0f m: %.1f m high, the slope here %s\nDrag a point up or down to set its height.", metres_at(metres, seg, frac),
                          track_trace_height_at(points, count, seg, frac), slope);
    }
}

// under the profile: the % / degrees switch, then the selected point's height, its place and the slopes to and from it (wrapped in a narrow window)
void readout(App *a, const std::vector<double> &metres)
{
    ImGui::TextUnformatted("Slope");
    ImGui::SameLine();
    if (ImGui::RadioButton("%", grade_unit == TRACE_GRADE_PERCENT)) grade_unit = TRACE_GRADE_PERCENT;
    ImGui::SameLine();
    if (ImGui::RadioButton("\xc2\xb0", grade_unit == TRACE_GRADE_DEGREES)) grade_unit = TRACE_GRADE_DEGREES;
    ImGui::SameLine();
    static char help[160];
    snprintf(help, sizeof help, "The slope is shown as a percentage or in degrees. A slope over %.0f %% (the steepest the generator builds) is drawn red.",
             TRACK_GEN_GRADE_MAX_PERCENT);
    help_marker(help);
    const int count = a->trace.count, i = a->selected;
    if (i < 0 || i >= count) {
        ImGui::TextDisabled("no point selected");
        return;
    }
    char in[32] = "-", out[32] = "-";
    if (i > 0) track_trace_grade_text(a->trace.points[i].h - a->trace.points[i - 1].h, metres[(size_t)i] - metres[(size_t)i - 1], grade_unit, in, sizeof in);
    if (i + 1 < count) track_trace_grade_text(a->trace.points[i + 1].h - a->trace.points[i].h, metres[(size_t)i + 1] - metres[(size_t)i], grade_unit, out, sizeof out);
    ImGui::TextWrapped("point %d: %.1f m high at %.0f m; slope in %s, out %s", i, a->trace.points[i].h, metres[(size_t)i], in, out);
}

}   // namespace

const char *const PANEL_STRIP = "Distance strip";

void strip_panel(App *a)
{
    const ImGuiID beside = panels_limits_dock();
    if (beside != 0) ImGui::SetNextWindowDockID(beside, ImGuiCond_FirstUseEver);   // a tab beside Limits
    a->profile_drawn.drawn = 0;
    if (a->focus_strip) {
        ImGui::SetNextWindowFocus();
        a->focus_strip = 0;
    }
    if (ImGui::Begin(PANEL_STRIP)) {
        const int count = a->trace.count;
        std::vector<double> metres((size_t)std::max(count, 0), 0.0);
        if (count >= 2) track_trace_point_distances(a->trace.points, count, metres.data());
        const double total = count >= 2 ? metres[(size_t)count - 1] : 0.0;
        if (total <= 0) {
            ImGui::TextDisabled("The road is not made yet.");
        } else {
            help_marker("Each bar is one road value along the road. A bright stretch starts at a point whose key sets the value and lasts to the next key that "
                        "sets it; the dim stretch before the first key is the template's. Click a stretch to select the point it starts at, Shift-click to "
                        "select all its points.");
            float label = 0;
            const std::vector<Channel> rows = channels();
            for (const Channel &row : rows) label = std::max(label, ImGui::CalcTextSize(row.label).x);
            label = std::min(label, std::max(LABEL_MIN_PX, ImGui::GetContentRegionAvail().x * LABEL_MAX_SHARE));   // a narrow window keeps room for the bars: long names are cut
            const float width = std::max(ImGui::GetContentRegionAvail().x - label - LABEL_PAD - ImGui::GetStyle().ScrollbarSize, 60.0f);
            ImVec2 at = ImGui::GetCursorScreenPos();
            ruler(total, ImVec2(at.x + label + LABEL_PAD, at.y), width);
            ImGui::Dummy(ImVec2(0, ROW_H));
            const ImVec2 profile_at = ImGui::GetCursorScreenPos();
            profile(a, metres, total, ImVec2(profile_at.x + label + LABEL_PAD, profile_at.y), width, label);
            ImGui::SetCursorScreenPos(ImVec2(profile_at.x, profile_at.y + PROFILE_H + 2));
            readout(a, metres);
            std::vector<TraceStripRun> runs;
            const float first_y = ImGui::GetCursorScreenPos().y;   // the first bar's top, where the selection's lines start
            float last_y = first_y;
            for (const Channel &row : rows) {
                at = ImGui::GetCursorScreenPos();
                last_y = at.y + ROW_H;
                const int n = track_trace_strip_runs(a->trace.keys, a->trace.key_count, count, row.bit, nullptr, 0);
                runs.assign((size_t)n, TraceStripRun{ 0, 0, 0 });
                track_trace_strip_runs(a->trace.keys, a->trace.key_count, count, row.bit, runs.data(), n);
                ImGui::AlignTextToFramePadding();
                ImGui::PushClipRect(at, ImVec2(at.x + label, at.y + ROW_H), true);
                ImGui::TextUnformatted(row.label);
                ImGui::PopClipRect();
                const ImVec2 origin(at.x + label + LABEL_PAD, at.y);
                const int over = bars(runs, metres, total, origin, width);
                ImGui::SetCursorScreenPos(ImVec2(at.x, at.y + ROW_H + 2));
                if (over >= 0) {
                    const TraceStripRun &run = runs[(size_t)over];
                    const double from = metres[(size_t)run.first], to = metres[(size_t)run.last];
                    ImGui::SetTooltip("%s: %s\nfrom %.0f m to %.0f m (%.0f m), points %d to %d",
                                      row.label, run.set ? "set by the key at the first of them" : "the template's, no key sets it yet", from, to, to - from,
                                      run.first, run.last);
                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsWindowHovered()) {
                        app_sel_click(a, run.first, SEL_ONE);
                        if (ImGui::GetIO().KeyShift) app_sel_click(a, run.last, SEL_RANGE);
                    }
                }
            }
            // the selected points over every bar, so a stretch can be read against them
            ImDrawList *draw = ImGui::GetWindowDrawList();
            const float left = at.x + label + LABEL_PAD;
            for (int i = 0; i < count; ++i) {
                if (!sel_has(&a->sel, i)) continue;
                const float x = left + (float)(metres[(size_t)i] / total) * width;
                draw->AddLine(ImVec2(x, first_y), ImVec2(x, last_y), IM_COL32(255, 255, 255, 200));
            }
        }
    }
    ImGui::End();
}
