// The track's Drive window (panels.h): Test drive, the game in its window on the open track as a time trial, with the car and the look picked here.
#include "panels.h"

#include "help_marker.h"

#include "imgui.h"
#include "imgui_internal.h"   // MarkIniSettingsDirty: the car is kept with the editor's settings

#include <stdio.h>
#include <string.h>

const char *const PANEL_DRIVE = "Drive";

namespace {

const ImVec4 BAD_TEXT(1.0f, 0.35f, 0.35f, 1.0f);

void bad_text(const char *text)
{
    ImGui::PushStyleColor(ImGuiCol_Text, BAD_TEXT);
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
}

// the car the player drives: the game's car menu, in its order; the pick is the editor's setting (ui.cpp keeps it in the ini)
void car_combo(App *a)
{
    const ExportCar *now = car_race_menu_car(a->drive_pick);
    ImGui::TextUnformatted("Car");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 13.0f);
    if (ImGui::BeginCombo("##drivecar", now != nullptr ? now->label : "")) {
        for (int pick = 0; car_race_menu_car(pick) != nullptr; ++pick)
            if (ImGui::Selectable(car_race_menu_car(pick)->label, pick == a->drive_pick)) {
                a->drive_pick = pick;
                ImGui::MarkIniSettingsDirty();
            }
        ImGui::EndCombo();
    }
}

// one line: what the run is doing, or how the last one ended, or why the button refused
void state_line(const App *a, const char *refused)
{
    const RaceJob &j = a->race;
    if (car_race_busy(&j)) {
        if (j.piece_track > 0) ImGui::Text("Running: the game took %s as track %d", j.piece, j.piece_track);
        else if (j.piece_track < 0) bad_text(j.piece_line);
        else ImGui::TextUnformatted("Running: the game is loading the track");
    } else if (refused[0] != '\0') {
        bad_text(refused);
    } else if (j.finished && j.piece[0] != '\0') {
        if (j.ok) ImGui::TextDisabled("%s", j.summary);
        else bad_text(j.summary);
    }
}

}   // namespace

void drive_panel(App *a)
{
    static char refused[APP_MSG];   // the last click's refusal, until the next click or another project
    static char refused_dir[APP_PATH];
    if (strcmp(refused_dir, a->dir) != 0) refused[0] = '\0';
    const ImGuiID beside = panels_limits_dock();
    if (beside != 0) ImGui::SetNextWindowDockID(beside, ImGuiCond_FirstUseEver);   // a layout kept from before this window: a tab beside Limits
    if (ImGui::Begin(PANEL_DRIVE)) {
        const bool busy = car_race_busy(&a->race);
        ImGui::BeginDisabled(busy);
        car_combo(a);
        ImGui::SameLine();
        if (ImGui::Button("Test drive")) {
            refused[0] = '\0';
            if (!app_test_drive(a)) snprintf(refused, sizeof refused, "%s", a->status);
            snprintf(refused_dir, sizeof refused_dir, "%s", a->dir);
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!busy);
        if (ImGui::Button("Stop")) app_race_stop(a);
        ImGui::EndDisabled();
        ImGui::SameLine();
        help_marker("Test drive builds the track, installs it into a scratch player folder (yours and its mods are not touched) and runs the game on it in its "
                    "window: a time trial with the car picked here. A track with red problems, or too short for a stage, is not run: Problems says why. "
                    "Close the game's window or press Stop to end it.");
        state_line(a, refused);
        ImGui::BeginDisabled(busy);
        race_look_controls(&a->race_look);
        ImGui::EndDisabled();
        const RaceJob &j = a->race;
        if ((j.log != nullptr && j.log_len > 0) || (j.run.log != nullptr && j.run.log_len > 0)) {
            if (ImGui::CollapsingHeader("The game's log###drivelog")) {   // folded: the state line says how it went
                if (ImGui::BeginChild("drivelog", ImVec2(0, 0), ImGuiChildFlags_Borders)) {
                    if (j.log != nullptr) ImGui::TextUnformatted(j.log, j.log + j.log_len);
                    if (j.run.log != nullptr) ImGui::TextUnformatted(j.run.log, j.run.log + j.run.log_len);
                }
                ImGui::EndChild();
            }
        }
    }
    ImGui::End();
}
