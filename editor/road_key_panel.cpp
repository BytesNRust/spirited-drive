// The Road key window, a tab beside Node (phase 2 of the editor): the values a road key on the selected point sets on each side of the road, Left | Right.
// A value the point inherits from the keys before shows dimmed; choosing one sets it on this point's key, right-click lets it inherit again. Step 2b's row is
// the shoulder's surface; step 3's are below it: the shoulder's width, how far the wall is, its kind and the fence. Above the table, the road's values
// (both sides'): step 4's lanes and the fence step's height and picture (the game has one fence look for both sides); and each side's «Custom barrier»
// (without it the barrier follows the fence).
#include "panels.h"

extern "C" {
#include "app_select.h"
#include "key_ops.h"
}

#include "imgui.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void road_extend_menu(App *a, int point, unsigned mask)
{
    ImGui::Separator();
    if (ImGui::MenuItem("Extend to next change")) app_extend_point(a, point, mask, 0);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Run this value on past the point: the next key that sets it goes, so the value holds up to the one after.");
    if (ImGui::MenuItem("Extend to the end")) app_extend_point(a, point, mask, 1);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Run this value on to the last point: every later key that sets it goes.");
}

void road_reach_hover(App *a, int point, unsigned mask)
{
    if (!ImGui::IsItemHovered() && !ImGui::IsItemActive()) return;
    int from = point;
    if (sel_count(&a->sel) > 1 && sel_has(&a->sel, point)) {   // a selection: from its first point, and past its last
        from = sel_first(&a->sel);
        point = sel_last(&a->sel);
    }
    a->reach_from = from, a->reach_to = keys_reach(&a->trace, point, mask);
}

namespace {

const ImVec4 WARN_TEXT(1.0f, 0.69f, 0.13f, 1.0f);   // amber, as panels.cpp's
const char *const DASH = "-";   // the selected points differ on this value (the font has no em dash)
unsigned g_mixed = 0;                        // the values that differ between the selected points (app_sel_mixed), taken once a frame by road_key_panel

bool is_mixed(unsigned bit) { return (g_mixed & bit) != 0; }
// the tip's first line over a value the selected points do not share
const char *mix_note(unsigned bit)
{
    return is_mixed(bit) ? "The selected points differ here. Changing it sets it on all of them.\n" : "";
}

// ImGui::Combo, but with «—» shown while the selected points differ; the choice made is returned in *choice
bool combo_mixable(const char *id, int *choice, const char *const *names, int count, bool mixed)
{
    if (!mixed) return ImGui::Combo(id, choice, names, count);
    bool changed = false;
    if (ImGui::BeginCombo(id, DASH)) {
        for (int i = 0; i < count; ++i)
            if (ImGui::Selectable(names[i], false)) *choice = i, changed = true;
        ImGui::EndCombo();
    }
    return changed;
}
const char *const SURFACE_LABEL = "Shoulder surface";   // the first row's label (step 2b)
const char *const LABELS[TRACE_SIDE_FIELDS] = { "Shoulder (m)", "Barrier from edge (m)", "Barrier", "Fence" };   // step 3's rows
const char *const ROAD_LABELS[] = { "Fence height", "Fence picture", "Custom barrier" };   // the fence step's rows

// the label column's width: its widest label, so none is cut
float label_width()
{
    float widest = ImGui::CalcTextSize(SURFACE_LABEL).x;
    for (int field = 0; field < TRACE_SIDE_FIELDS; ++field) widest = fmaxf(widest, ImGui::CalcTextSize(LABELS[field]).x);
    for (const char *label : ROAD_LABELS) widest = fmaxf(widest, ImGui::CalcTextSize(label).x);
    return widest;
}

// the point's lanes, left + right (step 4): a combo 1+1 / 1+2 / 2+1 showing what the point has, dimmed when it holds from a key before; right-click on a
// set one inherits again. Lanes are the whole road's, not a side's: a row of road_rows' table
void lanes_row(App *a, int point, bool own, int shown)
{
    static const char *const NAMES[] = { "1+1", "1+2", "2+1" };
    static const int LANES[] = { TRACK_GEN_LANES_1_1, TRACK_GEN_LANES_1_2, TRACK_GEN_LANES_2_1 };
    int choice = shown == TRACK_GEN_LANES_1_2 ? 1 : shown == TRACK_GEN_LANES_2_1 ? 2 : 0;
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Lanes");
    ImGui::TableSetColumnIndex(1);
    if (!own) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (combo_mixable("##lanes", &choice, NAMES, 3, is_mixed(TRACE_KEY_LANES))) app_set_point_lanes(a, point, LANES[choice]);
    if (!own) ImGui::PopStyleColor();
    road_reach_hover(a, point, TRACE_KEY_LANES);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s%s\nLeft + right lanes, 5 m each.\n1+2 or 2+1: the road is 15 m wide from this point to the next one that sets lanes; a width set "
                          "inside is not used. The extra lane opens and closes over about 50 m, the shoulder, fence and barrier moving out with it.\n"
                          "The game keeps one lanes setting per 4 nodes, the one at their first node.",
                          mix_note(TRACE_KEY_LANES), own ? "Set on this point. Right-click to let it inherit again." : "Not set on this point: it holds from the key before. Choose to set it on this point.");
    if (own && ImGui::BeginPopupContextItem("##inherit_lanes")) {
        if (ImGui::MenuItem("Inherit again")) app_clear_point_lanes(a, point);
        road_extend_menu(a, point, TRACE_KEY_LANES);
        ImGui::EndPopup();
    }
}

