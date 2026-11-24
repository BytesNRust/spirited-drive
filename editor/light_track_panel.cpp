// The Track light window's lists on the new one-file-a-track format (light_track.h): the Sun of the time, the Tunnels (the track's tunnel facts with the time's look of each), the
// Lamps, and the Retouches (Brightness and Tone, each over a node range and a strip of the row). Every edit goes through the model, which checks the whole file with the game's
// rules: a refused value shows the rule in the window's error line and changes nothing. Called from light_panel.cpp.
#include "panels.h"

#include "help_marker.h"
#include "light_strip_bar.h"
#include "ui_probe.h"
#include "light_strip.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {

void refused(App *a, const char *why)
{
    snprintf(a->light.error, sizeof a->light.error, "%s", why);
}

LightTrack &file_of(App *a)
{
    return a->light.session.file;
}

int time_of(const App *a)
{
    return a->light.time;   // 0..3 = LT_TIME_A..N
}

unsigned last_node(const App *a)
{
    return a->light.session.nodes > 0 ? a->light.session.nodes - 1 : 0;
}

unsigned non_negative(int v)
{
    return static_cast<unsigned>(v < 0 ? 0 : v);
}

// "RRGGBB" to three floats 0..1; false for anything else
bool hex_to_rgb(const char *hex, float rgb[3])
{
    if (strlen(hex) != 6) return false;
    char *end = nullptr;
    const unsigned long v = strtoul(hex, &end, 16);
    if (*end != '\0') return false;
    rgb[0] = static_cast<float>((v >> 16) & 0xff) / 255.0f;
    rgb[1] = static_cast<float>((v >> 8) & 0xff) / 255.0f;
    rgb[2] = static_cast<float>(v & 0xff) / 255.0f;
    return true;
}

void rgb_to_hex(const float rgb[3], char *out, size_t cap)
{
    unsigned c[3];
    for (int k = 0; k < 3; ++k) c[k] = static_cast<unsigned>(rgb[k] * 255.0f + 0.5f) & 0xff;
    snprintf(out, cap, "%02x%02x%02x", c[0], c[1], c[2]);
}

// a colour swatch that opens the picker; true when the colour changed (`hex` then holds six digits)
bool swatch(const char *id, char *hex, size_t cap)
{
    float rgb[3] = { 1, 1, 1 };
    hex_to_rgb(hex, rgb);
    if (!ImGui::ColorEdit3(id, rgb, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_PickerHueWheel)) return false;
    rgb_to_hex(rgb, hex, cap);
    return true;
}

// an optional number: an empty box is "not set" (the value is the default's), a number sets it. The box keeps what is typed while it is being edited. True when the value changed.
bool optional_number(const char *id, int *has, double *value, const char *format, float width)
{
    static char kept[24];        // what is typed in the box being edited (one box at a time is)
    static ImGuiID editing = 0;
    char text[24];
    const ImGuiID me = ImGui::GetID(id);
    if (editing == me) snprintf(text, sizeof text, "%s", kept);
    else if (*has) snprintf(text, sizeof text, format, *value);
    else text[0] = '\0';
    ImGui::SetNextItemWidth(width);
    const bool typed = ImGui::InputText(id, text, sizeof text, ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_AutoSelectAll);
    if (ImGui::IsItemActive()) editing = me, snprintf(kept, sizeof kept, "%s", text);
    else if (editing == me) editing = 0;
    if (!typed) return false;
    if (text[0] == '\0') {
        const bool was = *has != 0;
        *has = 0;
        return was;
    }
    char *end = nullptr;
    const double v = strtod(text, &end);
    if (end == text || *end != '\0') return false;   // "-" or "1." on the way to a number
    if (*has && *value == v) return false;
    *has = 1, *value = v;
    return true;
}

bool node_box(const char *id, unsigned *node, float width)
{
    int v = static_cast<int>(*node);
    ImGui::SetNextItemWidth(width);
    if (!ImGui::InputInt(id, &v, 0, 0)) return false;
    *node = non_negative(v);
    return true;
}

// ---- Sun ---------------------------------------------------------------------------------------------------------------------------------------------------------------------

// the compass: north up, the arrow the way the light comes from (a long one for a low sun)
void sun_dial(const LightTrackSun &s)
{
    const float size = 76.0f;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(size, size));
    ImDrawList *draw = ImGui::GetWindowDrawList();
    const ImVec2 mid(origin.x + size * 0.5f, origin.y + size * 0.5f);
    const float r = size * 0.5f - 3.0f;
    draw->AddCircle(mid, r, ImGui::GetColorU32(ImGuiCol_Border), 32);
    draw->AddText(ImVec2(mid.x - 3.0f, origin.y - 2.0f), ImGui::GetColorU32(ImGuiCol_TextDisabled), "N");
    if (!s.has_azimuth) return;
    const float az = static_cast<float>(s.azimuth) * 0.01745329f;
    const float el = s.has_elevation ? static_cast<float>(s.elevation) * 0.01745329f : 0.35f;
    const float len = r * (0.25f + 0.75f * cosf(el < 0 ? 0.0f : el));
    const ImVec2 tip(mid.x + sinf(az) * len, mid.y - cosf(az) * len);
    draw->AddLine(mid, tip, ImGui::GetColorU32(ImVec4(1.0f, 0.8f, 0.2f, 1.0f)), 2.5f);
    draw->AddCircleFilled(tip, 4.0f, ImGui::GetColorU32(ImVec4(1.0f, 0.8f, 0.2f, 1.0f)));
}

