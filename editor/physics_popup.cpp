// See physics_popup.h.
#include "physics_popup.h"

#include "car_physics_from.h"
extern "C" {
#include "formats/car_piece.h"
}
#include "help_marker.h"
#include "imgui.h"

static const char *word_at(int i)
{
    const char *words[CAR_PHYSICS_WORDS];
    const int n = car_physics_words(words, CAR_PHYSICS_WORDS);
    return i >= 0 && i < n ? words[i] : "";
}

void physics_row(App *a)
{
    const char *words[CAR_PHYSICS_WORDS];
    const int n = car_physics_words(words, CAR_PHYSICS_WORDS);
    ImGui::BeginDisabled(!app_car_is_kit(a) || car_piece_stem_is_original(a->car.info.stem));
    ImGui::TextUnformatted("Physics from");
    ImGui::SameLine(130);
    ImGui::SetNextItemWidth(140);
    if (ImGui::BeginCombo("##physics", word_at(a->physics_pick))) {
        for (int i = 0; i < n; ++i)
            if (ImGui::Selectable(words[i], i == a->physics_pick)) a->physics_pick = i;
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::Button("Copy...")) a->physics_dialog = 1;
    ImGui::EndDisabled();
    ImGui::SameLine();
    help_marker("Copies the handling of one of the game's cars into this car: its specs (power, gears, grip, brakes), mass and the AI's speeds. The model's size, the assist words and the collision body stay the car's own.");
}

void physics_popup(App *a)
{
    static const char *const NAME = "Physics from";
    if (a->physics_dialog && !ImGui::IsPopupOpen(NAME)) ImGui::OpenPopup(NAME);
    if (!ImGui::BeginPopupModal(NAME, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    if (!a->physics_dialog) {
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }
    ImGui::Text("Copy the physics of the %s into this car?", word_at(a->physics_pick));
    ImGui::Spacing();
    ImGui::BulletText("This overwrites the car's specs/specs.tsv and specs/pdn.tsv.");
    ImGui::BulletText("The car's replay fingerprint changes.");
    ImGui::Spacing();
    const bool copy = ImGui::Button("Copy");
    ImGui::SameLine();
    const bool cancel = ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape, false);
    if (copy) app_physics_from(a, word_at(a->physics_pick));
    if (copy || cancel) a->physics_dialog = 0;
    if (!a->physics_dialog) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}
