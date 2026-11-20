// The Road key block under the Points table (phase 2 of the editor): the values a road key on the selected point sets on each side of the road, Left | Right.
// A value the point inherits from the keys before shows dimmed; choosing one sets it on this point's key, right-click lets it inherit again. Step 2b's row is
// the shoulder's surface; step 3 adds its rows (shoulder width, barrier, wall, fence) below it.
#include "panels.h"

#include "imgui.h"

#include <float.h>
#include <stdlib.h>

namespace {

const int ROWS = 1;   // the rows of the block's table (step 3 raises it with its own)

// one side's shoulder surface: a combo Verge / Tarmac showing what the point has, dimmed when inherited; right-click on a set one inherits again
void surface_cell(App *a, int point, int side, TraceSurface set, TraceSurface shown)
{
    static const char *const NAMES[] = { "Verge", "Tarmac" };
    const bool own = set != TRACE_SURFACE_INHERIT;
    int choice = shown == TRACE_SURFACE_TARMAC ? 1 : 0;
    ImGui::PushID(side);
    if (!own) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::Combo("##surface", &choice, NAMES, 2)) app_set_point_surface(a, point, side, choice == 1 ? TRACE_SURFACE_TARMAC : TRACE_SURFACE_VERGE);
    if (!own) ImGui::PopStyleColor();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s\nVerge: the verge material of the Track panel, which slows a car off the road.\nTarmac: the road's own surface, no slow-down "
                          "(the player's car; the rivals do not feel either).",
                          own ? "Set on this point. Right-click to let it inherit again." : "Inherited from the points before. Choose to set it on this point.");
    if (own && ImGui::BeginPopupContextItem("##inherit")) {
        if (ImGui::MenuItem("Inherit again")) app_set_point_surface(a, point, side, TRACE_SURFACE_INHERIT);
        ImGui::EndPopup();
    }
    ImGui::PopID();
}

}   // namespace

float road_key_panel_height()
{
    const ImGuiStyle &st = ImGui::GetStyle();
    return ImGui::GetTextLineHeightWithSpacing() + (ROWS + 1) * (ImGui::GetFrameHeight() + 2 * st.CellPadding.y) + 2 * st.ItemSpacing.y;
}

void road_key_panel(App *a)
{
    const int point = a->selected;
    if (point < 0 || point >= a->trace.count) {
        ImGui::TextDisabled("Road key: select a point to set the road from there on, each side.");
        return;
    }
    ImGui::Text("Road key on point %d", point);
    const int at = track_trace_key_find(a->trace.keys, a->trace.key_count, point);
    const TraceKey *key = at >= 0 ? &a->trace.keys[at] : nullptr;
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
        ImGui::EndTable();
    }
    free(left);
    free(right);
}
