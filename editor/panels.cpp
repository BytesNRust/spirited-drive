// The editor's side panels (panels.h).
#include "panels.h"
#include "help_marker.h"
#include "path_elide.h"

#include "imgui.h"
#include <SDL3/SDL.h>

#include <float.h>
#include <math.h>
#include <stdio.h>

extern "C" {
#include "app_select.h"
#include "formats/track_gen.h"
#include "interact.h"
#include "shortcut_keys.h"
}

const char *const PANEL_POINTS = "Points", *const PANEL_LIMITS = "Limits", *const PANEL_PROBLEMS = "Problems", *const PANEL_NODE = "Node", *const PANEL_PROJECT = "Project", *const PANEL_TRACK = "Track",
                  *const PANEL_ROAD_KEY = "Road profile";   // the window's title, and the start of its header ("Road profile at point 3")

namespace {

const ImVec4 BAD_TEXT(1.0f, 0.35f, 0.35f, 1.0f), WARN_TEXT(1.0f, 0.69f, 0.13f, 1.0f), OK_TEXT(0.47f, 0.86f, 0.55f, 1.0f);
const float POINT_COL_NUMBER = 28;   // the Points table's # column
// the Points table's other columns: header, and the widest value it usually shows (a column is never narrower than either)
const int POINT_COLS = 6;
const char *const POINT_HEADERS[POINT_COLS] = { "x (m)", "z (m)", "w (m)", "bank (\xc2\xb0)", "dist m", "turn" };
const char *const POINT_SAMPLES[POINT_COLS] = { "-1234.5", "-1234.5", "30.00", "-60.0", "12345", "R 12.3\xc2\xb0" };
const float ROTATE_DRAG_SPEED = 0.25f;   // degrees a pixel of drag on Rotate points
const int POINTS_MIN_ROWS = 4;   // the Points table shows at least these rows: in a short window the panel scrolls instead
const int PATH_MAX_SHOWN = 1024;   // a path line's buffer (APP_PATH and more: elided, it is shorter)
const int PROBLEM_ROWS = 200;   // the nodes listed in Problems; the count says how many more

}   // namespace

