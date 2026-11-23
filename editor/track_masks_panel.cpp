// The Masks panel of an original track (dep103): a row for each texture that has a mask, with its state; a click shows the mask over the texture, read-only. The PSDs generate the
// PNGs the game reads (track_masks.h); a PNG edited by hand is kept as it is and says so, with Regenerate from PSD.
#include "panels.h"
#include "ui_probe.h"

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
    if (s == TM_EMPTY || s == TM_NONE) return ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
    return ImGui::GetStyleColorVec4(ImGuiCol_Text);
}

void summary(const TrackMasks &m)
{
    int n[8] = { 0 };
    for (int i = 0; i < m.count; ++i) ++n[m.row[i].state];
    ImGui::Text("%d textures: %d ok", m.count, n[TM_OK]);
    if (n[TM_NONE] > 0) ImGui::SameLine(), ImGui::TextDisabled("%d without mask", n[TM_NONE]);
    if (n[TM_PNG_SOURCE] > 0) ImGui::SameLine(), ImGui::TextDisabled("%d PNG only", n[TM_PNG_SOURCE]);
    if (n[TM_EMPTY] > 0) ImGui::SameLine(), ImGui::TextDisabled("%d PSD not painted", n[TM_EMPTY]);
    if (n[TM_EDITED] > 0) ImGui::SameLine(), ImGui::TextColored(state_colour(TM_EDITED), "%d edited by hand", n[TM_EDITED]);
    if (n[TM_BAD] > 0) ImGui::SameLine(), ImGui::TextColored(state_colour(TM_BAD), "%d unreadable", n[TM_BAD]);
}

// a PSD with nothing painted in its mask and no PNG beside it
bool unpainted_row(const TrackMaskRow &r)
{
    return r.state == TM_EMPTY && !r.has_png;
}