// a road value (both sides') as a combo over the Left and Right columns: what the point has, dimmed when it holds from a key before; right-click on a set
// one inherits again. `bit` is its TRACE_KEY_ bit, `names` its values in order
void road_row(App *a, int point, const char *label, unsigned bit, const char *const *names, int count, bool own, int shown, const char *what)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::TableSetColumnIndex(1);
    ImGui::PushID(label);
    int choice = shown;
    if (!own) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (combo_mixable("##road", &choice, names, count, is_mixed(bit))) app_set_point_fence(a, point, bit, choice);
    if (!own) ImGui::PopStyleColor();
    road_reach_hover(a, point, bit);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s%s\n%s", mix_note(bit), own ? "Set on this point. Right-click to let it inherit again."
                                                              : "Not set on this point: it holds from the key before. Choose to set it on this point.", what);
    if (own && ImGui::BeginPopupContextItem("##inherit")) {
        if (ImGui::MenuItem("Inherit again")) app_clear_point_fence(a, point, bit);
        road_extend_menu(a, point, bit);
        ImGui::EndPopup();
    }
    ImGui::PopID();
}

// the road's values, both sides': the lanes, the fence's height and picture; one row each over both columns, in a table whose label column is the one below
void road_rows(App *a, int point, const TraceKey *key, const TraceFencePoint &fence)
{
    static const char *const HEIGHT[] = { "Low", "Tall" }, *const ART[TRACE_FENCE_PICTURES] = { "Planks", "Rail", "Parapet" };
    if (!ImGui::BeginTable("road", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) return;
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, label_width());
    ImGui::TableSetupColumn("");
    lanes_row(a, point, key != nullptr && (key->has & TRACE_KEY_LANES), app_point_lanes(a, point));
    const char *const both = "\nThe game draws one fence height and picture per 4 nodes for both sides, the one at their first node.";
    char what[256];
    snprintf(what, sizeof what, "Low: 0.9 m. Tall: 2 m.%s", both);
    road_row(a, point, ROAD_LABELS[0], TRACE_KEY_FENCE_HEIGHT, HEIGHT, 2, key != nullptr && (key->has & TRACE_KEY_FENCE_HEIGHT), fence.tall, what);
    snprintf(what, sizeof what, "Planks and Rail are see-through between their boards and posts; Parapet is a solid stone wall.%s", both);
    road_row(a, point, ROAD_LABELS[1], TRACE_KEY_FENCE_ART, ART, TRACE_FENCE_PICTURES, key != nullptr && (key->has & TRACE_KEY_FENCE_ART), fence.art, what);
    ImGui::EndTable();
}

