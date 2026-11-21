// The Masks panel of an original track (dep103): a row for each texture that has a mask, with its state; a click shows the mask over the texture, read-only. The PSDs generate the
// PNGs the game reads (track_masks.h); a PNG edited by hand is kept as it is and says so, with Regenerate from PSD.
#include "panels.h"

#include "help_marker.h"

#include <stdio.h>

const char *const PANEL_TRACK_MASKS = "Track masks";

namespace {

// a size from the room the window leaves, kept between lo and hi; a window not laid out yet can report an infinite or no room, and ImGui's own sums would turn that into NaN
float bounded(float v, float lo, float hi)
{
    return v > lo ? (v < hi ? v : hi) : lo;
}

ImVec4 state_colour(TrackMaskState s)
{
    if (s == TM_EDITED || s == TM_BAD) return ImVec4(1.0f, 0.75f, 0.25f, 1.0f);
    if (s == TM_EMPTY) return ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
    return ImGui::GetStyleColorVec4(ImGuiCol_Text);
}

void summary(const TrackMasks &m)
{
    int n[7] = { 0 };
    for (int i = 0; i < m.count; ++i) ++n[m.row[i].state];
    ImGui::Text("%d masks: %d ok", m.count, n[TM_OK]);
    if (n[TM_PNG_SOURCE] > 0) ImGui::SameLine(), ImGui::TextDisabled("%d PNG only", n[TM_PNG_SOURCE]);
    if (n[TM_EMPTY] > 0) ImGui::SameLine(), ImGui::TextDisabled("%d PSD not painted", n[TM_EMPTY]);
    if (n[TM_EDITED] > 0) ImGui::SameLine(), ImGui::TextColored(state_colour(TM_EDITED), "%d edited by hand", n[TM_EDITED]);
    if (n[TM_BAD] > 0) ImGui::SameLine(), ImGui::TextColored(state_colour(TM_BAD), "%d unreadable", n[TM_BAD]);
}

}   // namespace

void track_masks_panel(App *a, ImTextureID picture)
{
    if (!ImGui::Begin(PANEL_TRACK_MASKS)) {
        ImGui::End();
        return;
    }
    const TrackMasks &m = a->tmasks;
    summary(m);
    ImGui::SameLine();
    help_marker("The owner's PSDs (art-source/masks) make the PNGs the game reads (art-generated/track_masks), on their own when a PSD changes. A texture with a PNG and no PSD uses the PNG. "
                "A PSD with nothing painted in its mask layer is not used. A PNG edited by hand is kept until Regenerate from PSD. Click a row to see its mask over the texture.");
    if (a->tmask_note[0] != '\0') ImGui::TextWrapped("%s", a->tmask_note);
    if (m.count == 0) {
        ImGui::TextDisabled("This track has no masks yet.");
        ImGui::End();
        return;
    }
    if (ImGui::BeginTable("##trackmasks", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders, ImVec2(0, bounded(ImGui::GetContentRegionAvail().y * 0.42f, 120.0f, 600.0f)))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Texture");
        ImGui::TableSetupColumn("Size");
        ImGui::TableSetupColumn("Mask");
        ImGui::TableSetupColumn("State");
        ImGui::TableHeadersRow();
        for (int i = 0; i < m.count; ++i) {
            const TrackMaskRow &r = m.row[i];
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::PushID(i);
            if (ImGui::Selectable(r.name, a->tmask_sel == i, ImGuiSelectableFlags_SpanAllColumns)) app_track_mask_select(a, i);
            ImGui::PopID();
            ImGui::TableNextColumn();
            if (r.width > 0) ImGui::Text("%dx%d", r.width, r.height);
            ImGui::TableNextColumn();
            ImGui::Text("%d%s", r.reflective, r.lamps > 0 ? "" : "");
            if (r.lamps > 0) ImGui::SameLine(0, 4), ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.0f, 1.0f), "+%d lamp", r.lamps);
            ImGui::TableNextColumn();
            ImGui::TextColored(state_colour(r.state), "%s", track_mask_state_text(r.state));
            if (r.note[0] != '\0' && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", r.note);
        }
        ImGui::EndTable();
    }
    const int sel = a->tmask_sel;
    if (sel >= 0 && sel < m.count) {
        const TrackMaskRow &r = m.row[sel];
        ImGui::Text("%s", r.name);
        if (r.has_psd && r.state != TM_EMPTY && r.state != TM_BAD) {
            ImGui::SameLine();
            if (ImGui::SmallButton("Regenerate from PSD")) app_track_mask_regenerate(a);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Makes this PNG again from its PSD; a PNG edited by hand is replaced");
        }
        if (picture != 0 && a->tmask_w > 0) {
            const float room = bounded(ImGui::GetContentRegionAvail().x, 16.0f, 4096.0f);
            const float scale = room / static_cast<float>(a->tmask_w);
            const float w = scale > 4.0f ? 4.0f * static_cast<float>(a->tmask_w) : room;   // a small board is not blown up past 4x
            ImGui::Image(picture, ImVec2(w, w * static_cast<float>(a->tmask_h) / static_cast<float>(a->tmask_w)));
        }
    }
    ImGui::End();
}
