// The Track light window (panels.h): a CD track's light file at one time of day, the retouch lines the game's light editor lists (brightness, lamp, tunnel tone) as rows,
// and the photo of the working copy (the game run headless at the photo's node). Every edit goes through the model (light_file.h): a refused value shows the game's rule.
#include "panels.h"

#include "command_keys.h"
#include "help_marker.h"

#include <float.h>
#include <stdio.h>
#include <string.h>

const char *const PANEL_LIGHT = "Track light";
const char *const PANEL_LIGHT_PHOTO = "Light photo";

namespace {

const ImVec4 BAD_TEXT(1.0f, 0.35f, 0.35f, 1.0f);

void bad_text(const char *text)
{
    ImGui::PushStyleColor(ImGuiCol_Text, BAD_TEXT);
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
}

// the model refused an edit: its reason is the caption until the next one that works
void refused(App *a, const char *why)
{
    snprintf(a->light.error, sizeof a->light.error, "%s", why);
}

void picker(App *a)
{
    LightUi &ui = a->light;
    ImGui::TextUnformatted("Track");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90.0f);
    if (ImGui::BeginCombo("##lighttrack", light_ui_track(ui.track))) {
        for (int i = 0; i < LIGHT_UI_TRACKS; ++i)
            if (ImGui::Selectable(light_ui_track(i), i == ui.track) && i != ui.track) app_light_select(a, i, ui.time);
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    help_marker("The CD's tracks, as the game's files name them. Their light files are the repo's data/track_light; TR5 has none: its light is made in code, so there is "
                "nothing to edit for it. Changing track or time asks for Save or Revert first.");
    for (int t = 0; t < LIGHT_UI_TIMES; ++t) {
        if (t > 0) ImGui::SameLine();
        if (ImGui::RadioButton(light_ui_time_name(t), ui.time == t) && ui.time != t) app_light_select(a, ui.track, t);
    }
}

void node_row(App *a)
{
    LightUi &ui = a->light;
    const int last = ui.session.has_file ? static_cast<int>(ui.session.file.nodes) - 1 : 0;
    ImGui::TextUnformatted("Photo at node");
    ImGui::SameLine();
    help_marker("The road node the car is put at for the photo (the game's TNFS_START_NODE). The photo is the game run headless, a moment after each change.");
    int node = ui.node;
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::SliderInt("##lightnode", &node, 0, last > 0 ? last : 1)) app_light_set_node(a, node);
    if (ImGui::Button("-10")) app_light_set_node(a, ui.node - 10);
    ImGui::SameLine();
    if (ImGui::Button("-1")) app_light_set_node(a, ui.node - 1);
    ImGui::SameLine();
    if (ImGui::Button("+1")) app_light_set_node(a, ui.node + 1);
    ImGui::SameLine();
    if (ImGui::Button("+10")) app_light_set_node(a, ui.node + 10);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.0f);
    if (ImGui::InputInt("##lighttyped", &node, 0, 0, ImGuiInputTextFlags_EnterReturnsTrue)) app_light_set_node(a, node);
}

void file_buttons(App *a)
{
    const LightUi &ui = a->light;
    const bool dirty = app_modified(a);
    ImGui::BeginDisabled(!dirty);
    if (ImGui::Button("Save")) app_light_save(a);
    ImGui::SameLine();
    if (ImGui::Button("Revert")) app_light_revert(a);
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (dirty) ImGui::TextDisabled("unsaved changes");
    else if (ui.session.has_file) ImGui::TextDisabled("%s%c.txt", ui.session.track, ui.session.letter);
    ImGui::SameLine();
    help_marker("Save writes only this track's light file in the light folder (the one in the Window's caption), and never a folder under original/. The file's other lines "
                "stay exactly as they are; a new line goes after the last. Revert goes back to the file as it was read or saved.");
}

