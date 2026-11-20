// The editor's side panels (panels.h).
#include "panels.h"

#include "imgui.h"
#include <SDL3/SDL.h>

#include <float.h>
#include <math.h>
#include <stdio.h>

extern "C" {
#include "formats/track_gen.h"
#include "interact.h"
}

const char *const PANEL_POINTS = "Points", *const PANEL_LIMITS = "Limits", *const PANEL_PROBLEMS = "Problems", *const PANEL_NODE = "Node", *const PANEL_PROJECT = "Project", *const PANEL_TRACK = "Track";

namespace {

const ImVec4 BAD_TEXT(1.0f, 0.35f, 0.35f, 1.0f), WARN_TEXT(1.0f, 0.69f, 0.13f, 1.0f), OK_TEXT(0.47f, 0.86f, 0.55f, 1.0f);
const float POINT_COL_NUMBER = 28, POINT_COL_DIST = 50, POINT_COL_TURN = 54;   // the Points table's fixed columns: #, dist m, turn ("R 12.3°")
const int POINTS_MIN_ROWS = 4;   // the Points table shows at least these rows, whatever the Road key block below takes
const int PROBLEM_ROWS = 200;   // the nodes listed in Problems; the count says how many more

}   // namespace

static ImGuiID points_dock = 0;   // the dock node Points is in, where a layout kept from before the Node window puts it as a tab

void points_panel(App *a)
{
    static int shown = -2;   // the selection the table last scrolled to
    if (ImGui::Begin(PANEL_POINTS)) {
        points_dock = ImGui::GetWindowDockID();
        if (ImGui::Button("Add (N)")) app_add_point(a);
        ImGui::SameLine();
        ImGui::BeginDisabled(a->selected < 0);
        if (ImGui::Button("Insert (I)")) app_insert_point(a);
        ImGui::SameLine();
        if (ImGui::Button("Remove (Del)")) app_remove_point(a);
        ImGui::EndDisabled();
        ImGui::TextDisabled("%d points", a->trace.count);
        const bool scroll_to_selected = a->selected != shown;
        // the table takes what the Road key block below leaves, but never fewer than POINTS_MIN_ROWS rows: in a short window the panel scrolls instead
        const ImGuiStyle &st = ImGui::GetStyle();
        const float row = ImGui::GetFrameHeight() + 2 * st.CellPadding.y;
        const float least = (POINTS_MIN_ROWS + 1) * row + st.ScrollbarSize + 2;   // the rows, the header, a sideways scroll bar, the borders
        const float room = ImGui::GetContentRegionAvail().y - road_key_panel_height();
        // the columns never narrower than a value ("-1234.5") or their header: a narrow panel scrolls the table sideways, a wide one stretches them
        const float value = ImGui::CalcTextSize("-1234.5").x, inner = POINT_COL_NUMBER + 3 * value + POINT_COL_DIST + POINT_COL_TURN + 12 * st.CellPadding.x + 8;
        const float wide = ImGui::GetContentRegionAvail().x;
        if (ImGui::BeginTable("points", 6, ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders,
                              ImVec2(0, room > least ? room : least), wide > inner ? wide : inner)) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, POINT_COL_NUMBER);
            ImGui::TableSetupColumn("x (m)");
            ImGui::TableSetupColumn("z (m)");
            ImGui::TableSetupColumn("w (m)");   // the road's width: short, so a narrow panel does not cut it
            ImGui::TableSetupColumn("dist m", ImGuiTableColumnFlags_WidthFixed, POINT_COL_DIST);
            ImGui::TableSetupColumn("turn", ImGuiTableColumnFlags_WidthFixed, POINT_COL_TURN);
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
                    // the road's width at the point: the key's own, or (dimmed) what the keys before and after make it; typing or dragging sets a key, right-click clears it
                    ImGui::TableSetColumnIndex(3);
                    const int key = track_trace_key_find(a->trace.keys, a->trace.key_count, i);
                    const bool keyed = key >= 0 && (a->trace.keys[key].has & TRACE_KEY_WIDTH);
                    double metres = keyed ? a->trace.keys[key].width_m
                                          : (a->road_made && a->point_width != NULL && i < a->point_count ? a->point_width[i] : project_road_metres(a->trace.width));
                    const double road_lo = TRACE_ROAD_MIN_M, road_hi = TRACE_ROAD_MAX_M;
                    if (!keyed) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (ImGui::DragScalar("##w", ImGuiDataType_Double, &metres, 0.25f, &road_lo, &road_hi, "%.2f", ImGuiSliderFlags_AlwaysClamp)) {
                        a->selected = shown = i;
                        app_set_point_width(a, i, metres);
                    }
                    if (!keyed) ImGui::PopStyleColor();
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip(keyed ? "A width set on this point. Right-click to let it inherit again." : "Inherited from the points before. Drag or type to set this point's width.");
                    if (keyed && ImGui::BeginPopupContextItem("##clear")) {
                        if (ImGui::MenuItem("Inherit again (clear this width)")) app_clear_point_width(a, i);
                        ImGui::EndPopup();
                    }
                    // read-only: the distance along the curve from the start, and the turn at the node nearest the point
                    ImGui::TableSetColumnIndex(4);
                    if (a->road_made && a->point_dist != NULL && i < a->point_count) ImGui::Text("%.0f", a->point_dist[i]);
                    else ImGui::TextDisabled("-");
                    ImGui::TableSetColumnIndex(5);
                    if (a->road_made && a->point_turn != NULL && i < a->point_count) {
                        char turn[16];
                        track_trace_turn_text(a->point_turn[i], turn, sizeof turn);
                        ImGui::TextUnformatted(turn);
                    } else {
                        ImGui::TextDisabled("-");
                    }
                    ImGui::PopID();
                }
            }
            ImGui::EndTable();
        }
        road_key_panel(a);
    }
    ImGui::End();
}

