// The editor's side panels (panels.h).
#include "panels.h"

#include "imgui.h"

#include <float.h>
#include <math.h>
#include <stdio.h>

extern "C" {
#include "formats/track_gen.h"
#include "interact.h"
}

const char *const PANEL_POINTS = "Points", *const PANEL_LIMITS = "Limits", *const PANEL_PROJECT = "Project";

namespace {

const ImVec4 BAD_TEXT(1.0f, 0.35f, 0.35f, 1.0f), WARN_TEXT(1.0f, 0.69f, 0.13f, 1.0f), OK_TEXT(0.47f, 0.86f, 0.55f, 1.0f);
const int LIMIT_ROWS = 200;   // the nodes listed in Limits; the count says how many more

// the control point nearest to the road's node `node` (the road's start is the first point)
int nearest_point(const App &a, int node)
{
    const double nx = a.nodes[node].position[0] / 65536.0, nz = a.nodes[node].position[2] / 65536.0;
    int best = 0;
    double best_d = HUGE_VAL;
    for (int i = 0; i < a.trace.count; ++i) {
        const double d = hypot(a.trace.points[i].x - nx, a.trace.points[i].z - nz);
        if (d < best_d) {
            best_d = d;
            best = i;
        }
    }
    return best;
}

}   // namespace