// each kind's rows: i is the line's index within its kind; an edit that the model takes is an edit, one it refuses shows its rule
void brightness_rows(App *a)
{
    LightFile &f = a->light.session.file;
    char why[LIGHT_WHY];
    const int count = light_file_count(&f, LIGHT_KIND_BRIGHTNESS);
    int remove = -1;
    if (count > 0 && ImGui::BeginTable("##brightness", 4, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("From node");
        ImGui::TableSetupColumn("To node");
        ImGui::TableSetupColumn("Percent");
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableHeadersRow();
        for (int i = 0; i < count; ++i) {
            LightBrightness b;
            if (!light_file_get_brightness(&f, i, &b)) continue;
            int v[3] = { static_cast<int>(b.from), static_cast<int>(b.to), static_cast<int>(b.percent) };
            bool changed = false;
            ImGui::PushID(i);
            ImGui::TableNextRow();
            for (int c = 0; c < 3; ++c) {
                ImGui::TableSetColumnIndex(c);
                ImGui::SetNextItemWidth(-FLT_MIN);
                changed = ImGui::InputInt(c == 0 ? "##from" : c == 1 ? "##to" : "##pct", &v[c], 0, 0) || changed;
            }
            ImGui::TableSetColumnIndex(3);
            if (ImGui::Button("Remove")) remove = i;
            ImGui::PopID();
            if (changed) {
                const LightBrightness next = { static_cast<unsigned>(v[0] < 0 ? 0 : v[0]), static_cast<unsigned>(v[1] < 0 ? 0 : v[1]), static_cast<unsigned>(v[2] < 0 ? 0 : v[2]) };
                if (light_file_set_brightness(&f, i, &next, why, sizeof why)) app_light_touch(a);
                else refused(a, why);
            }
        }
        ImGui::EndTable();
    }
    if (remove >= 0 && light_file_remove(&f, LIGHT_KIND_BRIGHTNESS, remove)) app_light_touch(a);
    if (ImGui::Button("Add brightness")) {
        const unsigned node = static_cast<unsigned>(a->light.node), last = f.nodes - 1;
        const LightBrightness b = { node, node + 50 < last ? node + 50 : last, 100 };
        if (light_file_set_brightness(&f, -1, &b, why, sizeof why)) app_light_touch(a);
        else refused(a, why);
    }
    ImGui::SameLine();
    help_marker("Brightness: from node, to node, percent. The road's light between those nodes is scaled (100 is as it is). A later line wins over an earlier one.");
}

// a night file has hundreds of lamps: the list is folded, with its count, and scrolls inside its own box
void lamp_rows(App *a)
{
    LightFile &f = a->light.session.file;
    char why[LIGHT_WHY];
    const int count = light_file_count(&f, LIGHT_KIND_LAMP);
    char label[48];
    snprintf(label, sizeof label, "Lamps (%d)###lamps", count);
    const bool open = ImGui::CollapsingHeader(label);
    ImGui::SameLine();
    help_marker("Lamp: node, light percent. A lamp lights the road around its node at night (a night file's only; the game takes 2048 at most). Its colour and radius stay as "
                "the file has them.");
    if (!open) return;
    int remove = -1;
    if (count > 0 && ImGui::BeginChild("##lamplist", ImVec2(0, 220), ImGuiChildFlags_Borders)) {
        if (ImGui::BeginTable("##lamps", 3, ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Node");
            ImGui::TableSetupColumn("Light %");
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 70.0f);
            ImGui::TableHeadersRow();
            for (int i = 0; i < count; ++i) {
                LightLamp l;
                if (!light_file_get_lamp(&f, i, &l)) continue;
                int v[2] = { static_cast<int>(l.node), static_cast<int>(l.percent) };
                bool changed = false;
                ImGui::PushID(i);
                ImGui::TableNextRow();
                for (int c = 0; c < 2; ++c) {
                    ImGui::TableSetColumnIndex(c);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    changed = ImGui::InputInt(c == 0 ? "##node" : "##pct", &v[c], 0, 0) || changed;
                }
                ImGui::TableSetColumnIndex(2);
                if (ImGui::Button("Remove")) remove = i;
                ImGui::PopID();
                if (changed) {
                    l.node = static_cast<unsigned>(v[0] < 0 ? 0 : v[0]);
                    l.percent = static_cast<unsigned>(v[1] < 0 ? 0 : v[1]);
                    if (light_file_set_lamp(&f, i, &l, why, sizeof why)) app_light_touch(a);
                    else refused(a, why);
                }
            }
            ImGui::EndTable();
        }
    }
    if (count > 0) ImGui::EndChild();
    if (remove >= 0 && light_file_remove(&f, LIGHT_KIND_LAMP, remove)) app_light_touch(a);
    if (ImGui::Button("Add lamp")) {
        LightLamp l;
        memset(&l, 0, sizeof l);
        l.node = static_cast<unsigned>(a->light.node);
        l.percent = 100;
        if (light_file_set_lamp(&f, -1, &l, why, sizeof why)) app_light_touch(a);
        else refused(a, why);
    }
}

void tunnel_tone_row(App *a)
{
    LightFile &f = a->light.session.file;
    char why[LIGHT_WHY];
    LightTunnelTone t;
    ImGui::TextUnformatted("Tunnel look");
    ImGui::SameLine();
    help_marker("Tunnel tone: the light inside the tunnels, in percent of full light. A file has at most one.");
    if (light_file_get_tunnel_tone(&f, 0, &t)) {
        float light = static_cast<float>(t.light);
        ImGui::SetNextItemWidth(-90.0f);
        if (ImGui::InputFloat("##tt", &light, 0, 0, "%.1f")) {
            t.light = light < 0 ? 0 : light;
            if (light_file_set_tunnel_tone(&f, 0, &t, why, sizeof why)) app_light_touch(a);
            else refused(a, why);
        }
        ImGui::SameLine();
        if (ImGui::Button("Remove##tt") && light_file_remove(&f, LIGHT_KIND_TUNNEL_TONE, 0)) app_light_touch(a);
    } else if (ImGui::Button("Add tunnel look")) {
        memset(&t, 0, sizeof t);
        t.light = 100;
        if (light_file_set_tunnel_tone(&f, -1, &t, why, sizeof why)) app_light_touch(a);
        else refused(a, why);
    }
}

// the lines the window does not edit: shown as they are, with a count
void other_lines(const App *a)
{
    const LightFile &f = a->light.session.file;
    const int other = light_file_count(&f, LIGHT_KIND_OTHER) + light_file_count(&f, LIGHT_KIND_BAD);
    char label[64];
    snprintf(label, sizeof label, "Other lines (%d)###lightother", other);
    if (!ImGui::CollapsingHeader(label)) return;
    if (ImGui::BeginChild("lightotherlines", ImVec2(0, 140), ImGuiChildFlags_Borders)) {
        for (int i = 0; i < f.count; ++i)
            if (f.lines[i].kind >= LIGHT_KIND_BAD) ImGui::TextUnformatted(f.lines[i].text);
    }
    ImGui::EndChild();
}

// the light folder this window tunes (or, in red, that none is set) and the button that asks for another
void folder_row(App *a)
{
    ImGui::TextUnformatted("Light folder");
    ImGui::SameLine();
    if (ImGui::Button("Choose...")) a->go = APP_ACT_LIGHT_PICK;
    if (a->light_dir[0] != '\0' && a->light.open) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextWrapped("%s", a->light_dir);
        ImGui::PopStyleColor();
    } else {
        bad_text(a->light.error[0] != '\0' ? a->light.error : "no folder set");
    }
}

}   // namespace