// one of the sun's numbers: a slider while it is set, "Auto" (the generator's own) while not
bool sun_part(const char *label, int *has, double *value, float lo, float hi, const char *format, float auto_value, const char *auto_unit)
{
    char auto_text[32];   // what "auto" is: the generator's own value, as the game takes it at this time ("auto: 90 deg"); auto_unit is a printf text, a percent sign doubled
    char auto_format[40];
    snprintf(auto_text, sizeof auto_text, "%.0f", static_cast<double>(auto_value));
    snprintf(auto_format, sizeof auto_format, "auto: %s %s", auto_text, auto_unit);
    bool changed = false;
    ImGui::PushID(label);
    ui_probe_scope(label);
    bool set = *has != 0;
    const bool ticked = ImGui::Checkbox("##set", &set);
    ui_probe("tick");
    if (ticked) {
        *has = set ? 1 : 0;
        if (set) *value = auto_value;
        changed = true;
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!set);
    float v = static_cast<float>(*value);
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::SliderFloat("##v", &v, lo, hi, set ? format : auto_format)) *value = static_cast<double>(v), changed = true;
    ui_probe("slider");
    ImGui::EndDisabled();
    ui_probe_scope("");
    ImGui::PopID();
    return changed;
}

void sun_rows(App *a)
{
    LightTrack &f = file_of(a);
    char why[LT_WHY];
    LightTrackSun s;
    memset(&s, 0, sizeof s);
    const int existing = light_track_get_sun(&f, time_of(a), &s);
    const bool open = ImGui::CollapsingHeader(time_of(a) == LT_TIME_N ? "Moon###lightsun" : "Sun###lightsun", ImGuiTreeNodeFlags_DefaultOpen);   // folded or not is kept by the window while Studio runs
    ui_probe("sun_header");
    ImGui::SameLine();
    help_marker("The light's direction (compass: north up, the arrow points to where the light comes from), its height above the horizon, and the shadow strength: the light left in "
                "full shadow, in percent (100 is no shadow). A part that is not ticked is the generator's own (the direction from the sky's glow). At night this is the moon, with no shadow "
                "by default. Shadows fall only in a clear race, never in fog, rain, storm or snow. The photo follows every change.");
    if (!open) return;
    bool changed = false;
    sun_dial(s);
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::TextUnformatted("Direction");
    LightTrackSun autos;   // the generator's own sun at this time: shown as "auto" and where a ticked part starts
    light_track_sun_auto(time_of(a), &autos);
    changed = sun_part("azimuth", &s.has_azimuth, &s.azimuth, 0.0f, 360.0f, "%.0f deg", static_cast<float>(autos.azimuth), "deg") || changed;
    ImGui::TextUnformatted("Height");
    changed = sun_part("elevation", &s.has_elevation, &s.elevation, -90.0f, 90.0f, "%.0f deg", static_cast<float>(autos.elevation), "deg") || changed;
    ImGui::TextUnformatted("Shadow strength");
    changed = sun_part("shadow", &s.has_shadow, &s.shadow, 0.0f, 100.0f, "%.0f %%", static_cast<float>(autos.shadow), "%%") || changed;
    ImGui::EndGroup();
    if (changed) {
        if (!s.has_azimuth && !s.has_elevation && !s.has_shadow) {   // everything auto again: the time has no sun line (the generator's own)
            if (existing && !light_track_remove(&f, time_of(a), LT_SUN, 0, why, sizeof why)) refused(a, why);
            else if (existing) app_light_touch(a);
            return;
        }
        if (light_track_set_sun(&f, time_of(a), &s, why, sizeof why)) app_light_touch(a);
        else refused(a, why);
    }
}

// ---- Tunnels -----------------------------------------------------------------------------------------------------------------------------------------------------------------

bool look_is_empty(const LightTrackLook &v)
{
    return !v.has_light && !v.has_colour && !v.has_lamps && !v.has_lamp_colour && !v.has_reach;
}

// writes a look the row edited at `time`: a new line when `index` is -1, a line with nothing left in it is dropped
void put_look(App *a, int time, int index, const LightTrackLook &v)
{
    char why[LT_WHY];
    LightTrack &f = file_of(a);
    if (look_is_empty(v) && v.has_nodes) {
        if (index >= 0 && !light_track_remove(&f, time, LT_LOOK, index, why, sizeof why)) return refused(a, why);
        return app_light_touch(a);
    }
    if (light_track_set_look(&f, time, index, &v, why, sizeof why)) app_light_touch(a);
    else refused(a, why);
}

