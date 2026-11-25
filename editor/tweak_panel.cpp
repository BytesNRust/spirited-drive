// Edit tweak, step 2 (notes/diseno-studio-flujo-tweak-2026-11-24.md): the list of the game's cars and tracks with their tweaks, shown in place of the main menu's doors, and the Tweak
// panel inside the editor of an original car or track: the tweak's folder, its files against the game repo, Check and Apply to game repo...
#include "panels.h"

#include "help_marker.h"
#include "text_elided.h"
#include "ui_probe.h"

#include <stdio.h>
#include <string.h>

const char *const PANEL_TWEAK = "Tweak";

namespace {

const ImVec4 GOOD_TEXT(0.47f, 0.86f, 0.55f, 1.0f), WARN_TEXT(0.95f, 0.78f, 0.35f, 1.0f), BAD_TEXT(1.0f, 0.35f, 0.35f, 1.0f);

}   // namespace

// the main menu while Edit tweak is on: the cars or the tracks, one row each, and Open on the chosen one
void tweak_list_view(App *a, float total)
{
    if (a->tweak_rows == 0) app_tweak_list_refresh(a);
    ImGui::SetWindowFontScale(1.25f);
    ImGui::TextUnformatted("Edit tweak");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::SameLine();
    help_marker("A tweak changes one of the game's own cars or tracks: its masks, its colors, its light. It is a folder in the workspace's tweaks/; Open makes it the first time and opens the car or the track with its panels.\nNothing goes into the game until you press Apply to game repo.");
    const bool cars = a->tweak_list == 1;
    if (ImGui::RadioButton("Car", cars)) a->tweak_list = 1, a->tweak_pick = -1;
    ui_probe("tweak_tab_car");
    ImGui::SameLine();
    if (ImGui::RadioButton("Track", !cars)) a->tweak_list = 2, a->tweak_pick = -1;
    ui_probe("tweak_tab_track");
    ImGui::SameLine(total - 90.0f);
    if (ImGui::Button("Main menu")) a->tweak_list = 0, a->tweak_pick = -1;
    ui_probe("tweak_main");
    if (a->workspace[0] == '\0') ImGui::TextColored(WARN_TEXT, "Choose a workspace first: File > Workspace.");
    const int first = cars ? 0 : EXPORT_CARS, last = cars ? EXPORT_CARS : a->tweak_rows;
    // the list is as tall as its rows (Open sits right under it); a long one scrolls inside what the window has left
    const ImGuiStyle &st = ImGui::GetStyle();
    const float row = ImGui::GetTextLineHeight() + st.CellPadding.y * 2 + 1.0f;
    const float room = ImGui::GetContentRegionAvail().y - 70.0f;
    const float want = row * static_cast<float>(last - first + 1) + 6.0f;
    const float list_height = want < room ? want : room;
    if (ImGui::BeginTable("tweaks", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_ScrollY, ImVec2(total, list_height))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 1.6f);
        ImGui::TableSetupColumn("Kind", ImGuiTableColumnFlags_WidthStretch, 0.8f);
        ImGui::TableSetupColumn("Tweak", ImGuiTableColumnFlags_WidthStretch, 1.4f);
        ImGui::TableSetupColumn("What changed", ImGuiTableColumnFlags_WidthStretch, 2.0f);
        ImGui::TableHeadersRow();
        for (int i = first; i < last; ++i) {
            const TweakRow &r = a->tweak_row[i];
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::PushID(i);
            ImGui::BeginDisabled(!r.usable);
            if (ImGui::Selectable(r.name, a->tweak_pick == i, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)) {
                a->tweak_pick = i;
                if (ImGui::IsMouseDoubleClicked(0)) app_tweak_open_row(a, i);
            }
            char probe[96];
            snprintf(probe, sizeof probe, "tweak_row_%s", r.name);
            ui_probe(probe);
            ImGui::EndDisabled();
            if (!r.usable && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("%s", r.why);
            ImGui::PopID();
            ImGui::TableNextColumn();
            ImGui::TextDisabled("%s", r.kind);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(r.has ? r.id : "-");
            ImGui::TableNextColumn();
            if (!r.usable) ImGui::TextDisabled("not available");
            else ImGui::TextUnformatted(r.summary);
        }
        ImGui::EndTable();
    }
    const bool picked = a->tweak_pick >= first && a->tweak_pick < last && a->tweak_row[a->tweak_pick].usable && a->workspace[0] != '\0';
    ImGui::BeginDisabled(!picked);
    if (ImGui::Button("Open")) app_tweak_open_row(a, a->tweak_pick);
    ui_probe("tweak_open");
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("%s", picked ? "Make the tweak of this one (the first time) and open it with its panels" : "Choose a row first");
}

