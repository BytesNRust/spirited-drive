// The Track light window (panels.h): a CD track's light file (one file, a block for each time of day): the sun, the tunnels, the lamps and the retouches of the time shown,
// and the photo of the working copy (the game run headless at the photo's node). Every edit goes through the model (light_track.h): a refused value shows the game's rule.
#include "panels.h"
#include "ui_probe.h"

#include "command_keys.h"
#include "help_marker.h"

#include <float.h>
#include <stdio.h>
#include <string.h>

const char *const PANEL_LIGHT = "Track light";
const char *const PANEL_LIGHT_PHOTO = "Light photo";

namespace {

const ImVec4 BAD_TEXT(1.0f, 0.35f, 0.35f, 1.0f);

void bad_text(const char *text)
{
    ImGui::PushStyleColor(ImGuiCol_Text, BAD_TEXT);
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
}

void picker(App *a)
{
    LightUi &ui = a->light;
    ImGui::TextUnformatted("Track");
    ImGui::SameLine();
    if (a->original_dir[0] != '\0') {   // an original track's own window: the track is fixed, only the time changes
        ImGui::TextUnformatted(light_ui_track(ui.track));
    } else {
        ImGui::SetNextItemWidth(90.0f);
        if (ImGui::BeginCombo("##lighttrack", light_ui_track(ui.track))) {
            for (int i = 0; i < LIGHT_UI_TRACKS; ++i) {
                if (ImGui::Selectable(light_ui_track(i), i == ui.track) && i != ui.track) app_light_select(a, i, ui.time);
                char name[24];
                snprintf(name, sizeof name, "track_%s", light_ui_track(i));
                ui_probe(name);
            }
            ImGui::EndCombo();
        }
        ui_probe("track_combo");
    }
    ImGui::SameLine();
    help_marker(a->original_dir[0] != '\0' ? "This original track's light file, in its folder in the workspace: one file, a block for each time of day. Changing the track asks for Save or Revert first. Publish to the game (File menu) copies them into the game's data/."
                                           : "The CD's tracks, as the game's files name them. Their light files are the repo's data/track_light, one per track, with a block for each time of day. Changing the track asks for "
                                             "Save or Revert first.");
    for (int t = 0; t < LIGHT_UI_TIMES; ++t) {
        if (t > 0) ImGui::SameLine();
        if (ImGui::RadioButton(light_ui_time_name(t), ui.time == t) && ui.time != t) app_light_select(a, ui.track, t);
        char probe[16];
        snprintf(probe, sizeof probe, "time_%c", light_ui_time_letter(t));
        ui_probe(probe);
    }
    ImGui::TextUnformatted("View");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(130.0f);
    if (ImGui::BeginCombo("##lightview", light_photo_view_name(ui.view))) {
        for (int i = 0; i < LIGHT_VIEWS; ++i) {
            if (ImGui::Selectable(light_photo_view_name(i), i == ui.view)) app_light_set_view(a, i);
            char name[16];
            snprintf(name, sizeof name, "view_%d", i);
            ui_probe(name);
        }
        ImGui::EndCombo();
    }
    ui_probe("view_combo");
    ImGui::SameLine();
    bool hud = ui.hud != 0;
    if (ImGui::Checkbox("HUD##lighthud", &hud)) app_light_set_hud(a, hud ? 1 : 0);
    ui_probe("hud");
    ImGui::SameLine();
    help_marker("The camera of the photo and of the comparison pictures: Bumper has no car in the picture (the track's light alone); Dash is the cockpit; Chase near, mid and far "
                "and High show the car from 4 to 16 m behind (their photos run longer, the camera glides to its place). It is kept for the next time. HUD: the game's top bar, tach, map and mirror in the picture; off by default, so the picture is the light alone (outside the car).");
}

void node_row(App *a)
{
    LightUi &ui = a->light;
    const int last = ui.session.has_file ? static_cast<int>(ui.session.nodes) - 1 : 0;
    ImGui::TextUnformatted("Photo at node");
    ImGui::SameLine();
    help_marker("The road node the car is put at for the photo (the game's TNFS_START_NODE). The photo is the game run headless, a moment after each change.");
    int node = ui.node;
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::SliderInt("##lightnode", &node, 0, last > 0 ? last : 1)) app_light_set_node(a, node);
    ui_probe("node_slider");
    if (ImGui::Button("-10")) app_light_set_node(a, ui.node - 10);
    ui_probe("node_m10");
    ImGui::SameLine();
    if (ImGui::Button("-1")) app_light_set_node(a, ui.node - 1);
    ui_probe("node_m1");
    ImGui::SameLine();
    if (ImGui::Button("+1")) app_light_set_node(a, ui.node + 1);
    ui_probe("node_p1");
    ImGui::SameLine();
    if (ImGui::Button("+10")) app_light_set_node(a, ui.node + 10);
    ui_probe("node_p10");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.0f);
    if (ImGui::InputInt("##lighttyped", &node, 0, 0, ImGuiInputTextFlags_EnterReturnsTrue)) app_light_set_node(a, node);
}

