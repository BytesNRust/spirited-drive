// The Points panel's Add curve and Add straight rows (dep88, app_points.h): the fields, Accept, and the line that says what Accept would make.
// While a row's fields are hovered or edited the Viewport draws it (App.curve.preview).
#include "curve_panel.h"

#include "imgui.h"

extern "C" {
#include "app_points.h"
#include "arc_block.h"
}

namespace {

const ImVec4 BAD_TEXT(1.0f, 0.35f, 0.35f, 1.0f), WARN_TEXT(1.0f, 0.69f, 0.13f, 1.0f);   // as panels.cpp's
const float DEGREE_SPEED = 0.5f, METRE_SPEED = 1.0f, POINT_SPEED = 0.1f;

float field_width(const char *sample) { return ImGui::CalcTextSize(sample).x + 2 * ImGui::GetStyle().FramePadding.x; }

// a field of the row, its tooltip; true while it is hovered or being edited
bool double_field(const char *id, double *v, float speed, double lo, double hi, const char *format, const char *sample, const char *tip)
{
    ImGui::SameLine();
    ImGui::SetNextItemWidth(field_width(sample));
    ImGui::DragScalar(id, ImGuiDataType_Double, v, speed, &lo, &hi, format, ImGuiSliderFlags_AlwaysClamp);
    ImGui::SetItemTooltip("%s", tip);
    return ImGui::IsItemHovered() || ImGui::IsItemActive();
}

bool count_field(const char *id, int *count, int fallback)
{
    ImGui::SameLine();
    ImGui::SetNextItemWidth(field_width("40 pts"));
    int shown = *count > 0 ? *count : fallback;
    if (ImGui::DragInt(id, &shown, POINT_SPEED, 1, ARC_POINTS_MAX, "%d pts", ImGuiSliderFlags_AlwaysClamp)) *count = shown;
    ImGui::SetItemTooltip("How many points the new stretch has; the fewer, the longer and sharper each step. Double-click to type.");
    return ImGui::IsItemHovered() || ImGui::IsItemActive();
}

bool curve_row(App *a)
{
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Add curve");
    bool live = false;
    ImGui::SameLine();
    if (ImGui::SmallButton(a->curve.right ? "R" : "L")) a->curve.right = !a->curve.right;
    ImGui::SetItemTooltip("Turns to the right (R) or left (L) as the view shows it. Click to change.");
    live |= ImGui::IsItemHovered();
    live |= double_field("##deg", &a->curve.degrees, DEGREE_SPEED, 1, ARC_DEGREES_MAX, "%.0f\xc2\xb0", "180\xc2\xb0", "How far the curve turns in all.");
    live |= double_field("##radius", &a->curve.radius_m, METRE_SPEED, 5, 2000, "R %.0f m", "R 2000 m", "The curve's radius.");
    live |= count_field("##count", &a->curve.count, arc_default_count(a->curve.degrees, a->curve.radius_m));
    ImGui::SameLine();
    if (ImGui::Button("Accept##curve")) app_add_curve(a, 0);
    ImGui::SetItemTooltip("Puts the curve in after the last selected point (the road's last when none is), going on the way the road goes there.");
    return live || ImGui::IsItemHovered();
}

bool straight_row(App *a)
{
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Add straight");
    bool live = double_field("##length", &a->curve.length_m, METRE_SPEED, 1, STRAIGHT_LENGTH_MAX_M, "%.0f m", "2000 m", "The straight's length.");
    live |= count_field("##scount", &a->curve.straight_count, straight_default_count(a->curve.length_m));
    ImGui::SameLine();
    if (ImGui::Button("Accept##straight")) app_add_curve(a, 1);
    ImGui::SetItemTooltip("Puts the straight in after the last selected point (the road's last when none is), going on the way the road goes there.");
    return live || ImGui::IsItemHovered();
}

}   // namespace

void curve_rows(App *a)
{
    app_curve_defaults(a);
    const bool curve = curve_row(a), straight = straight_row(a);
    a->curve.preview = curve ? 1 : straight ? 2 : a->curve.preview_hold;
    if (a->curve.preview == 0) return;
    char text[160];
    app_curve_text(a, a->curve.preview == 2, text, sizeof text);
    const int level = a->curve.preview == 1 ? app_curve_level(a) : 0;
    if (level == 0) ImGui::TextDisabled("%s", text);
    else ImGui::TextColored(level == 2 ? BAD_TEXT : WARN_TEXT, "%s", text);
}
