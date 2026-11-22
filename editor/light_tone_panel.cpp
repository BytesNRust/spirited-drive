// The Track light window's tone rows (dep98 step 2, part D): scene tones, car tones, tunnel zones and the track's places ranges, as tables with a colour picker. Every edit goes
// through the model (light_file.h): a refused value shows the game's rule in the window's error line. Called from light_panel.cpp.
#include "panels.h"

#include "help_marker.h"

#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {

void refused(App *a, const char *why)
{
    snprintf(a->light.error, sizeof a->light.error, "%s", why);
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

void rgb_to_hex(const float rgb[3], char out[8])
{
    unsigned c[3];
    for (int k = 0; k < 3; ++k) c[k] = static_cast<unsigned>(rgb[k] * 255.0f + 0.5f) & 0xff;
    snprintf(out, 8, "%02x%02x%02x", c[0], c[1], c[2]);
}

// a colour swatch that opens the picker; true when the colour changed (`hex` then holds it)
bool swatch(const char *id, char hex[8])
{
    float rgb[3] = { 1, 1, 1 };
    hex_to_rgb(hex, rgb);
    if (!ImGui::ColorEdit3(id, rgb, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_PickerHueWheel)) return false;
    rgb_to_hex(rgb, hex);
    return true;
}

// the row points a tone covers: "all", or "p6 p7"
void points_text(unsigned mask, char out[48])
{
    if (mask == 0 || mask == (1u << LIGHT_POINTS) - 1u) {
        snprintf(out, 48, "all");
        return;
    }
    size_t n = 0;
    out[0] = '\0';
    for (unsigned p = 0; p < LIGHT_POINTS; ++p)
        if (mask & (1u << p)) n += static_cast<size_t>(snprintf(out + n, 48 - n, n > 0 ? " p%u" : "p%u", p));
}

// the points' popup; true when a box changed (`mask` then holds the new set; none ticked means all)
bool points_cell(unsigned *mask)
{
    char text[48];
    points_text(*mask, text);
    bool changed = false;
    if (ImGui::Button(text, ImVec2(-FLT_MIN, 0))) ImGui::OpenPopup("points");
    if (ImGui::BeginPopup("points")) {
        for (unsigned p = 0; p < LIGHT_POINTS; ++p) {
            bool on = (*mask & (1u << p)) != 0;
            char label[8];
            snprintf(label, sizeof label, "p%u", p);
            if (ImGui::Checkbox(label, &on)) {
                *mask = on ? (*mask | (1u << p)) : (*mask & ~(1u << p));
                changed = true;
            }
        }
        ImGui::EndPopup();
    }
    return changed;
}

bool section(const char *title, int count, const char *id)
{
    char label[64];
    snprintf(label, sizeof label, "%s (%d)###%s", title, count, id);
    return ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen);
}