// a cell of the Tunnels table: the look's value `which` (0 Light %, 1 Lamps %, 2 Reach, 3 Colour) at one time; true when it changed. A hall takes no Light % or Colour.
bool value_cell(LightTrackLook &v, int which, bool hall, bool open_time)
{
    if (open_time) ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(60, 90, 140, 80));
    if (hall && (which == 0 || which == 3)) {
        ImGui::TextDisabled("--");
        return false;
    }
    if (which == 0) return optional_number("##value", &v.has_light, &v.light, "%g", -FLT_MIN);
    if (which == 1) return optional_number("##value", &v.has_lamps, &v.lamps, "%g", -FLT_MIN);
    if (which == 2) return optional_number("##value", &v.has_reach, &v.reach, "%g", -FLT_MIN);
    bool changed = false;
    char hex[8] = "ffffff";
    if (v.has_colour && strlen(v.colour) >= 6) snprintf(hex, sizeof hex, "%.6s", v.colour);
    if (swatch("##value", hex, sizeof hex)) v.has_colour = 1, snprintf(v.colour, sizeof v.colour, "%s", hex), changed = true;
    return changed;
}

// the four value cells of a row (Dawn, Day, Dusk, Night): the look of that time is read, the cell edited, and the look written at that time. `look[t]` is the look's index there (-1: none yet; the row's nodes
// make a new one), `nodes` 0 for the default row.
void value_cells(App *a, const int look[LT_TIMES], bool nodes, unsigned from, unsigned to, bool hall, int first_column)
{
    static const char *const CELL[LT_TIMES] = { "dawn", "day", "dusk", "night" };
    LightTrack &f = file_of(a);
    for (int t = 0; t < LT_TIMES; ++t) {
        LightTrackLook v;
        memset(&v, 0, sizeof v);
        if (look[t] >= 0) light_track_get_look(&f, t, look[t], &v);
        else if (nodes) v.has_nodes = 1, v.from = from, v.to = to;
        ImGui::TableSetColumnIndex(first_column + t);
        ImGui::PushID(t);
        const bool changed = value_cell(v, a->light.tunnel_show, hall, t == time_of(a));
        ui_probe(CELL[t]);
        ImGui::PopID();
        if (changed) put_look(a, t, look[t], v);
    }
}

