// See physics_popup.h.
#include "physics_popup.h"

#include "car_physics_from.h"
extern "C" {
#include "formats/car_piece.h"
}
#include "help_marker.h"
#include "imgui.h"
#include "ui_probe.h"

#include <ctype.h>
#include <stdio.h>
#include <strings.h>

static const char *word_at(int i)
{
    const char *words[CAR_PHYSICS_WORDS];
    const int n = car_physics_words(words, CAR_PHYSICS_WORDS);
    return i >= 0 && i < n ? words[i] : "";
}

const char *physics_row_text(const App *a, char *buf, size_t cap)
{
    if (app_car_is_original(a)) {   // the game's own car: its own physics, as Sound from names its own sound
        const char *words[CAR_PHYSICS_WORDS];
        const int n = car_physics_words(words, CAR_PHYSICS_WORDS);
        for (int i = 0; i < n; ++i) {
            const char *stem = car_physics_stem(words[i]);
            if (stem != nullptr && strcasecmp(stem, a->car.info.stem) == 0) return words[i];
        }
        size_t k = 0;   // a car with no word of its own (a traffic car): its stem
        for (; k + 1 < cap && a->car.info.stem[k] != '\0'; ++k) buf[k] = (char)tolower((unsigned char)a->car.info.stem[k]);
        buf[k] = '\0';
        return buf;
    }
    return a->physics_chosen ? word_at(a->physics_pick) : "not set";
}

void physics_row(App *a)
{
    const char *words[CAR_PHYSICS_WORDS];
    const int n = car_physics_words(words, CAR_PHYSICS_WORDS);
    const ImGuiStyle &st = ImGui::GetStyle();
    const float copy_w = ImGui::CalcTextSize("Copy...").x + 2 * st.FramePadding.x, marker_w = ImGui::CalcTextSize("(?)").x;
    char stem_word[32];
    ImGui::TableNextRow();   // a row of the card's table, so its box starts where Sound from's does and is as wide
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Physics from");
    ImGui::TableSetColumnIndex(1);
    ImGui::BeginDisabled(!app_car_is_kit(a) || car_piece_stem_is_original(a->car.info.stem));
    ImGui::SetNextItemWidth(-(copy_w + marker_w + 2 * st.ItemSpacing.x));
    if (ImGui::BeginCombo("##physics", physics_row_text(a, stem_word, sizeof stem_word))) {
        for (int i = 0; i < n; ++i)
            if (ImGui::Selectable(words[i], a->physics_chosen && i == a->physics_pick)) a->physics_pick = i, a->physics_chosen = 1;
        ImGui::EndCombo();
    }
    ui_probe("physics_box");
    ImGui::SameLine();
    ImGui::BeginDisabled(!a->physics_chosen);
    if (ImGui::Button("Copy...")) a->physics_dialog = 1;
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    ImGui::SameLine();
    help_marker("Copies the handling of one of the game's cars into this car: its specs (power, gears, grip, brakes), mass and the AI's speeds. The model's size, the assist words and the collision body stay the car's own. "
                "A car of the game shows its own physics here, and nothing is copied into it.");
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
