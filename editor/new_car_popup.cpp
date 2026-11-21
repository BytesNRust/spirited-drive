// See new_car_popup.h.
#include "new_car_popup.h"

extern "C" {
#include "formats/car_piece.h"
}
#include "formats/mod_ini.h"
#include "help_marker.h"
#include "imgui.h"

enum { RACE_CARS = 8 };

void new_car_popup(App *a)
{
    static const char *const NAME = "New car";
    if (a->new_car_dialog && !ImGui::IsPopupOpen(NAME)) ImGui::OpenPopup(NAME);
    if (!ImGui::BeginPopupModal(NAME, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    if (!a->new_car_dialog) {
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }
    ImGui::TextUnformatted("Name");
    ImGui::SameLine();
    if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
    if (ImGui::InputText("##name", a->new_car_name, sizeof a->new_car_name) && !a->new_car_id_typed)
        workspace_id_from_name(a->new_car_name, a->new_car_id, sizeof a->new_car_id, MOD_INI_ID_MAX);
    ImGui::TextUnformatted("Id  ");
    ImGui::SameLine();
    if (ImGui::InputText("##id", a->new_car_id, sizeof a->new_car_id)) a->new_car_id_typed = 1;
    ImGui::SameLine();
    help_marker("The id is the car's folder in the workspace's cars/. It is made from the name; type over it to choose another.");
    ImGui::TextUnformatted("Start as");
    ImGui::SameLine();
    const char *const current = car_piece_word_of_model(a->new_car_template);
    if (ImGui::BeginCombo("##template", current != nullptr ? current : "")) {
        for (int m = 0; m < RACE_CARS; ++m) {
            const char *word = car_piece_word_of_model(m);
            if (word != nullptr && ImGui::Selectable(word, m == a->new_car_template)) a->new_car_template = m;
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    help_marker("The new car starts as a copy of this car: its model, specs (handling), sound, dash and masks. Change them afterwards in the car's windows.");
    if (a->new_car_error[0] != '\0') ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "%s", a->new_car_error);
    ImGui::Spacing();
    const bool create = ImGui::Button("Create") || ImGui::IsKeyPressed(ImGuiKey_Enter, false);
    ImGui::SameLine();
    const bool cancel = ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape, false);
    if (create && app_new_car_named(a, a->new_car_name, a->new_car_id, car_piece_word_of_model(a->new_car_template))) a->new_car_dialog = 0;
    if (cancel) a->new_car_dialog = 0, a->new_car_error[0] = '\0';
    if (!a->new_car_dialog) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}