void tone_rows(App *a)
{
    LightFile &f = a->light.session.file;
    char why[LIGHT_WHY];
    const int count = light_file_count(&f, LIGHT_KIND_TONE);
    const bool open = section("Tone", count, "lighttone");
    ImGui::SameLine();
    help_marker("Tone: nodes from..to are seen through a colour at a strength 0 to 100. Points narrows it to some of the road's 11 row points (a side strip); none ticked is all of "
                "them. A track has 16 different colour and strength pairs at most; a later line over the same nodes wins.");
    if (!open) return;
    int remove = -1;
    if (count > 0 && ImGui::BeginTable("##tones", 6, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("From node");
        ImGui::TableSetupColumn("To node");
        ImGui::TableSetupColumn("Points");
        ImGui::TableSetupColumn("Colour", ImGuiTableColumnFlags_WidthFixed, 50.0f);
        ImGui::TableSetupColumn("Strength %");
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableHeadersRow();
        for (int i = 0; i < count; ++i) {
            LightTone t;
            if (!light_file_get_tone(&f, i, &t)) continue;
            int v[3] = { static_cast<int>(t.from), static_cast<int>(t.to), static_cast<int>(t.percent) };
            bool changed = false;
            ImGui::PushID(i);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::SetNextItemWidth(-FLT_MIN);
            changed = ImGui::InputInt("##from", &v[0], 0, 0) || changed;
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-FLT_MIN);
            changed = ImGui::InputInt("##to", &v[1], 0, 0) || changed;
            ImGui::TableSetColumnIndex(2);
            changed = points_cell(&t.mask) || changed;
            ImGui::TableSetColumnIndex(3);
            changed = swatch("##colour", t.colour) || changed;
            ImGui::TableSetColumnIndex(4);
            ImGui::SetNextItemWidth(-FLT_MIN);
            changed = ImGui::InputInt("##pct", &v[2], 0, 0) || changed;
            ImGui::TableSetColumnIndex(5);
            if (ImGui::Button("Remove")) remove = i;
            ImGui::PopID();
            if (changed) {
                t.from = non_negative(v[0]), t.to = non_negative(v[1]), t.percent = non_negative(v[2]);
                if (light_file_set_tone(&f, i, &t, why, sizeof why)) app_light_touch(a);
                else refused(a, why);
            }
        }
        ImGui::EndTable();
    }
    if (remove >= 0 && light_file_remove(&f, LIGHT_KIND_TONE, remove)) app_light_touch(a);
    if (ImGui::Button("Add tone")) {
        const unsigned node = static_cast<unsigned>(a->light.node), last = f.nodes - 1;
        LightTone t = { node, node + 50 < last ? node + 50 : last, 0, "ffe080", 50 };
        if (light_file_set_tone(&f, -1, &t, why, sizeof why)) app_light_touch(a);
        else refused(a, why);
    }
}

void car_tone_rows(App *a)
{
    LightFile &f = a->light.session.file;
    char why[LIGHT_WHY];
    const int count = light_file_count(&f, LIGHT_KIND_CAR_TONE);
    const bool open = section("Car tone", count, "lightcartone");
    ImGui::SameLine();
    help_marker("Car tone: the colour the car is seen through, at a strength 0 to 100, over the whole track or over nodes from..to. A later line wins over an earlier one.");
    if (!open) return;
    int remove = -1;
    if (count > 0 && ImGui::BeginTable("##cartones", 6, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Whole track", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("From node");
        ImGui::TableSetupColumn("To node");
        ImGui::TableSetupColumn("Colour", ImGuiTableColumnFlags_WidthFixed, 50.0f);
        ImGui::TableSetupColumn("Strength %");
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableHeadersRow();
        for (int i = 0; i < count; ++i) {
            LightCarTone c;
            if (!light_file_get_car_tone(&f, i, &c)) continue;
            int v[3] = { static_cast<int>(c.from), static_cast<int>(c.to), static_cast<int>(c.percent) };
            bool whole = !c.has_nodes, changed = false;
            ImGui::PushID(i);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (ImGui::Checkbox("##whole", &whole)) {
                c.has_nodes = whole ? 0 : 1;
                if (c.has_nodes) c.from = 0, c.to = f.nodes - 1, v[0] = 0, v[1] = static_cast<int>(f.nodes) - 1;
                changed = true;
            }
            ImGui::BeginDisabled(whole);
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-FLT_MIN);
            changed = ImGui::InputInt("##from", &v[0], 0, 0) || changed;
            ImGui::TableSetColumnIndex(2);
            ImGui::SetNextItemWidth(-FLT_MIN);
            changed = ImGui::InputInt("##to", &v[1], 0, 0) || changed;
            ImGui::EndDisabled();
            ImGui::TableSetColumnIndex(3);
            changed = swatch("##colour", c.colour) || changed;
            ImGui::TableSetColumnIndex(4);
            ImGui::SetNextItemWidth(-FLT_MIN);
            changed = ImGui::InputInt("##pct", &v[2], 0, 0) || changed;
            ImGui::TableSetColumnIndex(5);
            if (ImGui::Button("Remove")) remove = i;
            ImGui::PopID();
            if (changed) {
                if (c.has_nodes) c.from = non_negative(v[0]), c.to = non_negative(v[1]);
                c.percent = non_negative(v[2]);
                if (light_file_set_car_tone(&f, i, &c, why, sizeof why)) app_light_touch(a);
                else refused(a, why);
            }
        }
        ImGui::EndTable();
    }
    if (remove >= 0 && light_file_remove(&f, LIGHT_KIND_CAR_TONE, remove)) app_light_touch(a);
    if (ImGui::Button("Add car tone")) {
        LightCarTone c = { 0, 0, 0, "ff8020", 30 };
        if (light_file_set_car_tone(&f, -1, &c, why, sizeof why)) app_light_touch(a);
        else refused(a, why);
    }
}

