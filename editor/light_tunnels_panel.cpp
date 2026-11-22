// The Track light window's one Tunnels section (light_tunnels.h): an All row (the tunnels' ambient light and lamps this time of day), a row for each tunnel of the track's places with its own
// look or "as all", and rows for what exists at this time of day only. Every edit goes through the file model; a refused value shows the game's rule in the window's error line.
#include "panels.h"

#include "help_marker.h"
#include "light_tunnels.h"

#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {

void refused(App *a, const char *why)
{
    snprintf(a->light.error, sizeof a->light.error, "%s", why);
}

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

// a colour as written: a swatch for six hex digits (the picker writes them), the text itself for r/g/b or a strength ("kept as it is"); `plain` the game's own when empty
bool colour_cell(const char *id, char colour[48], bool own, const char *game_default)
{
    float rgb[3] = { 1.0f, 0.82f, 0.5f };
    const bool plain = strlen(colour) == 6;
    if (colour[0] != '\0' && !plain) {
        ImGui::TextDisabled("r/g/b");   // kept as written; the full text is the tooltip, the column is narrow
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", colour);
        return false;
    }
    hex_to_rgb(colour[0] != '\0' ? colour : game_default, rgb);
    if (!own) ImGui::BeginDisabled();
    const bool picked = ImGui::ColorEdit3(id, rgb, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_PickerHueWheel);
    if (!own) ImGui::EndDisabled();
    if (!picked) return false;
    unsigned c[3];
    for (int k = 0; k < 3; ++k) c[k] = static_cast<unsigned>(rgb[k] * 255.0f + 0.5f) & 0xff;
    snprintf(colour, 48, "%02x%02x%02x", c[0], c[1], c[2]);
    return true;
}

// one number cell: shown dimmed when inherited; true when typed over
bool number_cell(const char *id, double *value, bool own, const char *format)
{
    float v = static_cast<float>(*value);
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (!own) ImGui::BeginDisabled();
    const bool changed = ImGui::InputFloat(id, &v, 0, 0, format);
    if (!own) ImGui::EndDisabled();
    if (changed) *value = v;
    return changed;
}

const char *lamp_default_colour() { return "ffd180"; }   // the warm amber the game's tunnel lamps have (1, 0.82, 0.5)

// the zone line of a row for an edit: its own, or one made from the inherited look
bool zone_of(LightFile &f, const LightTunnelRow &r, const LightTunnelLook &look, LightZone *z, int *index)
{
    *index = r.zone;
    if (r.zone >= 0) return light_file_get_zone(&f, r.zone, z) != 0;
    memset(z, 0, sizeof *z);
    z->from = r.from, z->to = r.to;
    if (look.has_light) z->has_light = 1, z->light = look.light;
    if (look.colour[0] != '\0') z->has_colour = 1, snprintf(z->colour, sizeof z->colour, "%s", look.colour);
    return true;
}

bool lamp_of(LightFile &f, const LightTunnelRow &r, const LightTunnelLook &look, LightTunnelLamp *l, int *index)
{
    *index = r.lamp;
    if (r.lamp >= 0) return light_file_get_tunnel_lamp(&f, r.lamp, l) != 0;
    memset(l, 0, sizeof *l);
    l->has_nodes = 1, l->from = r.from, l->to = r.to, l->strength = look.strength;
    if (look.lamp_colour[0] != '\0') l->has_colour = 1, snprintf(l->colour, sizeof l->colour, "%s", look.lamp_colour);
    if (look.reach_own) l->has_reach = 1, l->reach = look.reach;
    return true;
}