// one side's shoulder surface: a combo Verge / Tarmac showing what the point has, dimmed when inherited; right-click on a set one inherits again
void surface_cell(App *a, int point, int side, TraceSurface set, TraceSurface shown)
{
    static const char *const NAMES[] = { "Off-road", "Asphalt" };   // TRACE_SURFACE_VERGE, TRACE_SURFACE_TARMAC
    const bool own = set != TRACE_SURFACE_INHERIT;
    int choice = shown == TRACE_SURFACE_TARMAC ? 1 : 0;
    ImGui::PushID(side);
    if (!own) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (combo_mixable("##surface", &choice, NAMES, 2, is_mixed(APP_SEL_MIXED_SURFACE(side)))) app_set_point_surface(a, point, side, choice == 1 ? TRACE_SURFACE_TARMAC : TRACE_SURFACE_VERGE);
    if (!own) ImGui::PopStyleColor();
    road_reach_hover(a, point, TRACE_KEY_SHOULDER_SURFACE);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s%s\nAsphalt: grips as the road does.\nOff-road: the Off-road ground of the Track panel; the car loses grip "
                          "(the player's car; the rivals do not feel either).",
                          mix_note(APP_SEL_MIXED_SURFACE(side)), own ? "Set on this point. Right-click to let it inherit again." : "Inherited from the points before. Choose to set it on this point.");
    if (own && ImGui::BeginPopupContextItem("##inherit")) {
        if (ImGui::MenuItem("Inherit again")) app_set_point_surface(a, point, side, TRACE_SURFACE_INHERIT);
        road_extend_menu(a, point, TRACE_KEY_SHOULDER_SURFACE);
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
        road_extend_menu(a, point, TRACE_KEY_SIDE(field, side));
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
    if (ImGui::DragScalar("##m", ImGuiDataType_Double, &metres, 0.05f, &lo, &hi, is_mixed(TRACE_KEY_SIDE(field, side)) && held_id != id ? DASH : "%.2f", ImGuiSliderFlags_AlwaysClamp)) {
        held_id = id, held = metres;
        app_set_point_side(a, point, field, side, metres);
    }
    if (held_id == id && !ImGui::IsItemActive()) held_id = 0;
    if (!own) ImGui::PopStyleColor();
    road_reach_hover(a, point, TRACE_KEY_SIDE(field, side));
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s%s\n%s", mix_note(TRACE_KEY_SIDE(field, side)), inherit_tip(own, true), what);
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
    if (combo_mixable("##wall", &choice, NAMES, 2, is_mixed(TRACE_KEY_SIDE(TRACE_SIDE_WALL, side)))) app_set_point_side(a, point, TRACE_SIDE_WALL, side, choice == 1 ? TRACK_GEN_WALL_SOFT : TRACK_GEN_WALL_HARD);
    if (!own) ImGui::PopStyleColor();
    road_reach_hover(a, point, TRACE_KEY_SIDE(TRACE_SIDE_WALL, side));
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s%s\nThe barrier is not drawn: it only stops the car, where Barrier from edge says. Both kinds stop it.\nHard: a hit makes the crash sound, a hard one is recorded in the race, and the car is turned "
                          "off the barrier twice as sharply.\nSoft: no sound and nothing recorded; the car is turned off the barrier more gently.",
                          mix_note(TRACE_KEY_SIDE(TRACE_SIDE_WALL, side)), inherit_tip(own, false));
    inherit_menu(a, point, TRACE_SIDE_WALL, side, own);
    ImGui::PopID();
}