void place_rows(App *a)
{
    LightSession &s = a->light.session;
    if (!s.has_places) return;
    char why[LIGHT_WHY];
    const int all_places = light_file_count(&s.places, LIGHT_KIND_PLACE);
    int count = 0;   // the tunnels are the Tunnels section's: here the canyon and car_dim ranges
    for (int i = 0; i < all_places; ++i) {
        LightPlace p;
        if (light_file_get_place(&s.places, i, &p) && strcmp(p.what, "tunnel") != 0) ++count;
    }
    const bool open = section("Places", count, "lightplaces");
    ImGui::SameLine();
    help_marker("The track's places other than tunnels, the same at every time of day (data/track/<TRACK>.txt): a canyon (TR6's) or nodes where the car keeps its dim tone (car_dim). "
                "Save writes this file too, when you changed it.");
    if (!open) return;
    static const char *const WHAT[3] = { "tunnel", "canyon", "car_dim" };
    int remove = -1;
    if (count > 0 && ImGui::BeginTable("##places", 4, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("What", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("From node");
        ImGui::TableSetupColumn("To node");
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableHeadersRow();
        for (int i = 0; i < all_places; ++i) {
            LightPlace p;
            if (!light_file_get_place(&s.places, i, &p) || strcmp(p.what, "tunnel") == 0) continue;
            int v[2] = { static_cast<int>(p.from), static_cast<int>(p.to) };
            bool changed = false;
            ImGui::PushID(i);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::BeginCombo("##what", p.what)) {
                for (const char *w : WHAT)
                    if (ImGui::Selectable(w, strcmp(w, p.what) == 0) && strcmp(w, p.what) != 0) snprintf(p.what, sizeof p.what, "%s", w), changed = true;
                ImGui::EndCombo();
            }
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-FLT_MIN);
            changed = ImGui::InputInt("##from", &v[0], 0, 0) || changed;
            ImGui::TableSetColumnIndex(2);
            ImGui::SetNextItemWidth(-FLT_MIN);
            changed = ImGui::InputInt("##to", &v[1], 0, 0) || changed;
            ImGui::TableSetColumnIndex(3);
            if (ImGui::Button("Remove")) remove = i;
            ImGui::PopID();
            if (changed) {
                p.from = non_negative(v[0]), p.to = non_negative(v[1]);
                if (light_file_set_place(&s.places, i, &p, s.file.nodes, why, sizeof why)) app_light_touch(a);
                else refused(a, why);
            }
        }
        ImGui::EndTable();
    }
    if (remove >= 0 && light_file_remove(&s.places, LIGHT_KIND_PLACE, remove)) app_light_touch(a);
    if (ImGui::Button("Add place")) {
        const unsigned node = static_cast<unsigned>(a->light.node), last = s.file.nodes - 1;
        LightPlace p = { "car_dim", node, node + 20 < last ? node + 20 : last };
        if (light_file_set_place(&s.places, -1, &p, s.file.nodes, why, sizeof why)) app_light_touch(a);
        else refused(a, why);
    }
}

}   // namespace

void light_stretch_rows(App *a)
{
    ImGui::PushID("tone_rows");   // each list numbers its rows from 0 in one window: its own ID scope, or two Remove buttons share an ID
    tone_rows(a);
    ImGui::PopID();
    ImGui::PushID("car_tone_rows");
    car_tone_rows(a);
    ImGui::PopID();
}

// the places other than tunnels (the canyons and the car dim), under the Tunnels section
void light_place_rows(App *a)
{
    ImGui::PushID("place_rows");
    place_rows(a);
    ImGui::PopID();
}