void tunnel_rows(App *a)
{
    LightTrack &f = file_of(a);
    char why[LT_WHY];
    const bool open = ImGui::CollapsingHeader("Tunnels###lighttunnels", ImGuiTreeNodeFlags_DefaultOpen);
    ui_probe("tunnels_header");
    ImGui::SameLine();
    help_marker("The track's tunnels: where each is (nodes) and what it is (a tunnel, or a hall: a roofed straight lit by its own lamps) are the same at every time. The four columns are Dawn, Day, Dusk and Night: "
                "the value chosen above (Light %: percent of full light inside; Lamps %: the lamps' strength, 100 as the game has them, 0 a tunnel with no lighting, the car does not blink going through it; "
                "Reach: the lamp pools' reach, in nodes; Colour) of that tunnel at that time. An empty cell takes the default row's. A hall takes only Lamps and Reach. Gradient (nodes, 0 to 8) is where the light falls inside each mouth: it is the tunnel's own, one for all four times. To copy a value to another time, "
                "type it there. The time that is open is tinted. A tunnel moved or removed takes its looks of all four times with it.");
    if (!open) return;
    LightTrackLookRow rows[2 * LT_MAX_PER_TIME];
    const int count = light_track_look_rows(&f, rows, 2 * LT_MAX_PER_TIME);
    int default_look[LT_TIMES];
    light_track_default_looks(&f, default_look);
    ImGui::TextUnformatted("Show");
    static const char *const SHOW[4] = { "Light %", "Lamps %", "Reach", "Colour" };
    static const char *const SHOW_PROBE[4] = { "show_light", "show_lamps", "show_reach", "show_colour" };
    for (int k = 0; k < 4; ++k) {
        ImGui::SameLine();
        ImGui::RadioButton(SHOW[k], &a->light.tunnel_show, k);
        ui_probe(SHOW_PROBE[k]);
    }
    int remove_row = -1;
    // the table scrolls across when the panel is narrow; its height is its rows' (a scrolling table would otherwise take what is left of the window)
    const float row_h = ImGui::GetFrameHeight() + 2.0f * ImGui::GetStyle().CellPadding.y;
    const float table_height = row_h * static_cast<float>(count + 2) + ImGui::GetStyle().ScrollbarSize + 2.0f;
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(2.0f, ImGui::GetStyle().CellPadding.y));
    const bool tunnel_table = ImGui::BeginTable("##tunnels", 9, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollX, ImVec2(0.0f, table_height));
    if (!tunnel_table) ImGui::PopStyleVar();
    if (tunnel_table) {
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 52.0f);   // fixed widths: a stretch column holding a -FLT_MIN item gives a NaN content size under UBSan
        ImGui::TableSetupColumn("From", ImGuiTableColumnFlags_WidthFixed, 36.0f);
        ImGui::TableSetupColumn("To", ImGuiTableColumnFlags_WidthFixed, 36.0f);
        ImGui::TableSetupColumn("Kind", ImGuiTableColumnFlags_WidthFixed, 52.0f);
        for (int t = 0; t < LT_TIMES; ++t) ImGui::TableSetupColumn(light_ui_time_name(t), ImGuiTableColumnFlags_WidthFixed, 36.0f);
        ImGui::TableSetupColumn("Gradient", ImGuiTableColumnFlags_WidthFixed, 46.0f);
        ImGui::TableHeadersRow();
        // the default row: the look of every tunnel with no row of its own
        ImGui::PushID("default");
        ui_probe_scope("tunneldefault");
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(1);
        ImGui::TextDisabled("default");
        value_cells(a, default_look, false, 0, 0, false, 4);
        ui_probe_scope("");
        ImGui::PopID();
        for (int r = 0; r < count; ++r) {
            LightTrackTunnel t = { rows[r].from, rows[r].to, rows[r].lit, 0, 0 };
            const bool fact = rows[r].tunnel >= 0;
            if (fact) light_track_get_tunnel(&f, rows[r].tunnel, &t);
            bool tunnel_changed = false;
            char scope[16];
            snprintf(scope, sizeof scope, "tunnel%d", r);
            ImGui::PushID(r);
            ImGui::TableNextRow();
            ui_probe_scope(scope);
            ImGui::TableSetColumnIndex(0);
            if (ImGui::Button("Remove")) remove_row = r;
            ui_probe("remove");
            ImGui::TableSetColumnIndex(1);
            if (fact) tunnel_changed = node_box("##from", &t.from, -FLT_MIN) || tunnel_changed, ui_probe("from");
            else ImGui::Text("%u", t.from);
            ImGui::TableSetColumnIndex(2);
            if (fact) tunnel_changed = node_box("##to", &t.to, -FLT_MIN) || tunnel_changed, ui_probe("to");
            else ImGui::Text("%u", t.to);
            ImGui::TableSetColumnIndex(3);
            if (fact) {
                ImGui::SetNextItemWidth(-FLT_MIN);
                if (ImGui::BeginCombo("##kind", t.lit ? "hall" : "tunnel")) {
                    if (ImGui::Selectable("tunnel", !t.lit) && t.lit) t.lit = 0, tunnel_changed = true;
                    ui_probe("kind_tunnel");
                    if (ImGui::Selectable("hall", t.lit != 0) && !t.lit) t.lit = 1, tunnel_changed = true;
                    ui_probe("kind_hall");
                    ImGui::EndCombo();
                }
                ui_probe("kind");
            } else {
                ImGui::TextDisabled("inside");
            }
            value_cells(a, rows[r].look, true, rows[r].from, rows[r].to, fact && rows[r].lit != 0, 4);
            ImGui::TableSetColumnIndex(8);
            if (rows[r].tunnel >= 0 && !t.lit) {   // the nodes inside each mouth over which the light falls (0: a straight cut); the same at every moment and at both mouths
                int g = t.has_gradient ? t.gradient : LT_DEFAULT_GRADIENT;
                ImGui::SetNextItemWidth(-FLT_MIN);
                if (ImGui::InputInt("##gradient", &g, 0, 0)) t.has_gradient = 1, t.gradient = g < 0 ? 0 : g > LT_MAX_GRADIENT ? LT_MAX_GRADIENT : g, tunnel_changed = true;
                ui_probe("gradient");
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Gradient: the nodes inside each mouth over which the light falls from the open road's to the tunnel's.\n0 is a straight cut. It is the same at every moment and at both mouths.");
            }
            ui_probe_scope("");
            ImGui::PopID();
            if (tunnel_changed) {   // the nodes move with the looks of all four times, all or nothing
                if (light_track_move_tunnel(&f, rows[r].tunnel, &t, why, sizeof why)) app_light_touch(a), a->light.resort = 1;
                else refused(a, why);
            }
        }
        ImGui::EndTable();
        ImGui::PopStyleVar();
    }
    if (remove_row >= 0) {   // a tunnel and its looks of the four times, or a row that is only looks
        const LightTrackLookRow &r = rows[remove_row];
        const int ok = r.tunnel >= 0 ? light_track_remove_tunnel(&f, r.tunnel, why, sizeof why) : light_track_remove_looks(&f, r.from, r.to, why, sizeof why);
        if (ok) app_light_touch(a);
        else refused(a, why);
    }
    const bool add_tunnel = ImGui::Button("Add tunnel");
    ui_probe("add_tunnel");
    if (add_tunnel) {
        const unsigned node = static_cast<unsigned>(a->light.node), last = last_node(a);
        const LightTrackTunnel t = { node, node + 50 < last ? node + 50 : last, 0, 0, 0 };
        if (light_track_set_tunnel(&f, -1, &t, why, sizeof why)) app_light_touch(a);
        else refused(a, why);
    }
}

// drops the i-th line of a kind at the time shown; a refusal shows its reason
void remove_line(App *a, int kind, int i)
{
    char why[LT_WHY];
    if (light_track_remove(&file_of(a), time_of(a), kind, i, why, sizeof why)) app_light_touch(a);
    else refused(a, why);
}