// `label` and a path on one line: a path too long for the panel keeps its start and end with "..." between (path_elide), the whole of it on hover
static void path_line(const char *label, const char *path, bool dim)
{
    const float room = ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(label).x;
    const float glyph = ImGui::CalcTextSize("0").x;   // the font's width per character (Studio's is monospaced)
    char shown[PATH_MAX_SHOWN];
    path_elide(path, glyph > 0 ? static_cast<int>(room / glyph) : static_cast<int>(strlen(path)), shown, sizeof shown);
    if (dim) ImGui::TextDisabled("%s%s", label, shown);
    else ImGui::Text("%s%s", label, shown);
    if (strcmp(shown, path) != 0 && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", path);
}

static ImGuiID points_dock = 0;   // the dock node Points is in, where a layout kept from before the Node window puts it as a tab

// the Points table's columns' least widths (need) and their sum with the # column, the cells' padding and the borders: the table's least inner width
static float points_table_inner(float need[POINT_COLS])
{
    const ImGuiStyle &st = ImGui::GetStyle();
    float inner = POINT_COL_NUMBER + 2 * st.CellPadding.x + 8;
    for (int c = 0; c < POINT_COLS; ++c) {
        need[c] = fmaxf(ImGui::CalcTextSize(POINT_HEADERS[c]).x, ImGui::CalcTextSize(POINT_SAMPLES[c]).x);
        inner += need[c] + 2 * st.CellPadding.x;
    }
    return inner;
}

float points_panel_width()
{
    float need[POINT_COLS];
    const ImGuiStyle &st = ImGui::GetStyle();
    return points_table_inner(need) + 2 * st.ScrollbarSize + 2 * st.WindowPadding.x;   // the table's scroll bar and the panel's own (a short window scrolls it)
}

// The road's bank at point i (column 4): the key's own, or (dimmed) what the keys around make it; amber over TRACE_BANK_WARN_DEG, red over the most the
// generator builds. Typing or dragging sets a key, right-click clears it.
static void bank_cell(App *a, int i, int *shown)
{
    ImGui::TableSetColumnIndex(4);
    const int key = track_trace_key_find(a->trace.keys, a->trace.key_count, i);
    const bool keyed = key >= 0 && (a->trace.keys[key].has & TRACE_KEY_BANK);
    double degrees = keyed ? a->trace.keys[key].bank_deg : (a->road_made && a->point_bank != NULL && i < a->point_count ? a->point_bank[i] : 0.0);
    const double lo = -TRACE_BANK_INPUT_MAX_DEG, hi = TRACE_BANK_INPUT_MAX_DEG;
    const double size = fabs(degrees);
    const bool red = size > TRACE_BANK_MAX_DEG, amber = !red && size > TRACE_BANK_WARN_DEG;
    const ImVec4 colour = red ? BAD_TEXT : amber ? WARN_TEXT : ImGui::GetStyleColorVec4(keyed ? ImGuiCol_Text : ImGuiCol_TextDisabled);
    ImGui::PushStyleColor(ImGuiCol_Text, (red || amber) && !keyed ? ImVec4(colour.x, colour.y, colour.z, 0.6f) : colour);
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::DragScalar("##b", ImGuiDataType_Double, &degrees, 0.25f, &lo, &hi, "%.1f", ImGuiSliderFlags_AlwaysClamp)) {
        a->selected = *shown = i;
        app_set_point_bank(a, i, degrees);
    }
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) {
        const char *what = keyed ? "A bank set on this point. Right-click to let it inherit again." : "Inherited from the points around. Drag or type to bank the road here.";
        const char *limit = red ? "Over %g\xc2\xb0: the track can't be built." : amber ? "Over %g\xc2\xb0: it may not look right in the game." : "";
        ImGui::BeginTooltip();
        ImGui::TextUnformatted(what);
        ImGui::TextUnformatted("Positive banks the road for a right-hand curve.");
        if (limit[0] != '\0') ImGui::TextColored(red ? BAD_TEXT : WARN_TEXT, limit, red ? TRACE_BANK_MAX_DEG : TRACE_BANK_WARN_DEG);
        ImGui::EndTooltip();
    }
    if (keyed && ImGui::BeginPopupContextItem("##clearbank")) {
        if (ImGui::MenuItem("Inherit again (clear this bank)")) app_clear_point_bank(a, i);
        ImGui::EndPopup();
    }
}

