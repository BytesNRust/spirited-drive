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
bool sun_part(const char *label, int *has, double *value, float lo, float hi, const char *format, float auto_value)
{
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
    if (ImGui::SliderFloat("##v", &v, lo, hi, set ? format : "auto")) *value = static_cast<double>(v), changed = true;
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
    ImGui::SeparatorText(time_of(a) == LT_TIME_N ? "Moon" : "Sun");
    ImGui::SameLine();
    help_marker("The light's direction (compass: north up, the arrow points to where the light comes from), its height above the horizon, and the shadow strength: the light left in "
                "full shadow, in percent (100 is no shadow). A part that is not ticked is the generator's own (the direction from the sky's glow). At night this is the moon, with no shadow "
                "by default. Shadows fall only in a clear race, never in fog, rain, storm or snow. The photo follows every change.");
    bool changed = false;
    sun_dial(s);
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::TextUnformatted("Direction");
    changed = sun_part("azimuth", &s.has_azimuth, &s.azimuth, 0.0f, 360.0f, "%.0f deg", 150.0f) || changed;
    ImGui::TextUnformatted("Height");
    changed = sun_part("elevation", &s.has_elevation, &s.elevation, -90.0f, 90.0f, "%.0f deg", 20.0f) || changed;
    ImGui::TextUnformatted("Shadow strength");
    changed = sun_part("shadow", &s.has_shadow, &s.shadow, 0.0f, 100.0f, "%.0f %%", 55.0f) || changed;
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

// writes a look the row edited: a new line when `index` is -1, a line with nothing left in it is dropped
void put_look(App *a, int index, const LightTrackLook &v)
{
    char why[LT_WHY];
    LightTrack &f = file_of(a);
    if (look_is_empty(v) && v.has_nodes) {
        if (index >= 0 && !light_track_remove(&f, time_of(a), LT_LOOK, index, why, sizeof why)) return refused(a, why);
        return app_light_touch(a);
    }
    if (light_track_set_look(&f, time_of(a), index, &v, why, sizeof why)) app_light_touch(a);
    else refused(a, why);
}

// the cells of a look: light, colour, lamps strength, reach; true when one changed. A hall takes only the lamps and the reach.
bool look_cells(LightTrackLook &v, bool hall)
{
    bool changed = false;
    ImGui::TableSetColumnIndex(3);
    ImGui::BeginDisabled(hall);
    changed = optional_number("##light", &v.has_light, &v.light, "%g", -FLT_MIN) || changed;
    ImGui::EndDisabled();
    ImGui::TableSetColumnIndex(4);
    ImGui::BeginDisabled(hall);
    char hex[8] = "ffffff";
    if (v.has_colour && strlen(v.colour) >= 6) snprintf(hex, sizeof hex, "%.6s", v.colour);
    if (swatch("##colour", hex, sizeof hex)) v.has_colour = 1, snprintf(v.colour, sizeof v.colour, "%s", hex), changed = true;
    if (v.has_colour) {
        ImGui::SameLine();
        if (ImGui::SmallButton("x")) v.has_colour = 0, changed = true;
    }
    ImGui::EndDisabled();
    ImGui::TableSetColumnIndex(5);
    changed = optional_number("##lamps", &v.has_lamps, &v.lamps, "%g", -FLT_MIN) || changed;
    ImGui::TableSetColumnIndex(6);
    changed = optional_number("##reach", &v.has_reach, &v.reach, "%g", -FLT_MIN) || changed;
    return changed;
}

struct TunnelRow {
    int tunnel;   // the fact's index, -1 for a look over some nodes inside a tunnel
    int look;     // the time's look line, -1 for none yet
};

void tunnel_rows(App *a)
{
    LightTrack &f = file_of(a);
    char why[LT_WHY];
    const int time = time_of(a);
    ImGui::SeparatorText("Tunnels");
    ImGui::SameLine();
    help_marker("The track's tunnels: where each is (nodes) and what it is (a tunnel, or a hall: a roofed straight lit by its own lamps) are the same at every time. Light (percent of "
                "full light inside), Colour, Lamps (the lamps' strength, percent) and Reach (the lamp pools' reach, in nodes) are this time's look of that tunnel; an empty cell takes the "
                "default row's. A hall takes only Lamps and Reach.");
    const int tunnels = light_track_count(&f, LT_TIME_FACTS, LT_TUNNEL), looks = light_track_count(&f, time, LT_LOOK);
    TunnelRow rows[2 * LT_MAX_PER_TIME];
    int count = 0, default_look = -1;
    bool used[LT_MAX_PER_TIME + 1] = { false };
    for (int i = 0; i < tunnels && count < 2 * LT_MAX_PER_TIME; ++i) {
        LightTrackTunnel t;
        if (!light_track_get_tunnel(&f, i, &t)) continue;
        int found = -1;
        for (int j = 0; j < looks && j < LT_MAX_PER_TIME && found < 0; ++j) {
            LightTrackLook v;
            if (light_track_get_look(&f, time, j, &v) && v.has_nodes && v.from == t.from && v.to == t.to && !used[j]) found = j;
        }
        if (found >= 0) used[found] = true;
        rows[count++] = { i, found };
    }
    for (int j = 0; j < looks && j < LT_MAX_PER_TIME; ++j) {
        LightTrackLook v;
        if (!light_track_get_look(&f, time, j, &v)) continue;
        if (!v.has_nodes) default_look = j;
        else if (!used[j] && count < 2 * LT_MAX_PER_TIME) rows[count++] = { -1, j };
    }
    int remove_tunnel = -1, remove_look = -1;
    // the table scrolls across when the panel is narrow; its height is its rows' (a scrolling table would otherwise take what is left of the window)
    const float row_h = ImGui::GetFrameHeight() + 2.0f * ImGui::GetStyle().CellPadding.y;
    const float table_height = row_h * static_cast<float>(count + 2) + ImGui::GetStyle().ScrollbarSize + 2.0f;
    if (ImGui::BeginTable("##tunnels", 8, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollX, ImVec2(0.0f, table_height))) {
        ImGui::TableSetupColumn("From", ImGuiTableColumnFlags_WidthFixed, 56.0f);
        ImGui::TableSetupColumn("To", ImGuiTableColumnFlags_WidthFixed, 56.0f);
        ImGui::TableSetupColumn("Kind", ImGuiTableColumnFlags_WidthFixed, 72.0f);
        ImGui::TableSetupColumn("Light %", ImGuiTableColumnFlags_WidthFixed, 60.0f);   // fixed widths: a stretch column holding a -FLT_MIN item gives a NaN content size under UBSan
        ImGui::TableSetupColumn("Colour", ImGuiTableColumnFlags_WidthFixed, 48.0f);
        ImGui::TableSetupColumn("Lamps %", ImGuiTableColumnFlags_WidthFixed, 64.0f);
        ImGui::TableSetupColumn("Reach", ImGuiTableColumnFlags_WidthFixed, 56.0f);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 62.0f);
        ImGui::TableHeadersRow();
        // the default row: the look of every tunnel with no row of its own
        {
            LightTrackLook v;
            memset(&v, 0, sizeof v);
            if (default_look >= 0) light_track_get_look(&f, time, default_look, &v);
            ImGui::PushID("default");
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextDisabled("default");
            if (look_cells(v, false)) {
                v.has_nodes = 0;
                put_look(a, default_look, v);
            }
            ImGui::PopID();
        }
        for (int r = 0; r < count; ++r) {
            LightTrackTunnel t = { 0, 0, 0 };
            LightTrackLook v;
            memset(&v, 0, sizeof v);
            if (rows[r].tunnel >= 0) light_track_get_tunnel(&f, rows[r].tunnel, &t);
            if (rows[r].look >= 0) light_track_get_look(&f, time, rows[r].look, &v);
            if (rows[r].tunnel < 0) t.from = v.from, t.to = v.to;
            else if (rows[r].look < 0) v.has_nodes = 1, v.from = t.from, v.to = t.to;
            ImGui::PushID(r);
            ImGui::TableNextRow();
            bool tunnel_changed = false;
            ImGui::TableSetColumnIndex(0);
            if (rows[r].tunnel >= 0) tunnel_changed = node_box("##from", &t.from, -FLT_MIN) || tunnel_changed;
            else ImGui::Text("%u", t.from);
            ImGui::TableSetColumnIndex(1);
            if (rows[r].tunnel >= 0) tunnel_changed = node_box("##to", &t.to, -FLT_MIN) || tunnel_changed;
            else ImGui::Text("%u", t.to);
            ImGui::TableSetColumnIndex(2);
            if (rows[r].tunnel >= 0) {
                ImGui::SetNextItemWidth(-FLT_MIN);
                if (ImGui::BeginCombo("##kind", t.lit ? "hall" : "tunnel")) {
                    if (ImGui::Selectable("tunnel", !t.lit) && t.lit) t.lit = 0, tunnel_changed = true;
                    if (ImGui::Selectable("hall", t.lit != 0) && !t.lit) t.lit = 1, tunnel_changed = true;
                    ImGui::EndCombo();
                }
            } else {
                ImGui::TextDisabled("inside");
            }
            const bool look_changed = look_cells(v, rows[r].tunnel >= 0 && t.lit != 0);
            ImGui::TableSetColumnIndex(7);
            const bool remove_row = ImGui::Button("Remove");
            char scope[16];
            snprintf(scope, sizeof scope, "tunnel%d", r);
            ui_probe_scope(scope);
            ui_probe("remove");
            ui_probe_scope("");
            if (remove_row) {
                remove_tunnel = rows[r].tunnel;
                remove_look = rows[r].look;
            }
            ImGui::PopID();
            if (tunnel_changed) {   // the nodes move with the look that follows them
                if (rows[r].look >= 0 && (v.from != t.from || v.to != t.to)) {
                    LightTrackLook moved = v;
                    moved.from = t.from, moved.to = t.to;
                    LightTrackTunnel old = t;
                    if (light_track_get_tunnel(&f, rows[r].tunnel, &old) && (old.from != t.from || old.to != t.to)) {
                        // the tunnel's nodes must hold the look: widen the tunnel first, then move the look, or the other way round when it narrows
                        if (!light_track_set_tunnel(&f, rows[r].tunnel, &t, why, sizeof why)) {
                            if (!light_track_set_look(&f, time, rows[r].look, &moved, why, sizeof why) || !light_track_set_tunnel(&f, rows[r].tunnel, &t, why, sizeof why)) {
                                refused(a, why);
                                continue;
                            }
                        } else if (!light_track_set_look(&f, time, rows[r].look, &moved, why, sizeof why)) {
                            refused(a, why);
                            continue;
                        }
                        app_light_touch(a);
                        continue;
                    }
                }
                if (light_track_set_tunnel(&f, rows[r].tunnel, &t, why, sizeof why)) app_light_touch(a);
                else refused(a, why);
            } else if (look_changed) {
                put_look(a, rows[r].look, v);
            }
        }
        ImGui::EndTable();
    }
    if (remove_tunnel >= 0 || remove_look >= 0) {   // the row's look of this time first, then the tunnel; the file changed after the first, so the window is touched either way
        bool dropped = false;
        if (remove_look >= 0) {
            if (light_track_remove(&f, time, LT_LOOK, remove_look, why, sizeof why)) dropped = true;
            else refused(a, why), remove_tunnel = -1;
        }
        if (remove_tunnel >= 0) {
            if (light_track_remove(&f, LT_TIME_FACTS, LT_TUNNEL, remove_tunnel, why, sizeof why)) dropped = true;
            else refused(a, why);
        }
        if (dropped) {
            char said[sizeof a->light.error];   // the touch clears the error line: a refusal of the second removal stays
            snprintf(said, sizeof said, "%s", a->light.error);
            app_light_touch(a);
            snprintf(a->light.error, sizeof a->light.error, "%s", said);
        }
    }
    const bool add_tunnel = ImGui::Button("Add tunnel");
    ui_probe("add_tunnel");
    if (add_tunnel) {
        const unsigned node = static_cast<unsigned>(a->light.node), last = last_node(a);
        const LightTrackTunnel t = { node, node + 50 < last ? node + 50 : last, 0 };
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
    char custom[16] = "";
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
        for (const Choice &c : CHOICES)
            if (ImGui::Selectable(c.label, l->has_strip && c.mask == (l->mask & LT_ALL_POINTS))) l->has_strip = 1, l->mask = c.mask, changed = true;
        ImGui::EndCombo();
    }
    ui_probe("strip");
    return changed;
}