// ---- Lamps -------------------------------------------------------------------------------------------------------------------------------------------------------------------

// the lamp's strip: none (the lamp stands as it did before strips), all, left, right or road; a run of points the file has (pA pB) is shown as it is and kept until another is chosen
bool lamp_strip_combo(LightTrackLamp *l)
{
    struct Choice { const char *label; unsigned mask; };
    static const Choice CHOICES[] = { { "all", LT_ALL_POINTS }, { "left", LT_STRIP_LEFT }, { "right", LT_STRIP_RIGHT }, { "road", LT_STRIP_ROAD } };
    char custom[48] = "";
    const char *shown = "none";
    if (l->has_strip) {
        shown = nullptr;
        for (const Choice &c : CHOICES)
            if (c.mask == (l->mask & LT_ALL_POINTS)) shown = c.label;
        if (shown == nullptr) {
            light_strip_text(l->mask, custom, sizeof custom);
            shown = custom[0] != '\0' ? custom : "points";
        }
    }
    bool changed = false;
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##strip", shown)) {
        if (ImGui::Selectable("none", !l->has_strip) && l->has_strip) l->has_strip = 0, l->mask = 0, changed = true;
        ui_probe("strip_none");
        for (const Choice &c : CHOICES) {
            if (ImGui::Selectable(c.label, l->has_strip && c.mask == (l->mask & LT_ALL_POINTS))) l->has_strip = 1, l->mask = c.mask, changed = true;
            char name[16];
            snprintf(name, sizeof name, "strip_%s", c.label);
            ui_probe(name);
        }
        ImGui::EndCombo();
    }
    ui_probe("strip");
    return changed;
}

// a night file has hundreds of lamps: the list is folded, with its count, and scrolls inside its own box
// ---- Copy to... ----------------------------------------------------------------------------------------------------------------------------------------------------------

// whether line `index` of the open time is one the last Copy to... made or changed (until the file is edited again)
bool copy_marked(const App *a, int kind, int index)
{
    const LightUi &ui = a->light;
    if (ui.copy_mark_kind != kind || ui.copy_mark_edits != ui.file_edits || !((ui.copy_mark_times >> time_of(a)) & 1u)) return false;
    unsigned key[3];
    return light_track_copy_key(&ui.session.file, kind, time_of(a), index, key) && memcmp(key, ui.copy_mark_key, sizeof key) == 0;
}

// a tint over the row just drawn (from `top` to the cursor) when it is a copied line
void copy_tint(const App *a, int kind, int index, ImVec2 top)
{
    if (!copy_marked(a, kind, index)) return;
    const ImVec2 end(top.x + ImGui::GetContentRegionAvail().x, ImGui::GetCursorScreenPos().y);
    ImGui::GetWindowDrawList()->AddRectFilled(top, end, IM_COL32(255, 190, 60, 40));
}

// the "Copy to..." button of a row and its menu: the other times, each with a mark (nothing: the line would be added; "has it": the same line is there; "differs": the same key with another value), then
// All others. The marks are worked out while the menu is open, in memory.
void copy_menu(App *a, int kind, int index)
{
    if (ImGui::SmallButton("Copy to...")) ImGui::OpenPopup("##copyto");
    ui_probe("copy");
    if (!ImGui::BeginPopup("##copyto")) return;
    LightTrackCopyPlan plan;
    char why[LT_WHY];
    const int here = time_of(a);
    const bool planned = light_track_copy_plan(&file_of(a), kind, here, index, 0xfu, &plan, why, sizeof why) != 0;
    for (int t = 0; t < LT_TIMES; ++t) {
        if (t == here) continue;
        const char *name = light_ui_time_name(t);
        if (ImGui::Selectable(name, false, ImGuiSelectableFlags_None, ImVec2(120, 0)) && planned) {
            app_light_copy(a, kind, index, 1u << t, 0);
            ImGui::CloseCurrentPopup();
        }
        char probe[24];
        snprintf(probe, sizeof probe, "copy_%s", name);
        ui_probe(probe);
        if (!planned) continue;
        ImGui::SameLine();
        if (plan.to[t].action == LT_COPY_SAME) ImGui::TextDisabled("has it");
        else if (plan.to[t].action == LT_COPY_CLASH) ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "differs");
    }
    ImGui::Separator();
    if (ImGui::Selectable("All others") && planned) {
        app_light_copy(a, kind, index, 0xfu & ~(1u << here), 0);
        ImGui::CloseCurrentPopup();
    }
    ui_probe("copy_all");
    ImGui::EndPopup();
}

const char *kind_word(int kind) { return kind == LT_BRIGHTNESS ? "brightness" : kind == LT_TONE ? "tone" : "lamp"; }