// with several points selected: «Rotate points [ 0.0° ] about point N», a field that rotates them about the first while it is dragged or typed,
// back to 0 when let go (the points stay where it left them: one undo step)
static void rotate_row(App *a)
{
    static float degrees = 0;
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Rotate points");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::CalcTextSize("-180.0\xc2\xb0").x + 2 * ImGui::GetStyle().FramePadding.x);
    if (ImGui::DragFloat("##rotate", &degrees, ROTATE_DRAG_SPEED, -180.0f, 180.0f, "%.1f\xc2\xb0", ImGuiSliderFlags_AlwaysClamp)) app_sel_rotate_live(a, degrees);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Rotates the selected points about the first of them. The view does not turn. Keys: [ and ] (Shift: 5\xc2\xb0).");
    if (ImGui::IsItemDeactivated()) {
        app_sel_rotate_end(a);
        degrees = 0;
    }
    ImGui::SameLine();
    ImGui::Text("about point %d", sel_first(&a->sel));
}

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
        if (ImGui::Button("Remove (Del)")) app_remove_selection(a);
        ImGui::EndDisabled();
        const int picked = app_sel_count(a);
        if (picked > 1) ImGui::TextDisabled("%d points, %d selected", a->trace.count, picked);
        else ImGui::TextDisabled("%d points", a->trace.count);
        if (picked > 1) rotate_row(a);
        const bool scroll_to_selected = a->selected != shown;
        // the table takes the rest of the panel, but never fewer than POINTS_MIN_ROWS rows
        const ImGuiStyle &st = ImGui::GetStyle();
        const float row = ImGui::GetFrameHeight() + 2 * st.CellPadding.y;
        const float least = (POINTS_MIN_ROWS + 1) * row + st.ScrollbarSize + 2;   // the rows, the header, a sideways scroll bar, the borders
        const float room = ImGui::GetContentRegionAvail().y;
        // the columns never narrower than their header or a value: a narrow panel scrolls the table sideways, a wide one stretches them (each by its need,
        // so none gets less than it). The vertical scroll bar's width is left out, or the stretched table would scroll sideways by that much.
        float need[POINT_COLS];
        const float inner = points_table_inner(need);
        const float wide = ImGui::GetContentRegionAvail().x - st.ScrollbarSize;
        if (ImGui::BeginTable("points", 7, ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders,
                              ImVec2(0, room > least ? room : least), wide > inner ? wide : inner)) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, POINT_COL_NUMBER);
            for (int c = 0; c < POINT_COLS; ++c) ImGui::TableSetupColumn(POINT_HEADERS[c], ImGuiTableColumnFlags_WidthStretch, need[c]);   // x, z, w, bank, dist m, turn
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
                    if (ImGui::Selectable(label, sel_has(&a->sel, i))) {   // as a click in the Viewport: Shift a range, Cmd one more or one less
                        const ImGuiIO &io = ImGui::GetIO();
                        app_sel_click(a, i, io.KeyShift ? SEL_RANGE : io.KeyCtrl ? SEL_TOGGLE : SEL_ONE);
                        shown = a->selected;   // chosen here: the table does not scroll to it
                    }
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
                    const int lanes = app_point_lanes(a, i);
                    const bool by_lanes = lanes == TRACK_GEN_LANES_1_2 || lanes == TRACK_GEN_LANES_2_1;   // a lane stretch sets the width: the cell is locked
                    double metres = keyed ? a->trace.keys[key].width_m
                                          : (a->road_made && a->point_width != NULL && i < a->point_count ? a->point_width[i] : project_road_metres(a->trace.width));
                    if (by_lanes) metres = 3 * TRACK_GEN_LANE / 8.0;   // three lanes of 5 m
                    const double road_lo = TRACE_ROAD_MIN_M, road_hi = TRACE_ROAD_MAX_M;
                    if (!keyed) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::BeginDisabled(by_lanes);
                    if (ImGui::DragScalar("##w", ImGuiDataType_Double, &metres, 0.25f, &road_lo, &road_hi, "%.2f", ImGuiSliderFlags_AlwaysClamp)) {
                        a->selected = shown = i;
                        app_set_point_width(a, i, metres);
                    }
                    ImGui::EndDisabled();
                    if (!keyed) ImGui::PopStyleColor();
                    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                        if (by_lanes) ImGui::SetTooltip("Set by the lanes (%d+%d, 5 m each). Change the lanes in Road profile to set a width here.", (lanes >> 4) & 0xf, lanes & 0xf);
                        else ImGui::SetTooltip("%s", keyed ? "A width set on this point. Right-click to let it inherit again." : "Inherited from the points before. Drag or type to set this point's width.");
                    }
                    if (keyed && ImGui::BeginPopupContextItem("##clear")) {
                        if (ImGui::MenuItem("Inherit again (clear this width)")) app_clear_point_width(a, i);
                        ImGui::EndPopup();
                    }
                    bank_cell(a, i, &shown);
                    // read-only: the distance along the curve from the start, and the turn at the node nearest the point
                    ImGui::TableSetColumnIndex(5);
                    if (a->road_made && a->point_dist != NULL && i < a->point_count) ImGui::Text("%.0f", a->point_dist[i]);
                    else ImGui::TextDisabled("-");
                    ImGui::TableSetColumnIndex(6);
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
    }
    ImGui::End();
}

static ImGuiID limits_dock = 0;   // the dock node Limits is in, where a layout kept from before the Problems window puts it as a tab

ImGuiID panels_limits_dock(void)
{
    return limits_dock;
}

