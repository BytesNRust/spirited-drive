// The Road key window, a tab beside Node (phase 2 of the editor): the values a road key on the selected point sets on each side of the road, Left | Right.
// A value the point inherits from the keys before shows dimmed; choosing one sets it on this point's key, right-click lets it inherit again. Step 2b's row is
// the shoulder's surface; step 3's are below it: the shoulder's width, how far the wall is, its kind and the fence.
#include "panels.h"

#include "imgui.h"

#include <float.h>
#include <stdlib.h>
#include <string.h>

namespace {

// one side's shoulder surface: a combo Verge / Tarmac showing what the point has, dimmed when inherited; right-click on a set one inherits again
void surface_cell(App *a, int point, int side, TraceSurface set, TraceSurface shown)
{
    static const char *const NAMES[] = { "Off-road", "Asphalt" };   // TRACE_SURFACE_VERGE, TRACE_SURFACE_TARMAC
    const bool own = set != TRACE_SURFACE_INHERIT;
    int choice = shown == TRACE_SURFACE_TARMAC ? 1 : 0;
    ImGui::PushID(side);
    if (!own) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::Combo("##surface", &choice, NAMES, 2)) app_set_point_surface(a, point, side, choice == 1 ? TRACE_SURFACE_TARMAC : TRACE_SURFACE_VERGE);
    if (!own) ImGui::PopStyleColor();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s\nAsphalt: grips as the road does.\nOff-road: the Verge material of the Track panel; the car loses grip "
                          "(the player's car; the rivals do not feel either).",
                          own ? "Set on this point. Right-click to let it inherit again." : "Inherited from the points before. Choose to set it on this point.");
    if (own && ImGui::BeginPopupContextItem("##inherit")) {
        if (ImGui::MenuItem("Inherit again")) app_set_point_surface(a, point, side, TRACE_SURFACE_INHERIT);
        ImGui::EndPopup();
    }
    ImGui::PopID();
}

// a side value's tip: set on the point, or not; a width the point does not set blends from the key before to the key after (it may show 2.37), a kind holds
const char *inherit_tip(bool own, bool blends)
{
    if (own) return "Set on this point. Right-click to let it inherit again.";
    return blends ? "Not set on this point: the road blends here from the key before to the key after. Change it to set it on this point."
                  : "Not set on this point: it holds from the key before. Change it to set it on this point.";
}

// the right-click menu of a side value the point sets: "Inherit again" clears it
void inherit_menu(App *a, int point, int field, int side, bool own)
{
    if (own && ImGui::BeginPopupContextItem("##inherit")) {
        if (ImGui::MenuItem("Inherit again")) app_clear_point_side(a, point, field, side);
        ImGui::EndPopup();
    }
}

// one side's width in metres (the shoulder, or the wall's distance from the road edge), formatted as the Points table's width. The key keeps a quarter of a
// metre; while the drag lasts the cell keeps the dragged value itself, so a slow drag (Shift: a tenth of the speed) adds up to the next quarter
void metres_cell(App *a, int point, int field, int side, bool own, double shown, const char *what)
{
    static ImGuiID held_id = 0;   // the cell being dragged, and its value as dragged (one drag at a time)
    static double held = 0;
    const double lo = 0, hi = TRACE_SIDE_MAX_M;
    ImGui::PushID(field * 2 + side);
    const ImGuiID id = ImGui::GetID("##m");
    double metres = held_id == id ? held : shown;
    if (!own) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::DragScalar("##m", ImGuiDataType_Double, &metres, 0.05f, &lo, &hi, "%.2f", ImGuiSliderFlags_AlwaysClamp)) {
        held_id = id, held = metres;
        app_set_point_side(a, point, field, side, metres);
    }
    if (held_id == id && !ImGui::IsItemActive()) held_id = 0;
    if (!own) ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s\n%s", inherit_tip(own, true), what);
    inherit_menu(a, point, field, side, own);
    ImGui::PopID();
}

// one side's wall: Hard or Soft
void wall_cell(App *a, int point, int side, bool own, double shown)
{
    static const char *const NAMES[] = { "Hard", "Soft" };
    int choice = shown == TRACK_GEN_WALL_SOFT ? 1 : 0;
    ImGui::PushID(TRACE_SIDE_WALL * 2 + side);
    if (!own) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::Combo("##wall", &choice, NAMES, 2)) app_set_point_side(a, point, TRACE_SIDE_WALL, side, choice == 1 ? TRACK_GEN_WALL_SOFT : TRACK_GEN_WALL_HARD);
    if (!own) ImGui::PopStyleColor();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s\nBoth stop the car at the wall.\nHard: a hit makes the crash sound, a hard one is recorded in the race, and the car is turned "
                          "off the wall twice as sharply.\nSoft: no sound and nothing recorded; the car is turned off the wall more gently.",
                          inherit_tip(own, false));
    inherit_menu(a, point, TRACE_SIDE_WALL, side, own);
    ImGui::PopID();
}