// one side's fence: on or off
void fence_cell(App *a, int point, int side, bool own, double shown)
{
    bool on = shown != 0;
    ImGui::PushID(TRACE_SIDE_FENCE * 2 + side);
    if (!own) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    const bool fence_mixed = is_mixed(TRACE_KEY_SIDE(TRACE_SIDE_FENCE, side));
    if (ImGui::Checkbox(fence_mixed ? "-###fence" : on ? "On###fence" : "Off###fence", &on)) app_set_point_side(a, point, TRACE_SIDE_FENCE, side, on ? 1 : 0);
    if (!own) ImGui::PopStyleColor();
    road_reach_hover(a, point, TRACE_KEY_SIDE(TRACE_SIDE_FENCE, side));
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s%s\nOn: the game's fence is drawn on this side, at the end of the shoulder; its height and picture are the Fence rows above.\n"
                          "Off: no fence is drawn; the barrier still stops the car where the shoulder ends (or at Barrier from edge with Custom barrier).\n"
                          "The game keeps one fence setting per 4 nodes, the one at their first node.",
                          mix_note(TRACE_KEY_SIDE(TRACE_SIDE_FENCE, side)), inherit_tip(own, false));
    inherit_menu(a, point, TRACE_SIDE_FENCE, side, own);
    ImGui::PopID();
}

// a side's barrier distance without Custom: it follows the fence (where the shoulder ends), shown dimmed and locked
void followed_barrier_cell(int field, int side, double shoulder)
{
    ImGui::PushID(field * 2 + side);
    ImGui::BeginDisabled();
    double metres = shoulder;
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::DragScalar("##m", ImGuiDataType_Double, &metres, 0.0f, nullptr, nullptr, "%.2f");
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Follows the fence: the barrier stops the car where the shoulder ends, where the fence stands.\nTick Custom barrier to set it apart.");
    ImGui::PopID();
}

// a side's «Custom barrier»: on, the barrier is Barrier from edge (amber: the car stops where nothing is drawn); off, it follows the fence
void custom_cell(App *a, int point, int side, bool own, bool on)
{
    const unsigned bit = TRACE_KEY_BARRIER_CUSTOM(side);
    ImGui::PushID(100 + side);
    if (!own) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    else if (on) ImGui::PushStyleColor(ImGuiCol_Text, WARN_TEXT);
    bool tick = on;
    if (ImGui::Checkbox(is_mixed(bit) ? "-###custom" : tick ? "On###custom" : "Off###custom", &tick)) app_set_point_fence(a, point, bit, tick ? 1 : 0);
    if (!own || on) ImGui::PopStyleColor();
    road_reach_hover(a, point, bit);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s%s\nOn: the barrier stops the car at Barrier from edge, set apart from the fence (Problems says where).\n"
                          "Off: the barrier follows the fence, where the shoulder ends.",
                          mix_note(bit), inherit_tip(own, false));
    if (own && ImGui::BeginPopupContextItem("##inherit")) {
        if (ImGui::MenuItem("Inherit again")) app_clear_point_fence(a, point, bit);
        road_extend_menu(a, point, bit);
        ImGui::EndPopup();
    }
    ImGui::PopID();
}

