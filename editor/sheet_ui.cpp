// The Race window's sheet row (dep98): a button that takes a sheet of photos, Save sheet and the progress.
#include "sheet_ui.h"

#include "help_marker.h"

extern "C" {
#include "app_sheet.h"
}

void sheet_row(App *a, bool busy)
{
    ImGui::BeginDisabled(busy);
    if (ImGui::Button("Sheet")) app_sheet_start(a);
    ImGui::SetItemTooltip("Day and Night photos of this car on the track chosen above, one picture with a label under each.");
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(busy || a->sheet.count == 0);
    if (ImGui::Button("Save sheet")) app_sheet_save(a);
    ImGui::SetItemTooltip("Writes the sheet as a PNG in this project's sheets folder and shows it in the Finder.");
    ImGui::EndDisabled();
    if (a->sheet_active) {
        ImGui::SameLine();
        ImGui::TextDisabled("%d of %d", sheet_finished_count(&a->sheet) + 1 > a->sheet.count ? a->sheet.count : sheet_finished_count(&a->sheet) + 1, a->sheet.count);
    }
}

ImVec2 sheet_size(const App *a)
{
    const int cols = a->sheet.count < a->sheet.cols ? a->sheet.count : a->sheet.cols, rows = (a->sheet.count + a->sheet.cols - 1) / a->sheet.cols;
    return ImVec2(static_cast<float>(cols * SHEET_CELL_W), static_cast<float>(rows * (SHEET_CELL_H + SHEET_LABEL_H)));
}