static ImGuiID limits_dock = 0;   // the dock node Limits is in, where a layout kept from before the Problems window puts it as a tab

void limits_panel(App *a)
{
    if (ImGui::Begin(PANEL_LIMITS)) {
        limits_dock = ImGui::GetWindowDockID();
        const int nodes = app_node_count(a);
        char text[48];
        snprintf(text, sizeof text, "%d / %d nodes", nodes, (int)TRACK_GEN_MAX_NODES);
        ImGui::ProgressBar((float)nodes / (float)TRACK_GEN_MAX_NODES, ImVec2(-FLT_MIN, 0), text);
        ImGui::Text("length %.0f m (%.2f km)", TRACE_METRES(a->length), TRACE_METRES(a->length) / 1000.0);
        if (a->road_text[0] != '\0') ImGui::TextColored(BAD_TEXT, "%s", a->road_text);   // the road is not made: why
        else if (a->summary_count == 0 && !a->stage_short) ImGui::TextColored(OK_TEXT, "limits: all good");
        if (a->road_made && a->stage_short) {   // the road as a whole: too short for a stage's race
            ImGui::PushStyleColor(ImGuiCol_Text, BAD_TEXT);
            ImGui::TextWrapped("%s", a->stage_text);
            ImGui::PopStyleColor();
        }
        // a line per rule some node breaks, red first: the nodes themselves are in Problems
        for (int i = 0; a->road_made && i < a->summary_count; ++i)
            ImGui::TextColored(a->summary[i].red ? BAD_TEXT : WARN_TEXT, "%s", a->summary[i].text);
    }
    ImGui::End();
}

void problems_panel(App *a)
{
    if (limits_dock != 0) ImGui::SetNextWindowDockID(limits_dock, ImGuiCond_FirstUseEver);
    if (ImGui::Begin(PANEL_PROBLEMS)) {
        if (a->road_made && a->stage_short) {   // the road as a whole first: a stage this short has no race
            ImGui::PushStyleColor(ImGuiCol_Text, BAD_TEXT);
            ImGui::TextWrapped("%s", a->stage_text);
            ImGui::PopStyleColor();
        }
        if (!a->road_made || (a->problem_count == 0 && !a->stage_short)) ImGui::TextDisabled("No problems");
        // red rows first, then amber, each by node (track_trace_problems); a click centres the Viewport on the node
        for (int i = 0; i < a->problem_count && i < PROBLEM_ROWS; ++i) {
            const TraceProblem &p = a->problems[i];
            char label[TRACE_PROBLEM_TEXT];
            track_trace_problem_text(&p, label, sizeof label);
            ImGui::PushStyleColor(ImGuiCol_Text, p.red ? BAD_TEXT : WARN_TEXT);
            ImGui::PushID(p.node);
            if (ImGui::Selectable(label)) {   // the node is shown in Node and the Viewport goes to it
                app_select_node(a, p.node);
                a->view.cx = a->nodes[p.node].position[0] / 65536.0;
                a->view.cz = a->nodes[p.node].position[2] / 65536.0;
            }
            ImGui::PopID();
            ImGui::PopStyleColor();
        }
        if (a->problem_count > PROBLEM_ROWS) ImGui::TextDisabled("and %d more", a->problem_count - PROBLEM_ROWS);
    }
    ImGui::End();
}