// step 3's rows: what the point has each side, as the keys make it, set ones bright and inherited ones dimmed. `key` and `values` are copies taken before
// the table is drawn: a cell that changes the keys this frame (and may move or free them) changes nothing the later cells read
void side_rows(App *a, int point, const TraceKey *key, const double values[TRACE_SIDE_FIELDS][2], const TraceFencePoint &fence)
{
    static const char *const WHAT[2] = { "How far the shoulder reaches past the road's edge.",
                                         "How far past the road's edge the barrier is: it stops the car there, and is not drawn. At most 31.875 m from the centre." };
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
            else if (field == TRACE_SIDE_BARRIER && !fence.custom[side]) followed_barrier_cell(field, side, values[TRACE_SIDE_SHOULDER][side]);
            else if (field == TRACE_SIDE_BARRIER) {
                ImGui::PushStyleColor(ImGuiCol_Text, WARN_TEXT);   // set apart from the fence: amber, as in Problems
                metres_cell(a, point, field, side, own, shown, WHAT[field]);
                ImGui::PopStyleColor();
            } else metres_cell(a, point, field, side, own, shown, WHAT[field]);
        }
        if (field != TRACE_SIDE_BARRIER) continue;
        ImGui::TableNextRow();   // the barrier's Custom, under it
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(ROAD_LABELS[2]);
        for (int side = TRACK_GEN_LEFT; side <= TRACK_GEN_RIGHT; ++side) {
            ImGui::TableSetColumnIndex(1 + side);
            custom_cell(a, point, side, key != nullptr && (key->has & TRACE_KEY_BARRIER_CUSTOM(side)), fence.custom[side] != 0);
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

float road_key_panel_width()
{
    const ImGuiStyle &st = ImGui::GetStyle();
    const float combo = ImGui::CalcTextSize("Off-road").x + ImGui::GetFrameHeight() + 2 * st.FramePadding.x;   // the widest cell: a surface combo and its arrow
    return label_width() + 2 * combo + 6 * st.CellPadding.x + 4 + st.ScrollbarSize + 2 * st.WindowPadding.x;
}

void road_key_panel(App *a)
{
    const int point = a->selected;
    if (point < 0 || point >= a->trace.count) {
        ImGui::TextDisabled("Select a point to set its road profile.");
        return;
    }
    g_mixed = app_sel_mixed(a);
    if (sel_count(&a->sel) > 1 && sel_has(&a->sel, point)) {   // several points: the rows show what they share, «—» where they differ; a change is made on all of them (dep87 part 2)
        char which[96];
        sel_describe(&a->sel, which, sizeof which);
        const bool run = sel_last(&a->sel) - sel_first(&a->sel) + 1 == sel_count(&a->sel);
        if (run) ImGui::Text("%s at points %s (%d points)", PANEL_ROAD_KEY, which, sel_count(&a->sel));
        else ImGui::Text("%s at %d points (%s)", PANEL_ROAD_KEY, sel_count(&a->sel), which);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("The rows show the value the selected points share, or - where they differ. A change sets only that value, on all of them; what held before comes back after them.");
    } else {
        ImGui::Text("%s at point %d", PANEL_ROAD_KEY, point);
    }
    if (ImGui::SmallButton("Copy profile")) app_copy_profile(a);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Copy every road value of the selected point: width, bank, lanes, the shoulder, wall and fence of both sides.");
    ImGui::SameLine();
    ImGui::BeginDisabled(a->profile_clip.has == 0);
    if (ImGui::SmallButton("Paste profile")) app_paste_profile(a);
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip(a->profile_clip.has == 0 ? "Nothing copied yet: select a point and Copy profile."
                                                   : "Set the copied profile on the selected points; what held before comes back after them. One undo step.");
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
    TraceFencePoint fence = { 0, TRACE_FENCE_PLANKS, { 0, 0 } };
    app_point_fence(a, point, &fence);
    road_rows(a, point, key, fence);
    if (ImGui::BeginTable("road_key", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, label_width());
        ImGui::TableSetupColumn("Left");
        ImGui::TableSetupColumn("Right");
        ImGui::TableHeadersRow();
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(SURFACE_LABEL);
        const bool sets = key != nullptr && (key->has & TRACE_KEY_SHOULDER_SURFACE);
        ImGui::TableSetColumnIndex(1);
        surface_cell(a, point, TRACK_GEN_LEFT, sets ? key->shoulder_l_surface : TRACE_SURFACE_INHERIT, left[point]);
        ImGui::TableSetColumnIndex(2);
        surface_cell(a, point, TRACK_GEN_RIGHT, sets ? key->shoulder_r_surface : TRACE_SURFACE_INHERIT, right[point]);
        side_rows(a, point, key, values, fence);
        ImGui::EndTable();
    }
    free(left);
    free(right);
}
