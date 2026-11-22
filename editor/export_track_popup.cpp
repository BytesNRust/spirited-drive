// See export_track_popup.h.
#include "export_track_popup.h"

extern "C" {
#include "original_track.h"
#include "original_track_trace.h"
}
#include "formats/mod_ini.h"
#include "help_marker.h"
#include "imgui.h"

void export_track_popup(App *a)
{
    static const char *const NAME = "Export original track";
    static char why[OT_STEMS][160];   // why a track cannot be exported ("" when it can), worked out when the window comes up
    if (a->export_track_dialog && !ImGui::IsPopupOpen(NAME)) ImGui::OpenPopup(NAME);
    if (!ImGui::BeginPopupModal(NAME, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    if (!a->export_track_dialog) {
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }
    if (ImGui::IsWindowAppearing())
        for (int i = 0; i < OT_STEMS; ++i)
            if (original_track_exportable(a->export_src.cd, original_track_stem(i), why[i], sizeof why[i])) why[i][0] = '\0';
    ImGui::TextUnformatted("Name");
    ImGui::SameLine();
    if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
    if (ImGui::InputText("##name", a->export_track_name, sizeof a->export_track_name) && !a->export_track_id_typed)
        workspace_id_from_name(a->export_track_name, a->export_track_id, sizeof a->export_track_id, MOD_INI_ID_MAX);
    ImGui::TextUnformatted("Id  ");
    ImGui::SameLine();
    if (ImGui::InputText("##id", a->export_track_id, sizeof a->export_track_id)) a->export_track_id_typed = 1;
    ImGui::SameLine();
    help_marker("The id is the track's folder in the workspace's tracks/. It is made from the name; type over it to choose another.");
    ImGui::TextUnformatted("Take the road of");
    ImGui::SameLine();
    if (ImGui::BeginCombo("##track", original_track_stem(a->export_track_pick))) {
        for (int i = 0; i < OT_STEMS; ++i) {
            ImGui::BeginDisabled(why[i][0] != '\0');
            if (ImGui::Selectable(original_track_stem(i), i == a->export_track_pick)) a->export_track_pick = i;
            ImGui::EndDisabled();
            if (why[i][0] != '\0' && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("%s", why[i]);
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    help_marker("The new track follows this CD track's road: its bends, hills, width and banking, to a few centimetres. Its scenery, terrain and textures are Studio's own generic ones, "
                "and the CD's light and masks stay with the original track. Tracks of more than 599 segments (the long ones) cannot be made.");
    if (a->export_track_error[0] != '\0') ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "%s", a->export_track_error);
    ImGui::Spacing();
    const bool create = ImGui::Button("Create") || ImGui::IsKeyPressed(ImGuiKey_Enter, false);
    ImGui::SameLine();
    const bool cancel = ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape, false);
    if (create && app_export_original_track(a, original_track_stem(a->export_track_pick), a->export_track_name, a->export_track_id)) a->export_track_dialog = 0;
    if (cancel) a->export_track_dialog = 0, a->export_track_error[0] = '\0';
    if (!a->export_track_dialog) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}