// the test drive: the game in a window beside Studio on this track, time and node, following every edit; the weather is its own choice
void drive_row(App *a)
{
    LightUi &ui = a->light;
    const bool running = app_light_drive_running(a);
    if (ImGui::Button(running ? "Stop test drive" : "Test drive")) {
        if (running) app_light_drive_stop(a);
        else app_light_drive(a);
    }
    ui_probe("drive");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::CalcTextSize("Storm").x + 2 * ImGui::GetStyle().FramePadding.x + ImGui::GetFrameHeight());
    if (ImGui::BeginCombo("##driveweather", light_drive_weather_name(ui.weather))) {
        for (int w = 0; w < LIGHT_DRIVE_WEATHERS; ++w) {
            if (ImGui::Selectable(light_drive_weather_name(w), w == ui.weather)) app_light_set_weather(a, w);
            char name[24];
            snprintf(name, sizeof name, "weather_%s", light_drive_weather_name(w));
            ui_probe(name);
        }
        ImGui::EndCombo();
    }
    ui_probe("weather_combo");
    ImGui::SameLine();
    help_marker("Test drive opens the game in a window of its own on this track, time of day and node, with the car standing there. Every change here reaches it within a second, "
                "without Save, and it restarts when you change the track, the time, the node or the weather. In the game's window: the + cross shows what the mouse points at, "
                "F10 holds the car or lets it go, F11 copies a brightness line for the node. The photo waits while it runs.");
    if (running) ImGui::TextDisabled("test drive running: %s, %s, node %u, %s", ui.drive.track, light_ui_time_name(ui.time), ui.drive.node, light_drive_weather_name(ui.drive.weather));
}

void file_buttons(App *a)
{
    const LightUi &ui = a->light;
    const bool dirty = app_modified(a);
    ImGui::BeginDisabled(!dirty);
    const bool save_now = ImGui::Button("Save");
    ui_probe("save");
    if (save_now) app_light_save(a);
    ImGui::SameLine();
    if (ImGui::Button("Revert")) app_light_revert(a);
    ui_probe("revert");
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (dirty) ImGui::TextDisabled("unsaved changes");
    else if (ui.session.has_file) ImGui::TextDisabled("%s.txt", ui.session.track);
    if (ui.copy_note[0] != '\0') {   // what the last Copy to... did, the detail hour by hour in its (?)
        ImGui::SameLine();
        ImGui::TextDisabled("%s", ui.copy_note);
        ui_probe("copy_note");
        if (ui.copy_detail[0] != '\0' && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", ui.copy_detail);
    }
    ImGui::SameLine();
    help_marker("Save writes only this track's light file in the light folder (the one in the Window's caption), and never a folder under original/. The file's other lines "
                "stay exactly as they are; a new line goes after the last. Revert goes back to the file as it was read or saved.");
}

// the light folder this window tunes (or, in red, that none is set) and the button that asks for another
void folder_row(App *a)
{
    if (a->original_dir[0] != '\0') {   // an original track: its folder in the workspace, no other to choose
        ImGui::TextUnformatted("Original track");
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextWrapped("%s", a->original_dir);
        ImGui::PopStyleColor();
        return;
    }
    ImGui::TextUnformatted("Light folder");
    ImGui::SameLine();
    if (ImGui::Button("Choose...")) a->go = APP_ACT_LIGHT_PICK;
    if (a->light_dir[0] != '\0' && a->light.open) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextWrapped("%s", a->light_dir);
        ImGui::PopStyleColor();
    } else {
        bad_text(a->light.error[0] != '\0' ? a->light.error : "no folder set");
    }
}

}   // namespace

void light_panel(App *a)
{
    LightUi &ui = a->light;
    if (ImGui::Begin(PANEL_LIGHT)) {
        folder_row(a);
        if (!ui.open) {   // no folder yet: nothing to tune until one is chosen
            ImGui::End();
            return;
        }
        picker(a);
        if (!ui.session.has_file) {
            ImGui::TextDisabled("No light file for this track");
            ImGui::SameLine();
            help_marker("This track's light is made in code: the CD has no light file for it, so there is nothing to edit or save here.");
        } else {
            node_row(a);
            file_buttons(a);
            drive_row(a);
            if (ui.error[0] != '\0') bad_text(ui.error);
            light_track_rows(a);   // the time's sun, the tunnels, the lamps and the retouches (light_track_panel.cpp)
        }
    }
    ImGui::End();
}

void light_photo_panel(App *a, ImTextureID photo)
{
    const LightUi &ui = a->light;
    if (ImGui::Begin(PANEL_LIGHT_PHOTO)) {
        const bool running = app_light_busy(a);
        if (running) ImGui::TextDisabled("refreshing...");
        else if (ui.photo.rgba != nullptr) ImGui::TextDisabled("%s, node %d, %s", ui.session.track, ui.photo.node, light_ui_time_name(ui.time));
        if (ui.photo.rgba != nullptr && photo != 0) {   // the picture, as big as the window leaves room for; the last good one stays while the next is made
            const ImVec2 room = ImGui::GetContentRegionAvail();
            const float pw = static_cast<float>(ui.photo.w), ph = static_cast<float>(ui.photo.h);
            float w = room.x < pw ? room.x : pw;
            if (w * ph / pw > room.y) w = room.y * pw / ph;
            if (w > 8.0f) ImGui::Image(photo, ImVec2(w, w * ph / pw));
        } else if (!running && ui.session.has_file && ui.error[0] == '\0') {
            ImGui::TextDisabled("no photo yet");
        }
        if (ui.open && ui.error[0] != '\0' && ui.photo.rgba == nullptr) bad_text(ui.error);   // with no folder the folder row says it
    }
    ImGui::End();
}

void light_shortcuts(App *a)
{
    if (command_key(ImGui::GetIO().WantTextInput) == COMMAND_SAVE) app_save(a);
}
