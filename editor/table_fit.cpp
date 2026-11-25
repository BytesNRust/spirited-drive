// The Track light panel's tables (table_fit.h)
#include "table_fit.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "ui_probe.h"

#include <stdio.h>
#include <string.h>

namespace {

enum { FIT_COLUMNS = 16, FIT_TABLES = 4, BORDER_HIT = 6 };

struct Report {
    char id[32];
    char label[FIT_COLUMNS][24];
    float width[FIT_COLUMNS], need[FIT_COLUMNS];
    int count, frame;
};
Report g_report[FIT_TABLES];
int g_columns = 0;
char g_id[32] = "";
float g_need[FIT_COLUMNS];
char g_labels[FIT_COLUMNS][24];

Report *report_for(const char *id)
{
    Report *free_slot = nullptr;
    const int frame = ImGui::GetFrameCount();
    for (int i = 0; i < FIT_TABLES; ++i) {
        if (strcmp(g_report[i].id, id) == 0) return &g_report[i];
        if (free_slot == nullptr && (g_report[i].id[0] == '\0' || g_report[i].frame < frame - 2)) free_slot = &g_report[i];
    }
    if (free_slot != nullptr) {
        memset(free_slot, 0, sizeof *free_slot);
        snprintf(free_slot->id, sizeof free_slot->id, "%s", id);
    }
    return free_slot;
}

}   // namespace

float table_fit_cell()
{
    const float w = ImGui::GetContentRegionAvail().x;
    return w > 1.0f ? w : 1.0f;
}

bool table_fit_begin(const char *id, const FitColumn *columns, int count, float height)
{
    if (count > FIT_COLUMNS) count = FIT_COLUMNS;
    const ImGuiStyle &style = ImGui::GetStyle();
    float sum = 0.0f;
    for (int i = 0; i < count; ++i) {
        const float header = ImGui::CalcTextSize(columns[i].label).x + 1.0f;   // a header is never cut: its text is the least
        g_need[i] = columns[i].need > header ? columns[i].need : header;
        snprintf(g_labels[i], sizeof g_labels[i], "%s", columns[i].label);
        sum += g_need[i];
    }
    g_columns = count;
    snprintf(g_id, sizeof g_id, "%s", id);
    const float least = sum + static_cast<float>(count) * (2.0f * style.CellPadding.x + 1.0f);   // the cells' padding and the borders are not in a column's width
    const float room = ImGui::GetContentRegionAvail().x - 1.0f;
    const float inner = room - 3.0f > least ? room - 3.0f : least;   // all of the panel's width, or what the columns need and the table scrolls across
    if (!ImGui::BeginTable(id, count, ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollX, ImVec2(0.0f, height), inner)) return false;
    for (int i = 0; i < count; ++i) ImGui::TableSetupColumn(columns[i].label, ImGuiTableColumnFlags_WidthStretch, g_need[i]);
    return true;
}

// a drag of a border can take all of the next column's width: after the frame's layout a column that is under its least is given it back from the columns that have more than theirs,
// in proportion to what they have to spare (the weights are what is kept in the layout file); with no room to spare, the widths stay as they are
static void hold_least(ImGuiTable *table)
{
    float width[FIT_COLUMNS], weight_sum = 0.0f, deficit = 0.0f, spare = 0.0f, total = 0.0f;
    const int n = g_columns < table->ColumnsCount ? g_columns : table->ColumnsCount;
    for (int i = 0; i < n; ++i) {
        if (table->Columns[i].Flags & ImGuiTableColumnFlags_WidthFixed) return;   // only stretch columns have weights
        width[i] = table->Columns[i].WidthGiven;
        weight_sum += table->Columns[i].StretchWeight;
        total += width[i];
        if (width[i] < g_need[i]) deficit += g_need[i] - width[i];
        else spare += width[i] - g_need[i];
    }
    if (deficit <= 0.01f || spare < deficit || total <= 0.0f) return;
    for (int i = 0; i < n; ++i) {
        const float w = width[i] < g_need[i] ? g_need[i] : width[i] - deficit * (width[i] - g_need[i]) / spare;
        table->Columns[i].StretchWeight = w / total * weight_sum;
    }
    table->IsSettingsDirty = true;
    ImGui::MarkIniSettingsDirty();
}

void table_fit_end()
{
    ImGuiTable *table = ImGui::GetCurrentTable();
    Report *r = table != nullptr ? report_for(g_id) : nullptr;
    if (r != nullptr) {
        r->count = g_columns;
        r->frame = ImGui::GetFrameCount();
        for (int i = 0; i < g_columns && i < table->ColumnsCount; ++i) {
            r->width[i] = table->Columns[i].WidthGiven;
            r->need[i] = g_need[i];
            snprintf(r->label[i], sizeof r->label[i], "%s", g_labels[i]);
            if (i + 1 < g_columns) {   // where a drag of the header's border starts (the input script's ui-span on it)
                char name[40];
                snprintf(name, sizeof name, "%s_border%d", g_id, i);
                ui_probe_rect(name, table->Columns[i].MaxX - BORDER_HIT / 2.0f, table->OuterRect.Min.y, static_cast<float>(BORDER_HIT), ImGui::GetFrameHeight());
            }
        }
    }
    if (table != nullptr) hold_least(table);
    ImGui::EndTable();
}

extern "C" void table_fit_report(FILE *out)
{
    for (int i = 0; i < FIT_TABLES; ++i) {
        const Report &r = g_report[i];
        if (r.id[0] == '\0') continue;
        for (int c = 0; c < r.count; ++c) fprintf(out, "fit %s %s width %.1f need %.1f\n", r.id, r.label[c][0] != '\0' ? r.label[c] : "(button)", r.width[c], r.need[c]);
    }
}