// the window that asks only when a copy would change a line that is there: Replace changes those and adds the rest, Skip only adds, Cancel touches nothing
void copy_modal(App *a)
{
    LightUi &ui = a->light;
    static const char TITLE[] = "Copy to other moments";
    if (!ui.copy_pending) return;
    if (!ImGui::IsPopupOpen(TITLE)) ImGui::OpenPopup(TITLE);
    if (!ImGui::BeginPopupModal(TITLE, nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) return;
    char key[64];
    light_track_copy_key_text(&file_of(a), ui.copy_plan.kind, ui.copy_plan.from_time, ui.copy_plan.index, key, sizeof key);
    ImGui::Text("Copy %s %s", kind_word(ui.copy_plan.kind), key);
    char mine[64];
    light_track_copy_value_text(&file_of(a), ui.copy_plan.kind, ui.copy_plan.from_time, ui.copy_plan.index, mine, sizeof mine);
    for (int t = 0; t < LT_TIMES; ++t) {
        const LightTrackCopyTime &to = ui.copy_plan.to[t];
        char theirs[64];
        if (to.action == LT_COPY_CLASH && light_track_copy_value_text(&file_of(a), ui.copy_plan.kind, t, to.clash, theirs, sizeof theirs))
            ImGui::Text("  %s: %s  ->  %s", light_ui_time_name(t), theirs, mine);
        else if (to.action == LT_COPY_ADD) ImGui::Text("  %s: will be added", light_ui_time_name(t));
        else if (to.action == LT_COPY_SAME) ImGui::TextDisabled("  %s: has it", light_ui_time_name(t));
    }
    if (ImGui::Button("Replace")) app_light_copy_answer(a, 1), ImGui::CloseCurrentPopup();
    ui_probe("copy_replace");
    ImGui::SameLine();
    if (ImGui::Button("Skip")) app_light_copy_answer(a, 2), ImGui::CloseCurrentPopup();
    ui_probe("copy_skip");
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) app_light_copy_answer(a, 3), ImGui::CloseCurrentPopup();
    ui_probe("copy_cancel");
    ImGui::EndPopup();
}

void lamp_rows(App *a)
{
    LightTrack &f = file_of(a);
    char why[LT_WHY];
    const int count = light_track_count(&f, time_of(a), LT_LAMP);
    char label[48];
    snprintf(label, sizeof label, "Lamps (%d)###lamps", count);
    const bool open = ImGui::CollapsingHeader(label, count > 0 && count <= 24 ? ImGuiTreeNodeFlags_DefaultOpen : 0);
    ui_probe("lamps_header");
    ImGui::SameLine();
    help_marker("A lamp lights the road around its node: its light in percent, its colour (empty: the game's), its radius in nodes (empty: the game's). A lamp is allowed at every time, not "
                "only at night (a neon, a shop window). The game takes 2048 at most. Its strip is the part of the road it stands over: all, left, right or road (none: where it stood before strips). A run of points typed in the file is kept as it is.");
    if (!open) return;
    int remove = -1;
    if (count > 0 && ImGui::BeginChild("##lamplist", ImVec2(0, 220), ImGuiChildFlags_Borders)) {
        if (ImGui::BeginTable("##lamps", 7, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollX)) {
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 58.0f);   // Remove first, and the widths what fits the panel at 1280 px: the last columns (Strip, Remove) were past its edge
            ImGui::TableSetupColumn("Node", ImGuiTableColumnFlags_WidthFixed, 54.0f);
            ImGui::TableSetupColumn("Light %", ImGuiTableColumnFlags_WidthFixed, 54.0f);
            ImGui::TableSetupColumn("Colour", ImGuiTableColumnFlags_WidthFixed, 40.0f);
            ImGui::TableSetupColumn("Radius", ImGuiTableColumnFlags_WidthFixed, 54.0f);
            ImGui::TableSetupColumn("Strip", ImGuiTableColumnFlags_WidthFixed, 84.0f);
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 74.0f);   // Copy to...
            ImGui::TableHeadersRow();
            // a night file has hundreds of lamps and each row is found by its number in the file: only the rows in view are drawn
            ImGuiListClipper clipper;
            clipper.Begin(count, ImGui::GetFrameHeight() + 2.0f * ImGui::GetStyle().CellPadding.y);
            while (clipper.Step())
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                LightTrackLamp l;
                if (!light_track_get_lamp(&f, time_of(a), i, &l)) continue;
                bool changed = false;
                ImGui::PushID(i);
                { char scope[16]; snprintf(scope, sizeof scope, "lamp%d", i); ui_probe_scope(scope); }
                ImGui::TableNextRow();
                if (copy_marked(a, LT_LAMP, i)) ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(255, 190, 60, 60));
                ImGui::TableSetColumnIndex(1);
                changed = node_box("##node", &l.node, -FLT_MIN) || changed;
                ui_probe("node");
                ImGui::TableSetColumnIndex(2);
                {
                    int v = static_cast<int>(l.percent);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (ImGui::InputInt("##pct", &v, 0, 0)) l.percent = non_negative(v), changed = true;
                    ui_probe("pct");
                }
                ImGui::TableSetColumnIndex(3);
                {
                    char hex[8] = "ffffff";
                    if (l.has_colour && strlen(l.colour) >= 6) snprintf(hex, sizeof hex, "%.6s", l.colour);
                    if (swatch("##colour", hex, sizeof hex)) l.has_colour = 1, snprintf(l.colour, sizeof l.colour, "%s", hex), changed = true;
                }
                ImGui::TableSetColumnIndex(4);
                {
                    double r = static_cast<double>(l.radius);
                    int has = l.has_radius;
                    if (optional_number("##radius", &has, &r, "%.0f", -FLT_MIN)) l.has_radius = has, l.radius = has ? non_negative(static_cast<int>(r)) : 0, changed = true;
                }
                ImGui::TableSetColumnIndex(5);
                changed = lamp_strip_combo(&l) || changed;
                ImGui::TableSetColumnIndex(6);
                copy_menu(a, LT_LAMP, i);
                ImGui::TableSetColumnIndex(0);
                if (ImGui::Button("Remove")) remove = i;
                ui_probe("remove");
                ui_probe_scope("");
                ImGui::PopID();
                if (changed) {
                    if (light_track_set_lamp(&f, time_of(a), i, &l, why, sizeof why)) { app_light_touch(a); a->light.resort = 1; }
                    else refused(a, why);
                }
            }
            ImGui::EndTable();
        }
    }
    if (count > 0) ImGui::EndChild();
    if (remove >= 0) remove_line(a, LT_LAMP, remove);
    const bool add_lamp = ImGui::Button("Add lamp");
    ui_probe("add_lamp");
    if (add_lamp) {
        LightTrackLamp l;
        memset(&l, 0, sizeof l);
        l.node = static_cast<unsigned>(a->light.node);
        l.percent = 100;
        if (light_track_set_lamp(&f, time_of(a), -1, &l, why, sizeof why)) app_light_touch(a);
        else refused(a, why);
    }
}