// a night file has hundreds of lamps: the list is folded, with its count, and scrolls inside its own box
void lamp_rows(App *a)
{
    LightTrack &f = file_of(a);
    char why[LT_WHY];
    const int count = light_track_count(&f, time_of(a), LT_LAMP);
    char label[48];
    snprintf(label, sizeof label, "Lamps (%d)###lamps", count);
    const bool open = ImGui::CollapsingHeader(label, count > 0 && count <= 24 ? ImGuiTreeNodeFlags_DefaultOpen : 0);
    ImGui::SameLine();
    help_marker("A lamp lights the road around its node: its light in percent, its colour (empty: the game's), its radius in nodes (empty: the game's). A lamp is allowed at every time, not "
                "only at night (a neon, a shop window). The game takes 2048 at most. Its strip is the part of the road it stands over: all, left, right or road (none: where it stood before strips). A run of points typed in the file is kept as it is.");
    if (!open) return;
    int remove = -1;
    if (count > 0 && ImGui::BeginChild("##lamplist", ImVec2(0, 220), ImGuiChildFlags_Borders)) {
        if (ImGui::BeginTable("##lamps", 6, ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Node", ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableSetupColumn("Light %", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("Colour", ImGuiTableColumnFlags_WidthFixed, 56.0f);
            ImGui::TableSetupColumn("Radius", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("Strip", ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 70.0f);
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
                ImGui::TableSetColumnIndex(0);
                changed = node_box("##node", &l.node, -FLT_MIN) || changed;
                ImGui::TableSetColumnIndex(1);
                {
                    int v = static_cast<int>(l.percent);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (ImGui::InputInt("##pct", &v, 0, 0)) l.percent = non_negative(v), changed = true;
                }
                ImGui::TableSetColumnIndex(2);
                {
                    char hex[8] = "ffffff";
                    if (l.has_colour && strlen(l.colour) >= 6) snprintf(hex, sizeof hex, "%.6s", l.colour);
                    if (swatch("##colour", hex, sizeof hex)) l.has_colour = 1, snprintf(l.colour, sizeof l.colour, "%s", hex), changed = true;
                }
                ImGui::TableSetColumnIndex(3);
                {
                    double r = static_cast<double>(l.radius);
                    int has = l.has_radius;
                    if (optional_number("##radius", &has, &r, "%.0f", -FLT_MIN)) l.has_radius = has, l.radius = has ? non_negative(static_cast<int>(r)) : 0, changed = true;
                }
                ImGui::TableSetColumnIndex(4);
                changed = lamp_strip_combo(&l) || changed;
                ImGui::TableSetColumnIndex(5);
                if (ImGui::Button("Remove")) remove = i;
                ui_probe_scope("");
                ImGui::PopID();
                if (changed) {
                    if (light_track_set_lamp(&f, time_of(a), i, &l, why, sizeof why)) app_light_touch(a);
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
        changed = light_strip_bar("##strip", &b.mask) || changed;
        ui_probe_scope("");
        ImGui::Separator();
        ImGui::PopID();
        if (changed) {
            if (b.mask == 0) {   // nothing picked: the strip stays the old one until a point is
                refused(a, "pick at least one point of the row");
                continue;
            }
            b.from = non_negative(v[0]), b.to = non_negative(v[1]), b.percent = non_negative(v[2]);
            if (light_track_set_brightness(&f, time_of(a), i, &b, why, sizeof why)) app_light_touch(a);
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
        changed = light_strip_bar("##strip", &t.mask) || changed;
        ui_probe_scope("");
        ImGui::Separator();
        ImGui::PopID();
        if (changed) {
            if (t.mask == 0) {
                refused(a, "pick at least one point of the row");
                continue;
            }
            t.from = non_negative(v[0]), t.to = non_negative(v[1]), t.percent = non_negative(v[2]);
            if (light_track_set_tone(&f, time_of(a), i, &t, why, sizeof why)) app_light_touch(a);
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
}
