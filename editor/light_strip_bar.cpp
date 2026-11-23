#include "light_strip_bar.h"

#include "light_strip.h"
#include "ui_probe.h"

#include "imgui.h"

#include <math.h>
#include <stdio.h>

bool light_strip_bar(const char *id, unsigned *mask)
{
    ImGui::PushID(id);
    bool changed = false;
    struct Shortcut { const char *label; unsigned mask; };
    static const Shortcut SHORTCUTS[] = { { "All", LIGHT_STRIP_ALL }, { "Left", LIGHT_STRIP_LEFT }, { "Right", LIGHT_STRIP_RIGHT }, { "Road", LIGHT_STRIP_ROAD } };
    for (const Shortcut &s : SHORTCUTS) {
        if (ImGui::SmallButton(s.label)) *mask = s.mask, changed = true;
        ui_probe(s.label);
        ImGui::SameLine();
    }
    char text[32];
    const bool one_line = light_strip_text(*mask, text, sizeof text) != 0;
    ImGui::TextDisabled("%s", one_line ? text : "not one line");
    const float cell_w = fmaxf(ImGui::GetContentRegionAvail().x / LIGHT_STRIP_POINTS, 1.0f), cell_h = ImGui::GetFrameHeight() * 1.3f;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##bar", ImVec2(cell_w * LIGHT_STRIP_POINTS, cell_h));
    ui_probe("bar");
    ImGuiStorage *storage = ImGui::GetStateStorage();   // the drag's first cell is this bar's own: several bars are on screen
    const ImGuiID bar = ImGui::GetItemID();
    int anchor = storage->GetInt(bar, -1);
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    float at = cell_w > 0.0f ? (mouse.x - origin.x) / cell_w : 0.0f;   // no mouse is a huge negative: clamp in floats before the cast
    at = !(at > 0.0f) ? 0.0f : at > static_cast<float>(LIGHT_STRIP_POINTS - 1) ? static_cast<float>(LIGHT_STRIP_POINTS - 1) : at;
    const int cell = static_cast<int>(at);
    if (ImGui::IsItemActivated()) anchor = cell, storage->SetInt(bar, anchor);
    if (ImGui::IsItemActive() && anchor >= 0) {
        const unsigned picked = light_strip_span(anchor, cell);
        if (picked != *mask) *mask = picked, changed = true;
    }
    if (ImGui::IsItemDeactivated()) storage->SetInt(bar, -1);
    ImDrawList *draw = ImGui::GetWindowDrawList();
    for (int c = 0; c < LIGHT_STRIP_POINTS; c++) {
        const int point = light_strip_cell_point(c);
        const bool on = (*mask >> point & 1u) != 0;
        const ImVec2 a(origin.x + c * cell_w + 1, origin.y), b(origin.x + (c + 1) * cell_w - 1, origin.y + cell_h);
        draw->AddRectFilled(a, b, ImGui::GetColorU32(on ? ImGuiCol_ButtonActive : ImGuiCol_FrameBg));
        char label[8];
        snprintf(label, sizeof label, point == 0 ? "C" : "%d", point);
        const ImVec2 size = ImGui::CalcTextSize(label);
        draw->AddText(ImVec2((a.x + b.x - size.x) * 0.5f, (a.y + b.y - size.y) * 0.5f), ImGui::GetColorU32(ImGuiCol_Text), label);
    }
    ImGui::PopID();
    return changed;
}