// ---- Retouches ---------------------------------------------------------------------------------------------------------------------------------------------------------------

bool section(const char *title, int count, const char *id)
{
    char label[64];
    snprintf(label, sizeof label, "%s (%d)###%s", title, count, id);
    return ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen);
}

void brightness_rows(App *a)
{
    LightTrack &f = file_of(a);
    char why[LT_WHY];
    const int count = light_track_count(&f, time_of(a), LT_BRIGHTNESS);
    const bool open = section("Brightness", count, "lightbrightness");
    ImGui::SameLine();
    help_marker("Brightness: from node, to node, percent. The light of those nodes is multiplied by the percent (100 is as it is), over the strip of the row you pick: All, Left, Right, "
                "Road, or points dragged on the bar (C is the centre point, then the ten points out to each side). Light is kept per point and blended, so the edges of one strip "
                "are soft. A later line over the same nodes is applied after an earlier one.");
    if (!open) return;
    int remove = -1;
    for (int i = 0; i < count; ++i) {
        LightTrackBrightness b;
        if (!light_track_get_brightness(&f, time_of(a), i, &b)) continue;
        int v[3] = { static_cast<int>(b.from), static_cast<int>(b.to), static_cast<int>(b.percent) };
        bool changed = false;
        ImGui::PushID(i);
        const ImVec2 row_top = ImGui::GetCursorScreenPos();
        { char scope[24]; snprintf(scope, sizeof scope, "%s%d", "brightness", i); ui_probe_scope(scope); }
        ImGui::SetNextItemWidth(70.0f);
        changed = ImGui::InputInt("##from", &v[0], 0, 0) || changed;
        ImGui::SameLine();
        ImGui::SetNextItemWidth(70.0f);
        changed = ImGui::InputInt("##to", &v[1], 0, 0) || changed;
        ImGui::SameLine();
        ImGui::SetNextItemWidth(70.0f);
        changed = ImGui::InputInt("##pct", &v[2], 0, 0) || changed;
        ImGui::SameLine();
        if (ImGui::Button("Remove")) remove = i;
        ui_probe("remove");
        ImGui::SameLine();
        copy_menu(a, LT_BRIGHTNESS, i);
        changed = light_strip_bar("##strip", &b.mask) || changed;
        ui_probe_scope("");
        copy_tint(a, LT_BRIGHTNESS, i, row_top);
        ImGui::Separator();
        ImGui::PopID();
        if (changed) {
            if (b.mask == 0) {   // nothing picked: the strip stays the old one until a point is
                refused(a, "pick at least one point of the row");
                continue;
            }
            b.from = non_negative(v[0]), b.to = non_negative(v[1]), b.percent = non_negative(v[2]);
            if (light_track_set_brightness(&f, time_of(a), i, &b, why, sizeof why)) { app_light_touch(a); a->light.resort = 1; }
            else refused(a, why);
        }
    }
    if (remove >= 0) remove_line(a, LT_BRIGHTNESS, remove);
    const bool add_bright = ImGui::Button("Add brightness");
    ui_probe("add_brightness");
    if (add_bright) {
        const unsigned node = static_cast<unsigned>(a->light.node), last = last_node(a);
        const LightTrackBrightness b = { node, node + 50 < last ? node + 50 : last, 100, LT_ALL_POINTS };
        if (light_track_set_brightness(&f, time_of(a), -1, &b, why, sizeof why)) app_light_touch(a);
        else refused(a, why);
    }
}

