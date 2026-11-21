#include "panels.h"

#include "app.h"
#include "imgui.h"

// The in-game photo in a window of its own, as large as the person makes it: the same picture the Drive window shows (the project's masks, Preview's colour),
// to check a mask or a dash again and again at the size the game draws it.
void photo_view_panel(App *a, ImTextureID photo)
{
    if (!a->photo_view_open) return;
    if (a->race.photo == nullptr || photo == 0 || a->race.photo_w <= 0 || a->race.photo_h <= 0) {   // no picture (yet): nothing to show, the window stays asked for
        return;
    }
    const float pw = static_cast<float>(a->race.photo_w), ph = static_cast<float>(a->race.photo_h);
    const ImVec2 screen = ImGui::GetMainViewport()->WorkSize;   // first size: the picture in its proportions, as big as 80% of the screen leaves room for
    float w0 = screen.x * 0.8f;
    if (w0 * ph / pw > screen.y * 0.8f - 40.0f) w0 = (screen.y * 0.8f - 40.0f) * pw / ph;
    ImGui::SetNextWindowSize(ImVec2(w0, w0 * ph / pw + 40.0f), ImGuiCond_FirstUseEver);
    bool open = true;
    if (ImGui::Begin("In-game photo###PhotoView", &open, ImGuiWindowFlags_NoDocking)) {
        const ImVec2 room = ImGui::GetContentRegionAvail();
        float w = room.x;
        if (w * ph / pw > room.y) w = room.y * pw / ph;   // the whole picture, in its own proportions
        if (w > 1.0f) ImGui::Image(photo, ImVec2(w, w * ph / pw));
    }
    ImGui::End();
    if (!open) a->photo_view_open = 0;
}
