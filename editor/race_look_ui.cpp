// The race look's switches (panels.h): shared by the car's Race window and the track's Drive window, so both run the game the same way.
#include "panels.h"

#include "imgui.h"

#include <stdio.h>

// The Race window's and the Drive window's look (T6): Restomod whole by default; four switches for how the car is shown and the render scale. The session's, never the project's.
// Under the buttons, folded by default (the Race tab is short at 960 x 600): its title says what is off, so a folded look is never a hidden one.
void race_look_controls(RaceLook *look_in)
{
    RaceLook &look = *look_in;
    char off[120], title[160];
    car_race_look_off(&look, off, sizeof off);
    const int n = off[0] != '\0' ? snprintf(title, sizeof title, "Look (%s)###racelook", off) : snprintf(title, sizeof title, "Look###racelook");
    if (n < 0 || static_cast<size_t>(n) >= sizeof title) snprintf(title, sizeof title, "Look (some off)###racelook");
    if (!ImGui::CollapsingHeader(title)) return;
    struct { const char *label, *tip; int *on; } sw[4] = {
        { "Reflections", "The sky's reflection on the body (Restomod's envmap); off: the exe's flat body", &look.reflections },
        { "Car colours", "The screen palette's car entries fitted to this car's colours; off: the race's shared palette", &look.car_colours },
        { "Soft shadow", "The soft shadow under the car; off: the exe's hard one", &look.soft_shadow },
        { "Texture filter", "Smoothed texels; off: hard pixels, as the exe draws them", &look.texture_filter } };
    for (int i = 0; i < 4; ++i) {
        bool on = *sw[i].on != 0;
        if (i > 0) ImGui::SameLine();
        if (ImGui::Checkbox(sw[i].label, &on)) *sw[i].on = on ? 1 : 0;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", sw[i].tip);
    }
    ImGui::TextUnformatted("Resolution");
    ImGui::SameLine();
    if (ImGui::RadioButton("1280 x 960", look.scale == 2)) look.scale = 2;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Render scale 2, Restomod's: every detail");
    ImGui::SameLine();
    if (ImGui::RadioButton("640 x 480", look.scale == 1)) look.scale = 1;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Render scale 1: the exe's own resolution");
}