// one table row of texture i
void mask_row(App *a, int i)
{
    const TrackMaskRow &r = a->tmasks.row[i];
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::PushID(i);
    if (ImGui::Selectable(r.name, a->tmask_sel == i, ImGuiSelectableFlags_SpanAllColumns)) app_track_mask_select(a, i);
    char scope[16];
    snprintf(scope, sizeof scope, "tmask%d", i);
    ui_probe_scope(scope);
    ui_probe("row");
    ui_probe_scope("");
    ImGui::PopID();
    ImGui::TableNextColumn();
    if (r.width > 0) ImGui::Text("%dx%d", r.width, r.height);
    ImGui::TableNextColumn();
    if (r.state == TM_NONE) ImGui::TextDisabled("-");
    else ImGui::Text("%d", r.reflective);
    if (r.lamps > 0) ImGui::SameLine(0, 4), ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.0f, 1.0f), "+%d lamp", r.lamps);
    ImGui::TableNextColumn();
    ImGui::TextColored(state_colour(r.state), "%s", track_mask_state_text(r.state));
    if (r.note[0] != '\0' && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", r.note);
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
                "A PSD with nothing painted in its mask layer is not used. A PNG edited by hand is kept until Update masks from PSD. Click a row to see its mask over the texture.");
    bool only = a->tmask_masked_only != 0;
    if (ImGui::Checkbox("With mask only", &only)) a->tmask_masked_only = only;
    ui_probe("tmask_only");
    ImGui::SetItemTooltip("Hides the textures that have no mask, leaving the list of those that do.");
    if (a->tmask_note[0] != '\0') ImGui::TextWrapped("%s", a->tmask_note);
    if (m.count == 0) {
        ImGui::TextDisabled("This track has no textures or masks to list.");
        ImGui::End();
        return;
    }
    if (ImGui::BeginTable("##trackmasks", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders, ImVec2(0, bounded(ImGui::GetContentRegionAvail().y * 0.42f, 120.0f, 600.0f)))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        // widths from the font, not pixels, so a narrow window (the laptop's 854 px) still reads every column: Texture is as wide as a texture's name; the «Not painted
        // yet (N)» line overflows its cell (NoClip) over the empty ones of its row; Size and Mask take their numbers, and State the rest
        const float pad = 2 * ImGui::GetStyle().CellPadding.x;
        ImGui::TableSetupColumn("Texture", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoClip, ImGui::CalcTextSize("scenery_00").x + ImGui::GetFrameHeight() + pad);
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("256x256").x + pad);
        ImGui::TableSetupColumn("Mask", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("0000").x + pad);
        ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();
        int unpainted = 0;
        for (int i = 0; i < m.count; ++i) unpainted += unpainted_row(m.row[i]) ? 1 : 0;
        for (int i = 0; i < m.count; ++i)
            if (!unpainted_row(m.row[i]) && !(a->tmask_masked_only && m.row[i].state == TM_NONE)) mask_row(a, i);
        if (unpainted > 0) {   // PSDs with no mask painted and no PNG: one closed header line at the bottom, open on click
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            char head[48];
            snprintf(head, sizeof head, "Not painted yet (%d)", unpainted);
            const bool open_unpainted = ImGui::TreeNodeEx(head, ImGuiTreeNodeFlags_SpanAllColumns);
            ui_probe("tmask_unpainted");
            if (open_unpainted) {
                for (int i = 0; i < m.count; ++i)
                    if (unpainted_row(m.row[i])) mask_row(a, i);
                ImGui::TreePop();
            }
        }
        ImGui::EndTable();
    }
    const int sel = a->tmask_sel;
    if (sel >= 0 && sel < m.count) {
        const TrackMaskRow &r = m.row[sel];
        ImGui::Text("%s", r.name);
        if (r.has_psd && r.state != TM_EMPTY && r.state != TM_BAD) {
            ImGui::SameLine();
            if (ImGui::SmallButton("Update masks from PSD")) {
                if (r.state == TM_EDITED) ImGui::OpenPopup("Replace the edited mask?");   // asks before replacing what was edited by hand
                else app_track_mask_regenerate(a);
            }
            ui_probe("tmask_regen");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Makes this PNG again from its PSD. A PNG edited by hand is not replaced without asking first.");
        }
        if (ImGui::BeginPopupModal("Replace the edited mask?", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
            if (r.state != TM_EDITED) ImGui::CloseCurrentPopup();   // nothing left to ask about
            ImGui::TextWrapped("The mask %s was edited by hand.", r.name);
            ImGui::TextWrapped("Replace it with what its PSD makes (the changes made by hand are lost), or keep it as it is.");
            if (ImGui::Button("Replace")) app_track_mask_regenerate(a), ImGui::CloseCurrentPopup();
            ui_probe("tmask_replace");
            ImGui::SameLine();
            if (ImGui::Button("Keep")) ImGui::CloseCurrentPopup();   // the PNG stays as it is
            ui_probe("tmask_keep");
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();   // nothing is done
            ui_probe("tmask_cancel");
            ImGui::EndPopup();
        }
        if (picture != 0 && a->tmask_w > 0) {
            // as wide as the panel, but no taller than what is left of it: the picture shrinks to fit and never runs past the panel's bottom edge (the whole mask is seen without scrolling)
            const float room_w = bounded(ImGui::GetContentRegionAvail().x, 16.0f, 4096.0f), room_h = bounded(ImGui::GetContentRegionAvail().y, 16.0f, 4096.0f);
            const float aspect = static_cast<float>(a->tmask_w) / static_cast<float>(a->tmask_h);
            float w = room_w < room_h * aspect ? room_w : room_h * aspect;
            if (w > 4.0f * static_cast<float>(a->tmask_w)) w = 4.0f * static_cast<float>(a->tmask_w);   // a small board is not blown up past 4x
            ImGui::Image(picture, ImVec2(w, w / aspect));
            ui_probe("tmask_picture");
        }
    }
    ImGui::End();
}