// the All row: the time's tunnel_tone and general tunnel_lamp line
void all_row(App *a, const LightTunnelLook &all)
{
    LightFile &f = a->light.session.file;
    char why[LIGHT_WHY];
    ImGui::TableNextRow();
    ImGui::PushID("all");
    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted("All tunnels");
    ImGui::TableSetColumnIndex(1);
    LightTunnelTone t;
    const bool have_tone = light_file_get_tunnel_tone(&f, 0, &t) != 0;
    double light = have_tone ? t.light : 100.0;
    if (number_cell("##light", &light, have_tone, have_tone ? "%.1f" : "as made")) {
        if (!have_tone) memset(&t, 0, sizeof t);
        t.light = light < 0 ? 0 : light;
        if (light_file_set_tunnel_tone(&f, have_tone ? 0 : -1, &t, why, sizeof why)) app_light_touch(a);
        else refused(a, why);
    }
    ImGui::TableSetColumnIndex(2);
    char colour[48];
    snprintf(colour, sizeof colour, "%s", have_tone && t.has_colour ? t.colour : "");
    if (colour_cell("##colour", colour, have_tone, lamp_default_colour())) {
        t.has_colour = 1, snprintf(t.colour, sizeof t.colour, "%s", colour);
        if (light_file_set_tunnel_tone(&f, 0, &t, why, sizeof why)) app_light_touch(a);
        else refused(a, why);
    }
    // the lamps' general line: the last one with no nodes
    LightTunnelLamp l;
    int lamp_at = -1;
    for (int i = 0; i < light_file_count(&f, LIGHT_KIND_TUNNEL_LAMP); ++i) {
        LightTunnelLamp g;
        if (light_file_get_tunnel_lamp(&f, i, &g) && !g.has_nodes) lamp_at = i;
    }
    const bool have_lamp = lamp_at >= 0 && light_file_get_tunnel_lamp(&f, lamp_at, &l);
    if (!have_lamp) memset(&l, 0, sizeof l), l.strength = all.strength, lamp_at = -1;
    bool changed = false;
    ImGui::TableSetColumnIndex(3);
    changed = number_cell("##strength", &l.strength, true, "%.0f") || changed;
    ImGui::TableSetColumnIndex(4);
    char lamp_colour[48];
    snprintf(lamp_colour, sizeof lamp_colour, "%s", l.has_colour ? l.colour : "");
    if (colour_cell("##lampcolour", lamp_colour, true, lamp_default_colour())) l.has_colour = 1, snprintf(l.colour, sizeof l.colour, "%s", lamp_colour), changed = true;
    ImGui::TableSetColumnIndex(5);
    double reach = l.has_reach ? l.reach : all.reach;
    if (number_cell("##reach", &reach, true, "%.1f")) l.has_reach = 1, l.reach = reach, changed = true;
    ImGui::PopID();
    if (changed) {
        l.has_nodes = 0;
        if (light_file_set_tunnel_lamp(&f, lamp_at, &l, why, sizeof why)) app_light_touch(a);
        else refused(a, why);
    }
}

// a tunnel's row; true when the file changed (the rows are listed again)
bool tunnel_row(App *a, const LightTunnelRow &r, int row_id, const LightTunnelLook &look, int *remove_place)
{
    LightFile &f = a->light.session.file;
    char why[LIGHT_WHY], label[64];
    const bool own = r.zone >= 0 || r.lamp >= 0;
    bool changed = false;
    ImGui::TableNextRow();
    ImGui::PushID(row_id);
    ImGui::TableSetColumnIndex(0);
    snprintf(label, sizeof label, "%u-%u", r.from, r.to);
    ImGui::TextUnformatted(label);
    if (r.place < 0) {
        ImGui::SameLine();
        ImGui::TextDisabled("(this time of day only)");
    } else if (r.hall) {
        ImGui::SameLine();
        ImGui::TextDisabled("(hall)");
    }
    if (r.hall) {   // a hall is lit by its own lamps: no values, only the place to take away
        if (r.place >= 0) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (ImGui::SmallButton("Remove tunnel")) *remove_place = r.place;
        }
        ImGui::PopID();
        return false;
    }
    ImGui::TableSetColumnIndex(1);
    double light = look.light;
    LightZone z;
    int zone_index = -1;
    if (number_cell("##light", &light, r.zone >= 0 && look.light_own, look.has_light ? "%.1f" : "as made") && zone_of(f, r, look, &z, &zone_index)) {
        z.has_light = 1, z.light = light < 0 ? 0 : light, z.lit = 0;
        if (light_file_set_zone(&f, zone_index, &z, why, sizeof why)) app_light_touch(a), changed = true;
        else refused(a, why);
    }
    ImGui::TableSetColumnIndex(2);
    char colour[48];
    snprintf(colour, sizeof colour, "%s", look.colour);
    if (colour_cell("##colour", colour, r.zone >= 0 && look.colour_own, lamp_default_colour()) && zone_of(f, r, look, &z, &zone_index)) {
        z.has_colour = 1, snprintf(z.colour, sizeof z.colour, "%s", colour);
        if (light_file_set_zone(&f, zone_index, &z, why, sizeof why)) app_light_touch(a), changed = true;
        else refused(a, why);
    }
    LightTunnelLamp l;
    int lamp_index = -1;
    ImGui::TableSetColumnIndex(3);
    double strength = look.strength;
    if (number_cell("##strength", &strength, r.lamp >= 0, "%.0f") && lamp_of(f, r, look, &l, &lamp_index)) {
        l.strength = strength;
        if (light_file_set_tunnel_lamp(&f, lamp_index, &l, why, sizeof why)) app_light_touch(a), changed = true;
        else refused(a, why);
    }
    ImGui::TableSetColumnIndex(4);
    char lamp_colour[48];
    snprintf(lamp_colour, sizeof lamp_colour, "%s", look.lamp_colour);
    if (colour_cell("##lampcolour", lamp_colour, r.lamp >= 0, lamp_default_colour()) && lamp_of(f, r, look, &l, &lamp_index)) {
        l.has_colour = 1, snprintf(l.colour, sizeof l.colour, "%s", lamp_colour);
        if (light_file_set_tunnel_lamp(&f, lamp_index, &l, why, sizeof why)) app_light_touch(a), changed = true;
        else refused(a, why);
    }
    ImGui::TableSetColumnIndex(5);
    double reach = look.reach;
    if (number_cell("##reach", &reach, r.lamp >= 0, "%.1f") && lamp_of(f, r, look, &l, &lamp_index)) {
        l.has_reach = 1, l.reach = reach;
        if (light_file_set_tunnel_lamp(&f, lamp_index, &l, why, sizeof why)) app_light_touch(a), changed = true;
        else refused(a, why);
    }
    ImGui::TableNextRow();   // the buttons have a line of their own: the panel is narrow
    ImGui::TableSetColumnIndex(0);
    if (own) {
        if (ImGui::SmallButton(r.place < 0 ? "Remove" : "Remove own look") && light_tunnels_remove_own(&f, &r)) app_light_touch(a), changed = true;
    } else if (ImGui::SmallButton("Own look")) {
        if (light_tunnels_own_look(&f, &r, why, sizeof why)) app_light_touch(a), changed = true;
        else refused(a, why);
    }
    if (r.place >= 0) {
        ImGui::SameLine();
        if (ImGui::SmallButton("Remove tunnel")) *remove_place = r.place;
    }
    ImGui::PopID();
    return changed;
}

}   // namespace

