// The Distance strip window (a tab beside Limits): one bar per road value along the road's length, from the start to the end. A bright stretch starts at a
// key that sets the value and lasts to the next key that sets it (track_trace_strip_runs); the dim first stretch is the template's, where no key has said
// anything yet. It answers "how far does this value reach". A click on a stretch selects the point it starts at; Shift-click selects the whole stretch.
#include "panels.h"

extern "C" {
#include "app_select.h"
}

#include "help_marker.h"
#include "imgui.h"

#include <algorithm>
#include <math.h>
#include <stdio.h>
#include <vector>

namespace {

const float ROW_H = 16.0f, LABEL_PAD = 8.0f, MIN_TICK_PX = 70.0f;

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

}   // namespace

const char *const PANEL_STRIP = "Distance strip";

void strip_panel(App *a)
{
    const ImGuiID beside = panels_limits_dock();
    if (beside != 0) ImGui::SetNextWindowDockID(beside, ImGuiCond_FirstUseEver);   // a tab beside Limits
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
            const float width = std::max(ImGui::GetContentRegionAvail().x - label - LABEL_PAD - ImGui::GetStyle().ScrollbarSize, 60.0f);
            ImVec2 at = ImGui::GetCursorScreenPos();
            ruler(total, ImVec2(at.x + label + LABEL_PAD, at.y), width);
            ImGui::Dummy(ImVec2(0, ROW_H));
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
                ImGui::TextUnformatted(row.label);
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