void points_panel(App *a)
{
    static int shown = -2;   // the selection the table last scrolled to
    if (ImGui::Begin(PANEL_POINTS)) {
        if (ImGui::Button("Add (N)")) app_add_point(a);
        ImGui::SameLine();
        ImGui::BeginDisabled(a->selected < 0);
        if (ImGui::Button("Insert (I)")) app_insert_point(a);
        ImGui::SameLine();
        if (ImGui::Button("Remove (Del)")) app_remove_point(a);
        ImGui::EndDisabled();
        ImGui::TextDisabled("%d points", a->trace.count);
        const bool scroll_to_selected = a->selected != shown;
        if (ImGui::BeginTable("points", 3, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 48);
            ImGui::TableSetupColumn("x");
            ImGui::TableSetupColumn("z");
            ImGui::TableHeadersRow();
            shown = a->selected;   // the table is drawn: it has scrolled to what was selected from elsewhere
            ImGuiListClipper clipper;
            clipper.Begin(a->trace.count);
            if (scroll_to_selected && a->selected >= 0) clipper.IncludeItemByIndex(a->selected);
            while (clipper.Step()) {
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                    ImGui::PushID(i);
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    char label[16];
                    snprintf(label, sizeof label, "%d", i);
                    if (ImGui::Selectable(label, i == a->selected)) a->selected = shown = i;   // chosen here: the table does not scroll to it
                    if (scroll_to_selected && i == a->selected) ImGui::SetScrollHereY();
                    TracePoint &p = a->trace.points[i];
                    const double lo = -TRACE_COORD_MAX, hi = TRACE_COORD_MAX;
                    ImGui::TableSetColumnIndex(1);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (ImGui::DragScalar("##x", ImGuiDataType_Double, &p.x, 0.5f, &lo, &hi, "%.1f", ImGuiSliderFlags_AlwaysClamp)) {
                        a->selected = shown = i;
                        a->dirty = 1;
                    }
                    ImGui::TableSetColumnIndex(2);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (ImGui::DragScalar("##z", ImGuiDataType_Double, &p.z, 0.5f, &lo, &hi, "%.1f", ImGuiSliderFlags_AlwaysClamp)) {
                        a->selected = shown = i;
                        a->dirty = 1;
                    }
                    ImGui::PopID();
                }
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();
}

void limits_panel(App *a)
{
    if (ImGui::Begin(PANEL_LIMITS)) {
        const int nodes = app_node_count(a);
        char text[48];
        snprintf(text, sizeof text, "%d / %d nodes", nodes, (int)TRACK_GEN_MAX_NODES);
        ImGui::ProgressBar((float)nodes / (float)TRACK_GEN_MAX_NODES, ImVec2(-FLT_MIN, 0), text);
        ImGui::Text("length %.0f u", a->length);
        if (a->road_text[0] != '\0') ImGui::TextColored(BAD_TEXT, "%s", a->road_text);
        else if (a->warn_text[0] != '\0') ImGui::TextColored(WARN_TEXT, "%s", a->warn_text);
        else ImGui::TextColored(OK_TEXT, "limits: all good");
        if (a->road_made) {
            ImGui::Text("turn > %g degrees: %d nodes", TRACE_TURN_MAX_DEGREES, a->turn_nodes);
            ImGui::Text("radius < %g units: %d nodes", TRACE_RADIUS_MIN, a->radius_nodes);
            // amber only when some node is over the limit: a count of none is plain text, as the red ones are
            ImGui::PushStyleColor(ImGuiCol_Text, a->warn_turn_nodes > 0 ? WARN_TEXT : ImGui::GetStyleColorVec4(ImGuiCol_Text));
            ImGui::Text("warning: turn > %g degrees: %d nodes", TRACE_TURN_WARN_DEGREES, a->warn_turn_nodes);
            ImGui::PopStyleColor();
            ImGui::PushStyleColor(ImGuiCol_Text, a->warn_radius_nodes > 0 ? WARN_TEXT : ImGui::GetStyleColorVec4(ImGuiCol_Text));
            ImGui::Text("warning: radius < %g units: %d nodes", TRACE_RADIUS_WARN, a->warn_radius_nodes);
            ImGui::PopStyleColor();
            int listed = 0, broken = 0;
            for (int i = 0; i < a->node_count; ++i) broken += a->flags[i] != 0;
            if (broken > 0 && ImGui::BeginChild("breaks", ImVec2(0, 0), ImGuiChildFlags_Borders)) {
                // the red nodes first (they are what stops Build), then the amber ones, so a long road's many warnings never push a red row out of the list
                for (int pass = 0; pass < 2; ++pass) {
                    for (int i = 0; i < a->node_count && listed < LIMIT_ROWS; ++i) {
                        const bool red = (a->flags[i] & (TRACE_FLAG_TURN | TRACE_FLAG_RADIUS)) != 0;
                        if (a->flags[i] == 0 || red != (pass == 0)) continue;
                        ++listed;
                        char label[64];
                        snprintf(label, sizeof label, "node %d: %s%s%s", i, (a->flags[i] & (TRACE_FLAG_TURN | TRACE_FLAG_TURN_WARN)) ? "turn " : "",
                                 (a->flags[i] & (TRACE_FLAG_RADIUS | TRACE_FLAG_RADIUS_WARN)) ? "radius" : "", red ? "" : " (warning)");
                        ImGui::PushStyleColor(ImGuiCol_Text, red ? BAD_TEXT : WARN_TEXT);
                        if (ImGui::Selectable(label)) {   // the point nearest to it is selected and the view goes there
                            a->selected = nearest_point(*a, i);
                            a->view.cx = a->nodes[i].position[0] / 65536.0;
                            a->view.cz = a->nodes[i].position[2] / 65536.0;
                        }
                        ImGui::PopStyleColor();
                    }
                }
                if (broken > listed) ImGui::TextDisabled("and %d more", broken - listed);
            }
            if (broken > 0) ImGui::EndChild();
        }
    }
    ImGui::End();
}

void project_panel(App *a)
{
    if (ImGui::Begin(PANEL_PROJECT)) {
        ImGui::Text("%s  stem %s", a->project.id, a->project.stem);
        ImGui::TextWrapped("art %s", a->project.art);
        ImGui::TextWrapped("%s", a->dir);
        ImGui::Separator();
        double width = a->trace.width;
        const double lo = PROJECT_WIDTH_MIN, hi = PROJECT_WIDTH_MAX;
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::DragScalar("##width", ImGuiDataType_Double, &width, 0.5f, &lo, &hi, "verge width %.1f", ImGuiSliderFlags_AlwaysClamp)) {
            a->trace.width = width;
            a->dirty = 1;
        }
        ImGui::Separator();
        if (ImGui::Button("Save (S)")) app_save(a);
        ImGui::SameLine();
        if (ImGui::Button("Build (B)")) app_build(a);
        ImGui::SameLine();
        ImGui::BeginDisabled(a->zip[0] == '\0');
        if (ImGui::Button("Install (U)")) app_install(a);
        ImGui::EndDisabled();
        if (a->zip[0] != '\0') ImGui::TextWrapped("package %s", a->zip);
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextWrapped("installs into %s", a->user[0] != '\0' ? a->user : "the game's player folder");
        ImGui::PopStyleColor();
        ImGui::TextWrapped("%s", a->status);
        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));   // the help wraps with the panel, never cut at its edge
        ImGui::TextWrapped("drag LMB  pan RMB/MMB  zoom wheel");
        ImGui::TextWrapped("arrows nudge  F fit  N add  I insert");
        ImGui::TextWrapped("Del remove  S save  B build  U install");
        ImGui::PopStyleColor();
    }
    ImGui::End();
}