void tone_rows(App *a)
{
    LightTrack &f = file_of(a);
    char why[LT_WHY];
    const int count = light_track_count(&f, time_of(a), LT_TONE);
    const bool open = section("Tone", count, "lighttone");
    ImGui::SameLine();
    help_marker("Tone: nodes from..to are seen through a colour at a strength 0 to 100, over the strip of the row you pick (as Brightness). A track has 16 different colour and strength "
                "pairs per time at most, and each is exact: a polygon takes it when all four of its corners have it.");
    if (!open) return;
    int remove = -1;
    for (int i = 0; i < count; ++i) {
        LightTrackTone t;
        if (!light_track_get_tone(&f, time_of(a), i, &t)) continue;
        int v[3] = { static_cast<int>(t.from), static_cast<int>(t.to), static_cast<int>(t.percent) };
        bool changed = false;
        ImGui::PushID(i);
        const ImVec2 row_top = ImGui::GetCursorScreenPos();
        { char scope[24]; snprintf(scope, sizeof scope, "%s%d", "tone", i); ui_probe_scope(scope); }
        ImGui::SetNextItemWidth(70.0f);
        changed = ImGui::InputInt("##from", &v[0], 0, 0) || changed;
        ImGui::SameLine();
        ImGui::SetNextItemWidth(70.0f);
        changed = ImGui::InputInt("##to", &v[1], 0, 0) || changed;
        ImGui::SameLine();
        changed = swatch("##colour", t.colour, sizeof t.colour) || changed;
        ImGui::SameLine();
        ImGui::SetNextItemWidth(70.0f);
        changed = ImGui::InputInt("##pct", &v[2], 0, 0) || changed;
        ImGui::SameLine();
        if (ImGui::Button("Remove")) remove = i;
        ui_probe("remove");
        ImGui::SameLine();
        copy_menu(a, LT_TONE, i);
        changed = light_strip_bar("##strip", &t.mask) || changed;
        ui_probe_scope("");
        copy_tint(a, LT_TONE, i, row_top);
        ImGui::Separator();
        ImGui::PopID();
        if (changed) {
            if (t.mask == 0) {
                refused(a, "pick at least one point of the row");
                continue;
            }
            t.from = non_negative(v[0]), t.to = non_negative(v[1]), t.percent = non_negative(v[2]);
            if (light_track_set_tone(&f, time_of(a), i, &t, why, sizeof why)) { app_light_touch(a); a->light.resort = 1; }
            else refused(a, why);
        }
    }
    if (remove >= 0) remove_line(a, LT_TONE, remove);
    const bool add_tone = ImGui::Button("Add tone");
    ui_probe("add_tone");
    if (add_tone) {
        const unsigned node = static_cast<unsigned>(a->light.node), last = last_node(a);
        LightTrackTone t;
        memset(&t, 0, sizeof t);
        t.from = node, t.to = node + 50 < last ? node + 50 : last, t.mask = LT_ALL_POINTS, t.percent = 50;
        snprintf(t.colour, sizeof t.colour, "ffe080");
        if (light_track_set_tone(&f, time_of(a), -1, &t, why, sizeof why)) app_light_touch(a);
        else refused(a, why);
    }
}

// the lines the window does not edit that the game would refuse, kept as they are
void bad_lines(const App *a)
{
    const LightTrack &f = a->light.session.file;
    const int bad = light_track_first_bad(&f);
    if (bad == 0) return;
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.35f, 0.35f, 1.0f));
    ImGui::TextWrapped("Line %d of the file is one the game would refuse (%s): it is kept as it is, and the game makes this track's light as before until it is fixed.", bad,
                       f.lines[bad - 1].why != nullptr ? f.lines[bad - 1].why : "not well-formed");
    ImGui::PopStyleColor();
}

}   // namespace

// the lists in the order of their first node, once the field being typed in is let go (a row that moves under the cursor would lose it)
void put_in_order(App *a)
{
    if (!a->light.resort || ImGui::IsAnyItemActive()) return;
    a->light.resort = 0;
    LightTrack &f = file_of(a);
    int moved = 0;
    static const int KINDS[] = { LT_TUNNEL, LT_LAMP, LT_BRIGHTNESS, LT_TONE };
    for (const int kind : KINDS) { const int n = light_track_sort(&f, time_of(a), kind); moved += n > 0 ? n : 0; }
    if (moved > 0) app_light_touch(a);
}

void light_track_rows(App *a)
{
    bad_lines(a);
    sun_rows(a);
    ImGui::PushID("tunnels");
    tunnel_rows(a);
    ImGui::PopID();
    ImGui::PushID("lamps");
    lamp_rows(a);
    ImGui::PopID();
    ImGui::SeparatorText("Retouches");
    ImGui::PushID("brightness");
    brightness_rows(a);
    ImGui::PopID();
    ImGui::PushID("tone");
    tone_rows(a);
    ImGui::PopID();
    put_in_order(a);
    copy_modal(a);
}
