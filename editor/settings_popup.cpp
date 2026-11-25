// Edit tweak, step 3: where Studio is told about the game once. The first start offers the game's location before the doors (game_offer_row, put aside with Not now, remembered), and File > Settings
// holds the three places afterwards: the game (to test drive and to find its files), the workspace, and the game repo's data/ (where Apply to game repo writes).
#include "panels.h"

#include "help_marker.h"
#include "path_elide.h"
#include "ui_probe.h"

#include <stdio.h>
#include <string.h>

namespace {

const float VALUE_ROOM = 300.0f;   // the pixels a value (a path) has in Settings, then its buttons

// the Locate dialog's answer: the app (or program) the user picked, none when it was cancelled
void game_located(void *userdata, const char *const *list, int)
{
    if (list != NULL && list[0] != NULL) app_set_game(static_cast<App *>(userdata), list[0]);
}

void locate_game(App *a)
{
    SDL_ShowOpenFileDialog(game_located, a, NULL, NULL, 0, "/Applications", false);   // its .app, or its program
}

// one row: the label, the value (cut in the middle when it does not fit, the tooltip has it whole) and its buttons are the caller's
void value_line(const char *label, const char *value, const char *none)
{
    ImGui::TextDisabled("%s", label);
    ImGui::SameLine(150.0f);
    const char *shown = value[0] != '\0' ? value : none;
    char cut[APP_PATH];
    const float glyph = ImGui::CalcTextSize("M").x;
    path_elide(shown, glyph > 0 ? (int)(VALUE_ROOM / glyph) : 40, cut, sizeof cut);   // a fixed room: the window sizes itself to its content, so the room left is the content's own
    ImGui::TextUnformatted(cut);
    if (ImGui::IsItemHovered() && value[0] != '\0') ImGui::SetTooltip("%s", value);
}

}   // namespace

// the first start's question, above the doors: where the game is (once; Not now puts it aside for good)
void game_offer_row(App *a)
{
    if (!app_game_offer(a)) return;
    ImGui::Dummy(ImVec2(0, 8));
    ImGui::TextWrapped("Where is the game? Studio runs it for the test drives and reads its files.");
    if (ImGui::Button("Locate game...")) locate_game(a);
    ui_probe("locate_game");
    ImGui::SameLine();
    help_marker("Pick Spirited Drive's app (or its program). It is asked once; you can change it later in File > Settings.");
    ImGui::SameLine();
    if (ImGui::SmallButton("Not now")) app_game_put_aside(a);
    ui_probe("locate_not_now");
}

void settings_popup(App *a)
{
    static const char *const NAME = "Settings";
    if (a->settings_dialog && !ImGui::IsPopupOpen(NAME)) ImGui::OpenPopup(NAME);
    ImGui::SetNextWindowSize(ImVec2(620, 0), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal(NAME, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    if (!a->settings_dialog) {
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }
    value_line("Game", app_game_found(a) ? a->game : "", "not found");
    ImGui::SameLine();
    if (ImGui::SmallButton("Locate...##game")) locate_game(a);
    ui_probe("settings_game");
    ImGui::SameLine();
    help_marker("Spirited Drive's app: Studio runs it for the test drives, the photos and Install.");
    value_line("Workspace", a->workspace, "none");
    ImGui::SameLine();
    if (ImGui::SmallButton("Choose...##workspace")) app_workspace_pick(a);
    ui_probe("settings_workspace");
    ImGui::SameLine();
    help_marker("One folder with your cars/, tracks/ and tweaks/ in it.");
    value_line("Game repo", app_repo_data(a), "not set");
    ImGui::SameLine();
    if (ImGui::SmallButton("Choose...##repo")) app_repo_data_pick(a);
    ui_probe("settings_repo");
    ImGui::SameLine();
    ImGui::BeginDisabled(app_repo_data(a)[0] == '\0');
    if (ImGui::SmallButton("Clear##repo")) app_repo_data_set(a, "");
    ImGui::EndDisabled();
    ImGui::SameLine();
    help_marker("The game repo's data/ folder: where Apply to game repo writes a tweak (the colors go into core/formats/car_paint.c beside it). Nothing is committed. Apply asks for it the first time.");
    if (a->status[0] != '\0') {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextWrapped("%s", a->status);
        ImGui::PopStyleColor();
    }
    ImGui::Spacing();
    if (ImGui::Button("Close") || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) a->settings_dialog = 0;
    ui_probe("settings_close");
    if (!a->settings_dialog) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}
