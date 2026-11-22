// The Track light window's comparison (dep98 step 2, part D): beside the window's own photo, the photo of the files as they are on disk (Before), or the four times of day at
// the same node in a 2 x 2. The pictures are made by light_compare.h on the poll; this only shows them.
#include "panels.h"

#include "help_marker.h"

#include <float.h>
#include <stdio.h>

const char *const PANEL_LIGHT_COMPARE = "Light compare";

namespace {

// one picture as wide as the cell leaves room for; `caption` above it
void picture(const char *caption, const LightPhotoJob &job, ImTextureID tex, float width, bool running, bool missing)
{
    ImGui::TextUnformatted(caption);
    if (running) {
        ImGui::SameLine();
        ImGui::TextDisabled("refreshing...");
    }
    if (tex == 0 || job.rgba == nullptr) {
        ImGui::TextDisabled(running ? "" : missing ? "no light file in the folder" : "no picture yet");
        return;
    }
    const float pw = static_cast<float>(job.w), ph = static_cast<float>(job.h);
    const float w = width < pw ? width : pw;
    if (w > 8.0f) ImGui::Image(tex, ImVec2(w, w * ph / pw));
}

}   // namespace

void light_compare_panel(App *a, const ImTextureID photos[LIGHT_COMPARE_SLOTS])
{
    LightUi &ui = a->light;
    LightCompare &c = ui.compare;
    if (ImGui::Begin(PANEL_LIGHT_COMPARE)) {
        int mode = c.mode;
        static const char *const NAMES[LIGHT_COMPARE_MODES] = { "Off", "Before", "Four times" };
        for (int m = 0; m < LIGHT_COMPARE_MODES; ++m) {
            if (m > 0) ImGui::SameLine();
            if (ImGui::RadioButton(NAMES[m], mode == m)) mode = m;
        }
        if (mode != c.mode) app_light_set_compare(a, mode);
        ImGui::SameLine();
        help_marker("Before: the photo of the files as they are on disk at the photo's node, beside the photo of your changes. Four times: dawn, day, dusk and night at the same "
                    "node; all four show your changes (the file holds every time). One game run at a time, after the photo of your changes.");
        if (c.why[0] != '\0') {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.35f, 0.35f, 1.0f));
            ImGui::TextWrapped("%s", c.why);
            ImGui::PopStyleColor();
        }
        const float room = ImGui::GetContentRegionAvail().x;
        if (c.mode == LIGHT_COMPARE_BEFORE) {
            char caption[64];
            snprintf(caption, sizeof caption, "Before: saved %s, %s, node %u", ui.session.track, light_ui_time_name(ui.time), c.job[0].node);
            picture(caption, c.job[0], photos[0], room, c.running == 0, c.missing[0] != 0);
        } else if (c.mode == LIGHT_COMPARE_FOUR) {
            const float cell = (room - ImGui::GetStyle().ItemSpacing.x) / 2.0f;
            for (int t = 0; t < LIGHT_COMPARE_SLOTS; ++t) {
                char caption[64];
                const bool editing = light_compare_letter(t) == light_ui_time_letter(ui.time);
                snprintf(caption, sizeof caption, "%s%s", light_ui_time_name(t), editing ? " (shown)" : "");
                ImGui::BeginGroup();
                picture(caption, c.job[t], photos[t], cell, c.running == t, c.missing[t] != 0);
                ImGui::EndGroup();
                if (t % 2 == 0) ImGui::SameLine();
            }
        }
    }
    ImGui::End();
}