void light_tunnel_rows(App *a)
{
    LightSession &s = a->light.session;
    LightFile &f = s.file;
    LightTunnelRow rows[LIGHT_TUNNEL_ROWS];
    const int count = light_tunnels_list(&f, s.has_places ? &s.places : nullptr, rows, LIGHT_TUNNEL_ROWS);
    char label[64];
    snprintf(label, sizeof label, "Tunnels (%d)###lighttunnels", count);
    const bool open = ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen);
    ImGui::SameLine();
    help_marker("The tunnels of the track: how bright each is inside (Ambient, percent of full light, and its colour) and how its lamps light the road (Lamps, percent, colour, and Reach in nodes across the road). "
                "A tunnel shown dimmed is as all: Own look gives it values of its own, this time of day only; Remove own look takes them back. Add tunnel and Remove tunnel change the track's places, "
                "the same at every time of day, and where it does not rain. A hall is lit by its own lamps and has no values.");
    if (!open) return;
    LightTunnelLook all;
    light_tunnels_all(&f, &all);
    if (ImGui::BeginTable("##tunnels", 6, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoClip)) {
        ImGui::TableSetupColumn("Nodes", ImGuiTableColumnFlags_WidthFixed, 84.0f);
        ImGui::TableSetupColumn("Ambient", ImGuiTableColumnFlags_WidthFixed, 64.0f);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 30.0f);
        ImGui::TableSetupColumn("Lamps", ImGuiTableColumnFlags_WidthFixed, 64.0f);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 30.0f);
        ImGui::TableSetupColumn("Reach", ImGuiTableColumnFlags_WidthFixed, 56.0f);
        ImGui::TableHeadersRow();
        all_row(a, all);
        int remove_place = -1;
        for (int i = 0; i < count; ++i) {
            LightTunnelLook look;
            light_tunnels_look(&f, &rows[i], &look);
            if (tunnel_row(a, rows[i], i, look, &remove_place)) break;   // the file changed: the rows are listed again next frame
        }
        ImGui::EndTable();
        if (remove_place >= 0 && s.has_places) {   // the place's index counts the tunnels only: find that tunnel's line among all the places
            int seen = 0;
            for (int p = 0; p < light_file_count(&s.places, LIGHT_KIND_PLACE); ++p) {
                LightPlace pl;
                if (!light_file_get_place(&s.places, p, &pl) || strcmp(pl.what, "tunnel") != 0) continue;
                if (seen++ == remove_place && light_file_remove(&s.places, LIGHT_KIND_PLACE, p)) app_light_touch(a);
            }
        }
    }
    if (s.has_places && ImGui::Button("Add tunnel")) {
        char why[LIGHT_WHY];
        const unsigned node = static_cast<unsigned>(a->light.node), last = f.nodes - 1;
        LightPlace p = { "tunnel", node, node + 20 < last ? node + 20 : last };
        if (light_file_set_place(&s.places, -1, &p, f.nodes, why, sizeof why)) app_light_touch(a);
        else refused(a, why);
    }
}
