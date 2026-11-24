// The Changes view of the Masks window (dep59 car editor): the masks of the open project against the game's own copy, read only (editor/mask_changes.h), and a button that puts the
// list on the clipboard for the owner to paste.
#include "panels.h"

extern "C" {
#include "mask_changes.h"
}

#include "help_marker.h"

#include <stdio.h>
#include <string.h>

namespace {

struct Compared {
    MaskChanges changes;
    char why[256] = "";
    bool have = false;
    char key_dir[APP_PATH] = "";
    unsigned key_generation = 0;
    int key_refresh = -1;
};

Compared compared;

}   // namespace

void masks_changes_section(App *a)
{
    const ExportCar *oc = car_race_original_of(&a->car);
    if (oc == nullptr || !a->car.has_masks_dir) return;   // only a car of the game has a copy of the game's to be compared with
    static int refresh = 0;
    char project[APP_PATH], game[APP_PATH];
    car_project_masks_dir(&a->car, project, sizeof project);
    snprintf(game, sizeof game, "%s/car_masks/%s", a->export_src.data, oc->folder);
    if (!compared.have || strcmp(compared.key_dir, a->dir) != 0 || compared.key_generation != a->mask.generation || compared.key_refresh != refresh) {
        compared.have = mask_changes_compare(project, game, &compared.changes, compared.why, sizeof compared.why) != 0;
        snprintf(compared.key_dir, sizeof compared.key_dir, "%s", a->dir);
        compared.key_generation = a->mask.generation;
        compared.key_refresh = refresh;
    }
    ImGui::Spacing();
    if (!ImGui::CollapsingHeader("Changes from the game's masks", ImGuiTreeNodeFlags_DefaultOpen)) return;
    if (ImGui::Button("Re-scan changes vs game repo")) ++refresh;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Read the files again (after your picture editor wrote one)");
    if (!compared.have) {
        ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "No game masks to compare with");   // the state in sight; the folder it looked in is behind the (?)
        ImGui::SameLine();
        help_marker(compared.why);
        return;
    }
    const MaskChanges &c = compared.changes;
    ImGui::SameLine();
    if (ImGui::Button("Copy list of changed files to clipboard")) {
        char text[4096];
        mask_changes_text(&c, oc->folder, text, sizeof text);
        ImGui::SetClipboardText(text);
        snprintf(a->status, sizeof a->status, "copied the list of changed masks (%d changed, %d new, %d missing)", c.changed, c.added, c.missing);
    }
    ImGui::Text("car_masks/%s against the game's: %d changed, %d new, %d missing, %d unchanged", oc->folder, c.changed, c.added, c.missing, c.unchanged);
    for (int i = 0; i < c.count; ++i) {
        const MaskChange &m = c.item[i];
        if (m.kind == MC_UNCHANGED) continue;
        char row[512];
        if (m.kind == MC_CHANGED) {
            int n = snprintf(row, sizeof row, "changed  %s  %d texels", m.file, m.texels);
            if (m.note[0] != '\0') n += snprintf(row + n, sizeof row - (size_t)n, " (%s)", m.note);
            const char *sep = "  ";
            for (int k = 0; k < ME_CLASSES && n < (int)sizeof row - 40; ++k)
                if (m.gained[k] > 0) n += snprintf(row + n, sizeof row - (size_t)n, "%s%s +%d", sep, mask_edit_class(k)->tag, m.gained[k]), sep = ", ";
            for (int k = 0; k < ME_CLASSES && n < (int)sizeof row - 40; ++k)
                if (m.lost[k] > 0) n += snprintf(row + n, sizeof row - (size_t)n, "%s%s -%d", sep, mask_edit_class(k)->tag, m.lost[k]), sep = ", ";
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.78f, 0.35f, 1.0f));   // wrapped: a long row of classes is not cut by a narrow panel
            ImGui::TextWrapped("%s", row);
            ImGui::PopStyleColor();
        } else if (m.kind == MC_NEW) {
            ImGui::TextColored(ImVec4(0.45f, 0.85f, 0.45f, 1.0f), "new      %s", m.file);
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "missing  %s", m.file);
        }
    }
    if (c.changed + c.added + c.missing == 0) ImGui::TextDisabled("Every mask is the game's own.");
}