void limits_panel(App *a)
{
    if (ImGui::Begin(PANEL_LIMITS)) {
        limits_dock = ImGui::GetWindowDockID();
        const int nodes = app_node_count(a);
        char text[48];
        snprintf(text, sizeof text, "%d / %d nodes", nodes, (int)TRACK_GEN_MAX_NODES);
        ImGui::ProgressBar((float)nodes / (float)TRACK_GEN_MAX_NODES, ImVec2(-FLT_MIN, 0), text);
        ImGui::Text("length %.0f m (%.2f km)", TRACE_METRES(a->length), TRACE_METRES(a->length) / 1000.0);
        const char *refusal = app_road_refusal(a);   // the road not made, or the generator refuses it: Build's words
        if (refusal != NULL) {
            ImGui::PushStyleColor(ImGuiCol_Text, BAD_TEXT);
            ImGui::TextWrapped("%s", refusal);
            ImGui::PopStyleColor();
        } else if (a->summary_count == 0 && !a->stage_short) {
            ImGui::TextColored(OK_TEXT, "limits: all good");
        }
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
        const char *refusal = app_road_refusal(a);   // what Build would refuse that no red row says: first, in Build's words
        if (refusal != NULL) {
            ImGui::PushStyleColor(ImGuiCol_Text, BAD_TEXT);
            ImGui::TextWrapped("%s", refusal);
            ImGui::PopStyleColor();
        }
        if (a->road_made && a->stage_short) {   // the road as a whole first: a stage this short has no race
            ImGui::PushStyleColor(ImGuiCol_Text, BAD_TEXT);
            ImGui::TextWrapped("%s", a->stage_text);
            ImGui::PopStyleColor();
        }
        char art[APP_MSG];
        const bool art_line = app_tarmac_art_note(a, art, sizeof art);   // tarmac shoulders on an art that is not Studio's current generic: said, nothing changed
        if (art_line) {
            ImGui::PushStyleColor(ImGuiCol_Text, WARN_TEXT);
            ImGui::TextWrapped("%s", art);
            ImGui::PopStyleColor();
        }
        char lanes[APP_MSG];
        const bool lanes_line = app_lane_width_note(a, lanes, sizeof lanes);   // a width key inside a lane stretch: ignored, said
        if (lanes_line) {
            ImGui::PushStyleColor(ImGuiCol_Text, WARN_TEXT);
            ImGui::TextWrapped("%s", lanes);
            ImGui::PopStyleColor();
        }
        char custom[APP_MSG];
        const bool custom_line = app_barrier_custom_note(a, custom, sizeof custom);   // a barrier set apart from the fence: amber, built as asked
        if (custom_line) {
            ImGui::PushStyleColor(ImGuiCol_Text, WARN_TEXT);
            ImGui::TextWrapped("%s", custom);
            ImGui::PopStyleColor();
        }
        if (refusal == NULL && (!a->road_made || (a->problem_count == 0 && !a->stage_short)) && !art_line && !lanes_line && !custom_line) ImGui::TextDisabled("No problems");
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

static ImGuiID node_dock = 0;   // the dock node Node is in, where a layout kept from before the Road key window puts it as a tab

// Node with several points selected: how many, which, and the stretch of road from the first to the last
static void selection_summary(const App *a)
{
    char which[96];
    sel_describe(&a->sel, which, sizeof which);
    ImGui::Text("%d points selected: %s", sel_count(&a->sel), which);
    const int first = sel_first(&a->sel), last = sel_last(&a->sel);
    if (a->road_made && a->point_dist != NULL && last < a->point_count)
        ImGui::Text("stretch: %.0f m along the road (points %d to %d)", a->point_dist[last] - a->point_dist[first], first, last);
}

void node_panel(App *a)
{
    if (points_dock != 0) ImGui::SetNextWindowDockID(points_dock, ImGuiCond_FirstUseEver);
    if (ImGui::Begin(PANEL_NODE)) {
        node_dock = ImGui::GetWindowDockID();
        app_recompute(a);   // a point added or removed in Points this frame: the road and its figures first
        app_sync_selection(a);
        const int n = a->node;
        if (app_sel_count(a) > 1) {
            selection_summary(a);
        } else if (!a->road_made || n < 0 || n >= a->node_count) {
            ImGui::TextDisabled("Select a node");
        } else {
            char turn[16];
            track_trace_turn_text(track_trace_signed_turn(a->nodes, n), turn, sizeof turn);
            ImGui::Text("node %d of %d", n, a->node_count - 1);
            ImGui::Text("x %.1f m   z %.1f m", TRACE_METRES(a->nodes[n].position[0] / 65536.0), TRACE_METRES(a->nodes[n].position[2] / 65536.0));
            ImGui::Text("distance from the start: %.1f m", a->node_dist != NULL ? a->node_dist[n] : 0.0);
            ImGui::Text("turn: %s", turn);
            const double right_half = a->nodes[n].width_r > 0 ? a->nodes[n].width_r : a->nodes[n].width;   // a lane stretch's halves differ (step 4)
            ImGui::Text("road width: %.2f m", (a->nodes[n].width + right_half) / (2 * TRACE_WIDTH_PER_METRE));
            const int lanes = a->nodes[n].lanes != 0 ? a->nodes[n].lanes : TRACK_GEN_LANES_1_1;
            ImGui::Text("lanes: %d+%d", (lanes >> 4) & 0xf, lanes & 0xf);
            {   // the bank, in the limit's colour; R or L for the curve it suits
                const double b = a->nodes[n].bank, size = fabs(b);
                char bank[24];
                if (size < 0.05) snprintf(bank, sizeof bank, "level");
                else snprintf(bank, sizeof bank, "%c %.1f\xc2\xb0", b > 0 ? 'R' : 'L', size);
                if (size > TRACE_BANK_MAX_DEG) ImGui::TextColored(BAD_TEXT, "bank: %s", bank);
                else if (size > TRACE_BANK_WARN_DEG) ImGui::TextColored(WARN_TEXT, "bank: %s", bank);
                else ImGui::Text("bank: %s", bank);
            }
            ImGui::Text("shoulder surface: left %s, right %s", a->nodes[n].side[TRACK_GEN_LEFT].tarmac ? "asphalt" : "off-road",
                        a->nodes[n].side[TRACK_GEN_RIGHT].tarmac ? "asphalt" : "off-road");
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
        ImGui::SameLine();
        char keys[256], save_key[16], undo_key[16], redo_key[24];
        shortcut_label(save_key, sizeof save_key, SHORTCUT_MAC, "S");
        shortcut_label(undo_key, sizeof undo_key, SHORTCUT_MAC, "Z");
        shortcut_label(redo_key, sizeof redo_key, SHORTCUT_MAC, "Shift+Z");
        snprintf(keys, sizeof keys, "Mouse: left drag moves a point, right or middle drag pans, the wheel zooms.\n"
                                    "Keys: arrows nudge, F fit, N add, I insert, Del remove, %s save, %s undo, %s redo, B build, U install.", save_key, undo_key, redo_key);
        help_marker(keys);
        path_line("art ", a->project.art, false);
        path_line("", a->dir, false);
        ImGui::Separator();
        char save[32], save_keys[16];
        shortcut_label(save_keys, sizeof save_keys, SHORTCUT_MAC, "S");
        snprintf(save, sizeof save, "Save (%s)", save_keys);
        if (ImGui::Button(save)) app_save(a);
        ImGui::SameLine();
        ImGui::BeginDisabled(app_install_busy(a) || car_race_installing(&a->race));   // the installer reads the zip Build writes
        if (ImGui::Button("Build (B)")) app_build(a);
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(a->zip[0] == '\0' || app_install_busy(a) || car_race_busy(&a->race));   // nor while a test drive runs
        if (ImGui::Button("Install (U)")) app_install(a);
        ImGui::EndDisabled();
        if (app_install_busy(a)) {   // the game's installer runs in the background: the window goes on (its own line: beside the buttons it runs off at 960 x 600)
            ImGui::TextUnformatted("Installing...");
            ImGui::SameLine();
            if (ImGui::SmallButton("Stop##install")) app_install_stop(a);
        }
        if (a->zip[0] != '\0') path_line("package ", a->zip, false);
        if (a->art_note[0] != '\0') {
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            ImGui::TextWrapped("%s", a->art_note);
            ImGui::PopStyleColor();
        }
        if (app_game_found(a)) {
            path_line("game ", a->game, false);
        } else {
            ImGui::TextWrapped("game not found");
            if (ImGui::Button("Locate game...")) SDL_ShowOpenFileDialog(game_located, a, NULL, NULL, 0, "/Applications", false);   // its .app, or its program
        }
        path_line("installs into ", a->user[0] != '\0' ? a->user : "the game's player folder", true);
        ImGui::TextWrapped("%s", a->status);
    }
    ImGui::End();
}

void road_key_window(App *a)
{
    if (node_dock != 0) ImGui::SetNextWindowDockID(node_dock, ImGuiCond_FirstUseEver);
    if (ImGui::Begin(PANEL_ROAD_KEY, nullptr, ImGuiWindowFlags_NoFocusOnAppearing)) road_key_panel(a);   // it does not take the front from Node
    ImGui::End();
}