namespace {

const char *state_word(const TweakFile &f)
{
    return f.state == TWEAK_SAME ? "same as the game" : f.state == TWEAK_NEW ? "new" : "changed";
}

// Apply found files the repo has changes of its own in: Replace them, Skip them, or Cancel
void own_popup(App *a)
{
    static const char *const NAME = "The game repo has changes of its own";
    if (a->tweak_own_dialog && !ImGui::IsPopupOpen(NAME)) ImGui::OpenPopup(NAME);
    if (!ImGui::BeginPopupModal(NAME, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    if (!a->tweak_own_dialog) {
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }
    ImGui::TextUnformatted("Files the repo has changed itself");
    ImGui::SameLine();
    help_marker("These files are not the game's own any more in the repo, and the tweak has another version of each.");
    if (ImGui::BeginChild("##own", ImVec2(520.0f, 150.0f), ImGuiChildFlags_Borders)) {
        for (int i = 0; i < a->tweak_st.count; ++i)
            if (a->tweak_st.item[i].own) ImGui::TextUnformatted(a->tweak_st.item[i].file);
    }
    ImGui::EndChild();
    if (ImGui::Button("Replace")) app_tweak_apply_answer(a, 1);
    ui_probe("tweak_replace");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Write the tweak's over them");
    ImGui::SameLine();
    if (ImGui::Button("Skip")) app_tweak_apply_answer(a, 0);
    ui_probe("tweak_skip");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Leave them and apply the rest");
    ImGui::SameLine();
    if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) a->tweak_own_dialog = 0;
    ui_probe("tweak_cancel");
    if (!a->tweak_own_dialog) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

// Apply to game repo: what it writes into the repo's data/, with Apply and Cancel (nothing is written before)
void confirm_popup(App *a)
{
    static const char *const NAME = "Apply to game repo";
    if (a->tweak_confirm_dialog && !ImGui::IsPopupOpen(NAME)) ImGui::OpenPopup(NAME);
    if (!ImGui::BeginPopupModal(NAME, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    if (!a->tweak_confirm_dialog) {
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }
    ImGui::TextDisabled("%s", app_repo_data(a));
    if (ImGui::BeginChild("##confirmlist", ImVec2(560.0f, 220.0f), ImGuiChildFlags_Borders)) {
        int shown = 0;
        for (int i = 0; i < a->tweak_st.count; ++i) {
            const TweakFile &f = a->tweak_st.item[i];
            if (f.state == TWEAK_SAME) continue;
            ImGui::Text("%s  %s%s%s", f.file, state_word(f), f.note[0] != '\0' ? "  " : "", f.note);
            ++shown;
        }
        if (shown == 0) ImGui::TextDisabled("Nothing to write: the repo has all of it.");
    }
    ImGui::EndChild();
    if (ImGui::Button("Apply")) app_tweak_apply_confirmed(a);
    ui_probe("tweak_confirm");
    ImGui::SameLine();
    if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) a->tweak_confirm_dialog = 0;
    ui_probe("tweak_confirm_cancel");
    if (!a->tweak_confirm_dialog) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

}   // namespace

void tweak_panel(App *a)
{
    if (ImGui::Begin(PANEL_TWEAK)) {
        if (!app_tweak_active(a)) {
            ImGui::TextDisabled("No tweak here");
            ImGui::SameLine();
            help_marker("A tweak is of one of the game's own cars or tracks.");
        } else {
            // the tweak is made again from the project when it first shows, when the masks changed under it, and by Check and Apply: no disk work per frame
            const unsigned stamp = a->kind == APP_KIND_CAR ? a->mask.generation : a->tmask_serial;
            static unsigned seen = 0;
            static char seen_dir[APP_PATH] = "";
            if (a->tweak_stale || (!a->tweak_st_ok && a->tweak_why[0] == '\0') || stamp != seen || strcmp(seen_dir, a->dir) != 0 || (a->kind == APP_KIND_LIGHT && a->tmask_serial != a->tweak_seen)) {
                seen = stamp;
                a->tweak_stale = 0;
                snprintf(seen_dir, sizeof seen_dir, "%s", a->dir);
                app_tweak_refresh(a);
            }
            ImGui::SetWindowFontScale(1.25f);
            ImGui::Text("Tweak: %s", a->tweak_id);
            ImGui::SetWindowFontScale(1.0f);
            ImGui::SameLine();
            help_marker("The tweak is this car's or track's folder in the workspace's tweaks/. It holds what differs from the game, in the game's own paths: copying its car_masks, dash_masks, track_light and track_masks folders over the repo's data/ is the same as Apply.\n"
                        "Opening one of the game's own cars or tracks makes this folder (and makes it again when its masks change); nothing else of your workspace is touched.\n"
                        "The colors are the exception: they go into the car's row of core/formats/car_paint.c, and only Apply writes them.\nApply never commits; it asks for the game repo's folder the first time and remembers it.");
            char dir[APP_PATH];
            snprintf(dir, sizeof dir, "%s/tweaks/%s", a->workspace, a->tweak_id);
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            text_elided_after("Folder  ", dir);   // cut in the middle (the folder's name stays), never in a word; the whole path on hover
            ImGui::PopStyleColor();
            if (ImGui::SmallButton("Show folder")) snprintf(a->reveal, sizeof a->reveal, "%s", dir);
            ui_probe("tweak_show");
            if (a->tweak_report[0] != '\0') ImGui::TextColored(strcmp(a->tweak_report, "ok") == 0 ? GOOD_TEXT : BAD_TEXT, "Check: %s", a->tweak_report);
            if (app_modified(a)) ImGui::TextColored(WARN_TEXT, "Unsaved changes are not in the tweak: File > Save first.");
            if (ImGui::Button("Check tweak")) app_tweak_check(a);
            ui_probe("tweak_check");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Make the tweak again from the project and look for what is wrong with it (no game needed)");
            ImGui::SameLine();
            if (ImGui::Button("Pack zip")) app_tweak_pack(a);
            ui_probe("tweak_pack");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Check, then zip what differs from the game into the tweak's build/ folder, to share. Body colors and a track's tunnel and snow facts go to the game repo only");
            ImGui::SameLine();
            if (ImGui::Button("Apply to game repo...")) app_tweak_apply_open(a);
            ui_probe("tweak_apply");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", app_repo_data(a)[0] == '\0' ? "Asks for the game repo's folder, once" : "Write the changed files into the game repo's data/ (and the colors into core/formats/car_paint.c); nothing is committed");
            if (!a->tweak_st_ok) {
                ImGui::TextColored(BAD_TEXT, "%s", a->tweak_why);
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                text_elided_after("Against ", app_tweak_base(a));
                ImGui::PopStyleColor();
                const float footer = ImGui::GetFrameHeightWithSpacing() + 4.0f;   // the summary line under the list
                if (ImGui::BeginChild("##tweakfiles", ImVec2(0, ImGui::GetContentRegionAvail().y > footer + 60.0f ? -footer : 60.0f), ImGuiChildFlags_Borders)) {
                    // what changed or is new first, then what is the game's own (the list may be longer than the box)
                    int order[TWEAK_FILES];
                    int n = 0;
                    for (int i = 0; i < a->tweak_st.count; ++i) if (a->tweak_st.item[i].state != TWEAK_SAME) order[n++] = i;
                    for (int i = 0; i < a->tweak_st.count; ++i) if (a->tweak_st.item[i].state == TWEAK_SAME) order[n++] = i;
                    for (int k = 0; k < n; ++k) {
                        const TweakFile &f = a->tweak_st.item[order[k]];
                        ImGui::TextUnformatted(f.file);
                        ImGui::SameLine(ImGui::GetWindowWidth() * 0.62f);
                        if (f.state == TWEAK_SAME) ImGui::TextDisabled("%s", state_word(f));
                        else {
                            ImGui::TextColored(WARN_TEXT, "%s", state_word(f));
                            if (f.note[0] != '\0') {   // beside the word when it fits, else on a line under the file's name
                                const float gap = ImGui::GetStyle().ItemSpacing.x * 2;
                                const float right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
                                const bool beside = ImGui::GetItemRectMax().x + gap + ImGui::CalcTextSize(f.note).x <= right;
                                if (beside) ImGui::SameLine(0, gap);
                                else ImGui::Indent(12.0f);
                                ImGui::PushStyleColor(ImGuiCol_Text, WARN_TEXT);
                                text_elided(f.note);
                                ImGui::PopStyleColor();
                                if (!beside) ImGui::Unindent(12.0f);
                            }
                        }
                        if (f.own) {   // on its own line: beside the state it does not fit the panel
                            ImGui::Indent(12.0f);
                            ImGui::PushStyleColor(ImGuiCol_Text, BAD_TEXT);
                            text_elided("the repo has changes of its own");
                            ImGui::PopStyleColor();
                            ImGui::Unindent(12.0f);
                        }
                    }
                    if (a->tweak_st.count == 0) ImGui::TextDisabled("Nothing in the tweak yet.");
                }
                ImGui::EndChild();
                ImGui::TextUnformatted(a->tweak_st.summary);
            }
            confirm_popup(a);
            own_popup(a);
        }
    }
    ImGui::End();
}
