#include "slope_panel.h"

#include "imgui.h"

extern "C" {
#include "app_slope.h"
#include "slope_block.h"
}

namespace {

const ImVec4 BAD_TEXT(1.0f, 0.35f, 0.35f, 1.0f);   // as panels.cpp's

float field_width(const char *sample) { return ImGui::CalcTextSize(sample).x + 2 * ImGui::GetStyle().FramePadding.x; }

// a field of the row with its tooltip; true while it is hovered or being edited
bool field(const char *id, double *v, double speed, double lo, double hi, const char *format, const char *sample, const char *tip)
{
    ImGui::SameLine();
    ImGui::SetNextItemWidth(field_width(sample));
    ImGui::DragScalar(id, ImGuiDataType_Double, v, speed, &lo, &hi, format, ImGuiSliderFlags_AlwaysClamp);
    ImGui::SetItemTooltip("%s", tip);
    return ImGui::IsItemHovered() || ImGui::IsItemActive();
}

}   // namespace

void slope_rows(App *a)
{
    app_slope_defaults(a);
    ImGui::SetNextItemOpen(false, ImGuiCond_Once);
    const bool open = ImGui::CollapsingHeader("Slope###slopeblock");
    ImGui::SetItemTooltip("Makes the selected run of points climb or fall by a grade or a rise, the grade eased in from the road's own and out to the one after the run.");
    if (!open) {
        a->slope.preview = a->slope.preview_hold;
        return;
    }
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Slope");
    bool live = false;
    ImGui::SameLine();
    if (ImGui::SmallButton(a->slope.by_rise ? "Rise" : "Grade")) a->slope.by_rise = !a->slope.by_rise;
    ImGui::SetItemTooltip("Set the slope by its grade or by the height it makes in all. Click to change.");
    live |= ImGui::IsItemHovered();
    if (a->slope.by_rise)
        live |= field("##rise", &a->slope.rise_m, 0.1, -SLOPE_RISE_MAX_M, SLOPE_RISE_MAX_M, "%.1f m", "-500.0 m", "How far the run's last point ends above its first (below when negative).");
    else
        live |= field("##grade", &a->slope.grade_pct, 0.1, -SLOPE_GRADE_MAX_PCT, SLOPE_GRADE_MAX_PCT, "%.1f %%", "-100.0 %", "The grade between the easings (a climb positive).");
    live |= field("##ein", &a->slope.ease_in_m, 1.0, 0, 2000, "in %.0f m", "in 2000 m", "How far the grade takes to come from the road's own to this one.");
    live |= field("##eout", &a->slope.ease_out_m, 1.0, 0, 2000, "out %.0f m", "out 2000 m", "How far the grade takes to go back to the one the road has after the run.");
    ImGui::SameLine();
    bool keep = a->slope.keep_after != 0;
    if (ImGui::Checkbox("Keep the road##keep", &keep)) a->slope.keep_after = keep;
    ImGui::SetItemTooltip("On: the points after the run move up or down with it, so no cliff appears. Off: they stay where they are.");
    live |= ImGui::IsItemHovered();
    ImGui::SameLine();
    if (ImGui::Button("Accept##slope")) app_slope_accept(a);
    ImGui::SetItemTooltip("Writes the heights of the run: one step to undo.");
    live |= ImGui::IsItemHovered();
    a->slope.preview = live ? 1 : a->slope.preview_hold;
    SlopePlan plan;
    app_slope_plan(a, &plan);
    if (plan.ok) ImGui::TextDisabled("%s", plan.text);
    else ImGui::TextColored(BAD_TEXT, "%s", plan.text);
    app_slope_plan_free(&plan);
}