// one side's fence: on or off
void fence_cell(App *a, int point, int side, bool own, double shown)
{
    bool on = shown != 0;
    ImGui::PushID(TRACE_SIDE_FENCE * 2 + side);
    if (!own) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    if (ImGui::Checkbox(on ? "On###fence" : "Off###fence", &on)) app_set_point_side(a, point, TRACE_SIDE_FENCE, side, on ? 1 : 0);
    if (!own) ImGui::PopStyleColor();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s\nOn: the wall is drawn on this side (the game's fence).\nOff: it is not drawn, but it still stops the car where Wall from edge says.\n"
                          "The game keeps one fence setting per 4 nodes, the one at their first node.",
                          inherit_tip(own, false));
    inherit_menu(a, point, TRACE_SIDE_FENCE, side, own);
    ImGui::PopID();
}

// step 3's rows: what the point has each side, as the keys make it, set ones bright and inherited ones dimmed. `key` and `values` are copies taken before
// the table is drawn: a cell that changes the keys this frame (and may move or free them) changes nothing the later cells read
void side_rows(App *a, int point, const TraceKey *key, const double values[TRACE_SIDE_FIELDS][2])
{
    static const char *const LABELS[TRACE_SIDE_FIELDS] = { "Shoulder (m)", "Wall from edge (m)", "Wall", "Fence" };
    static const char *const WHAT[2] = { "How far the shoulder reaches past the road's edge.",
                                         "How far past the road's edge the wall stands: it stops the car there. At most 31.875 m from the centre." };
    for (int field = 0; field < TRACE_SIDE_FIELDS; ++field) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(LABELS[field]);
        for (int side = TRACK_GEN_LEFT; side <= TRACK_GEN_RIGHT; ++side) {
            ImGui::TableSetColumnIndex(1 + side);
            const bool own = key != nullptr && (key->has & TRACE_KEY_SIDE(field, side));
            const double shown = values[field][side];
            if (field == TRACE_SIDE_WALL) wall_cell(a, point, side, own, shown);
            else if (field == TRACE_SIDE_FENCE) fence_cell(a, point, side, own, shown);
            else metres_cell(a, point, field, side, own, shown, WHAT[field]);
        }
    }
}

// a point's side values as the keys make them: the App's, made with the road; while there is no road, worked out here. 0 when out of memory
bool point_side_values(const App *a, int point, double out[TRACE_SIDE_FIELDS][2])
{
    if (a->road_made && a->point_sides != nullptr && point < a->point_count) {
        memcpy(out, a->point_sides[point], sizeof(double) * TRACE_SIDE_FIELDS * 2);
        return true;
    }
    double (*all)[TRACE_SIDE_FIELDS][2] = static_cast<double (*)[TRACE_SIDE_FIELDS][2]>(malloc(sizeof *all * (size_t)a->trace.count));
    if (all == nullptr) return false;
    track_trace_point_sides(a->trace.points, a->trace.count, a->trace.keys, a->trace.key_count, all);
    memcpy(out, all[point], sizeof(double) * TRACE_SIDE_FIELDS * 2);
    free(all);
    return true;
}

}   // namespace

void road_key_panel(App *a)
{
    const int point = a->selected;
    if (point < 0 || point >= a->trace.count) {
        ImGui::TextDisabled("Select a point to set its road profile.");
        return;
    }
    ImGui::Text("%s at point %d", PANEL_ROAD_KEY, point);
    // the point's key and side values as they are before any cell draws: copies, as a cell's change may move or free the keys
    const int at = track_trace_key_find(a->trace.keys, a->trace.key_count, point);
    TraceKey own_key = {};
    if (at >= 0) own_key = a->trace.keys[at];
    const TraceKey *key = at >= 0 ? &own_key : nullptr;
    double values[TRACE_SIDE_FIELDS][2];
    if (!point_side_values(a, point, values)) {
        ImGui::TextDisabled("out of memory");
        return;
    }
    // what the point has, each side, as the keys up to it make it (a key after it changes nothing here)
    TraceSurface *left = static_cast<TraceSurface *>(malloc(sizeof(TraceSurface) * (size_t)(point + 1)));
    TraceSurface *right = static_cast<TraceSurface *>(malloc(sizeof(TraceSurface) * (size_t)(point + 1)));
    if (left == nullptr || right == nullptr) {
        free(left);
        free(right);
        ImGui::TextDisabled("out of memory");
        return;
    }
    track_trace_point_surfaces(a->trace.keys, a->trace.key_count, point + 1, left, right);
    if (ImGui::BeginTable("road_key", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableSetupColumn("Left");
        ImGui::TableSetupColumn("Right");
        ImGui::TableHeadersRow();
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Shoulder surface");
        const bool sets = key != nullptr && (key->has & TRACE_KEY_SHOULDER_SURFACE);
        ImGui::TableSetColumnIndex(1);
        surface_cell(a, point, TRACK_GEN_LEFT, sets ? key->shoulder_l_surface : TRACE_SURFACE_INHERIT, left[point]);
        ImGui::TableSetColumnIndex(2);
        surface_cell(a, point, TRACK_GEN_RIGHT, sets ? key->shoulder_r_surface : TRACE_SURFACE_INHERIT, right[point]);
        side_rows(a, point, key, values);
        ImGui::EndTable();
    }
    free(left);
    free(right);
}