void node_panel(App *a)
{
    if (points_dock != 0) ImGui::SetNextWindowDockID(points_dock, ImGuiCond_FirstUseEver);
    if (ImGui::Begin(PANEL_NODE)) {
        app_recompute(a);   // a point added or removed in Points this frame: the road and its figures first
        app_sync_selection(a);
        const int n = a->node;
        if (!a->road_made || n < 0 || n >= a->node_count) {
            ImGui::TextDisabled("Select a node");
        } else {
            char turn[16];
            track_trace_turn_text(track_trace_signed_turn(a->nodes, n), turn, sizeof turn);
            ImGui::Text("node %d of %d", n, a->node_count - 1);
            ImGui::Text("x %.1f m   z %.1f m", TRACE_METRES(a->nodes[n].position[0] / 65536.0), TRACE_METRES(a->nodes[n].position[2] / 65536.0));
            ImGui::Text("distance from the start: %.1f m", a->node_dist != NULL ? a->node_dist[n] : 0.0);
            ImGui::Text("turn: %s", turn);
            ImGui::Text("road width: %.2f m", a->nodes[n].width / TRACE_WIDTH_PER_METRE);
            ImGui::Text("shoulder surface: left %s, right %s", a->nodes[n].side[TRACK_GEN_LEFT].tarmac ? "tarmac" : "verge",
                        a->nodes[n].side[TRACK_GEN_RIGHT].tarmac ? "tarmac" : "verge");
            ImGui::Text("to the previous node: %.2f m", track_trace_step(a->nodes, n));
            if (n + 1 < a->node_count) ImGui::Text("to the next node: %.2f m", track_trace_step(a->nodes, n + 1));
            else ImGui::TextDisabled("to the next node: -");
            for (int i = 0; i < a->problem_count; ++i) {   // the limits it breaks, worded as in Problems
                if (a->problems[i].node != n) continue;
                char text[TRACE_PROBLEM_TEXT];
                track_trace_problem_text(&a->problems[i], text, sizeof text);
                ImGui::PushStyleColor(ImGuiCol_Text, a->problems[i].red ? BAD_TEXT : WARN_TEXT);
                ImGui::TextWrapped("%s", text);
                ImGui::PopStyleColor();
            }
        }
    }
    ImGui::End();
}

static ImGuiID project_dock = 0;   // the dock node Project is in, where a layout kept from before the Track window puts it as a tab

// the road's width in metres, tarmac edge to edge; the trace keeps the byte, a whole one: a quarter of a metre
void track_panel(App *a)
{
    if (project_dock != 0) ImGui::SetNextWindowDockID(project_dock, ImGuiCond_FirstUseEver);
    if (ImGui::Begin(PANEL_TRACK)) {
        double road = project_road_metres(a->trace.width);
        const double lo = project_road_metres(PROJECT_WIDTH_MIN), hi = project_road_metres(PROJECT_WIDTH_MAX);
        ImGui::TextUnformatted("Road width (m)");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::DragScalar("##road", ImGuiDataType_Double, &road, 0.25f, &lo, &hi, "%.2f m", ImGuiSliderFlags_AlwaysClamp)) {
            const double width = round(project_width_from_road(road));
            if (width != a->trace.width) {
                a->trace.width = width;
                a->dirty = 1;
            }
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", PROJECT_ROAD_TIP);
        // the verge's material, mod.ini's edge=: dirt is not offered, its third surface keeps the off-road flag of the race before (race_sim.c KEEP(surfaces)),
        // so it drives differently by what was raced first; a project.ini that already says dirt keeps showing it
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Verge");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##edge", a->project.edge)) {
            for (int i = 0; PROJECT_EDGES[i] != NULL; ++i) {
                const bool is_dirt = strcmp(PROJECT_EDGES[i], "dirt") == 0;
                const bool current = strcmp(a->project.edge, PROJECT_EDGES[i]) == 0;
                if (is_dirt && !current) continue;
                if (ImGui::Selectable(PROJECT_EDGES[i], current) && !current) {
                    snprintf(a->project.edge, sizeof a->project.edge, "%s", PROJECT_EDGES[i]);
                }
            }
            ImGui::EndCombo();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("What lies off the road: the verge's look, sound and grip in the game.");
    }
    ImGui::End();
}

// the Locate game dialog's answer: the app (or program) the user picked, none when it was cancelled
static void game_located(void *userdata, const char *const *list, int)
{
    if (list != NULL && list[0] != NULL) app_set_game(static_cast<App *>(userdata), list[0]);
}

void project_panel(App *a)
{
    const bool open = ImGui::Begin(PANEL_PROJECT);
    project_dock = ImGui::GetWindowDockID();   // also while it is a hidden tab
    if (open) {
        ImGui::Text("%s  stem %s", a->project.id, a->project.stem);
        ImGui::TextWrapped("art %s", a->project.art);
        ImGui::TextWrapped("%s", a->dir);
        ImGui::Separator();
        if (ImGui::Button("Save (S)")) app_save(a);
        ImGui::SameLine();
        if (ImGui::Button("Build (B)")) app_build(a);
        ImGui::SameLine();
        ImGui::BeginDisabled(a->zip[0] == '\0');
        if (ImGui::Button("Install (U)")) app_install(a);
        ImGui::EndDisabled();
        if (a->zip[0] != '\0') ImGui::TextWrapped("package %s", a->zip);
        if (a->art_note[0] != '\0') {
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            ImGui::TextWrapped("%s", a->art_note);
            ImGui::PopStyleColor();
        }
        if (app_game_found(a)) {
            ImGui::TextWrapped("game %s", a->game);
        } else {
            ImGui::TextWrapped("game not found");
            if (ImGui::Button("Locate game...")) SDL_ShowOpenFileDialog(game_located, a, NULL, NULL, 0, "/Applications", false);   // its .app, or its program
        }
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