void light_panel(App *a)
{
    LightUi &ui = a->light;
    if (ImGui::Begin(PANEL_LIGHT)) {
        folder_row(a);
        if (!ui.open) {   // no folder yet: nothing to tune until one is chosen
            ImGui::End();
            return;
        }
        picker(a);
        if (!ui.session.has_file) {
            ImGui::TextDisabled("No light file for this track");
            ImGui::SameLine();
            help_marker("This track's light is made in code: the CD has no light file for it, so there is nothing to edit or save here.");
        } else {
            node_row(a);
            file_buttons(a);
            if (ui.error[0] != '\0') bad_text(ui.error);
            ImGui::SeparatorText("Brightness");
            brightness_rows(a);
            if (ui.session.letter == 'N') {
                lamp_rows(a);
            }
            ImGui::SeparatorText("Tunnels");
            tunnel_tone_row(a);
            light_tone_rows(a);
            other_lines(a);
        }
    }
    ImGui::End();
}

void light_photo_panel(App *a, ImTextureID photo)
{
    const LightUi &ui = a->light;
    if (ImGui::Begin(PANEL_LIGHT_PHOTO)) {
        const bool running = app_light_busy(a);
        if (running) ImGui::TextDisabled("refreshing...");
        else if (ui.photo.rgba != nullptr) ImGui::TextDisabled("%s, node %d, %s", ui.session.track, ui.photo.node, light_ui_time_name(ui.time));
        if (ui.photo.rgba != nullptr && photo != 0) {   // the picture, as big as the window leaves room for; the last good one stays while the next is made
            const ImVec2 room = ImGui::GetContentRegionAvail();
            const float pw = static_cast<float>(ui.photo.w), ph = static_cast<float>(ui.photo.h);
            float w = room.x < pw ? room.x : pw;
            if (w * ph / pw > room.y) w = room.y * pw / ph;
            if (w > 8.0f) ImGui::Image(photo, ImVec2(w, w * ph / pw));
        } else if (!running && ui.session.has_file && ui.error[0] == '\0') {
            ImGui::TextDisabled("no photo yet");
        }
        if (ui.open && ui.error[0] != '\0' && ui.photo.rgba == nullptr) bad_text(ui.error);   // with no folder the folder row says it
    }
    ImGui::End();
}

void light_shortcuts(App *a)
{
    if (command_key(ImGui::GetIO().WantTextInput) == COMMAND_SAVE) app_save(a);
}
