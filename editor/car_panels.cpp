// The start screen and the car's panels (panels.h): what Studio shows with nothing open, and what it shows of an open car (read only for now).
#include "panels.h"

#include "imgui.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

const char *const PANEL_CAR = "Car", *const PANEL_TEXTURES = "Textures", *const PANEL_MASKS = "Masks";

namespace {

void muted_wrapped(const char *format, ...) IM_FMTARGS(1);
void muted_wrapped(const char *format, ...)
{
    char text[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof text, format, args);
    va_end(args);
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
}

// the last component of a folder path
const char *leaf(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash != nullptr && slash[1] != '\0' ? slash + 1 : path;
}

// a row of the mod.ini table: nothing when the card does not give the key
void field_row(const char *key, const char *value)
{
    if (value[0] == '\0') return;
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextDisabled("%s", key);
    ImGui::TableSetColumnIndex(1);
    ImGui::TextWrapped("%s", value);
}

// a list of file names, with how many more there are than the names kept
void name_list(const char names[][CAR_PROJECT_NAME], int count)
{
    for (int i = 0; i < count && i < CAR_PROJECT_LIST; ++i) ImGui::BulletText("%s", names[i]);
    if (count > CAR_PROJECT_LIST) ImGui::TextDisabled("and %d more", count - CAR_PROJECT_LIST);
}

}   // namespace

void start_screen(App *a)
{
    const ImGuiViewport *vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus;
    if (ImGui::Begin("Start", nullptr, flags)) {
        const float width = ImGui::GetContentRegionAvail().x, column = width < 520.0f ? width : 520.0f;
        ImGui::SetCursorPosX((width - column) / 2 + ImGui::GetStyle().WindowPadding.x);
        ImGui::BeginGroup();
        ImGui::Dummy(ImVec2(0, 18));
        ImGui::SetWindowFontScale(1.6f);
        ImGui::TextUnformatted("Spirited Drive Studio");
        ImGui::SetWindowFontScale(1.0f);
        muted_wrapped("Make cars and tracks for Spirited Drive.");
        ImGui::Dummy(ImVec2(0, 10));
        const ImVec2 button(column / 2 - 4, 0);
        if (ImGui::Button("Open car...", button)) app_ask(a, APP_ACT_OPEN_CAR, "");
        ImGui::SameLine();
        if (ImGui::Button("Open track...", button)) app_ask(a, APP_ACT_OPEN, "");
        if (ImGui::Button("New track...", button)) app_ask(a, APP_ACT_NEW, "");
        ImGui::SameLine();
        ImGui::BeginDisabled();
        ImGui::Button("Export original car...", button);
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Coming soon");
        ImGui::Dummy(ImVec2(0, 14));
        ImGui::SeparatorText("Recent");
        if (a->recent.count == 0) muted_wrapped("Nothing opened yet.");
        for (int i = 0; i < a->recent.count; ++i) {
            const bool there = recent_present(a->recent.dir[i], a->recent.kind[i]);
            const bool car = a->recent.kind[i] == RECENT_CAR;
            char label[128];
            snprintf(label, sizeof label, "%s  (%s)", leaf(a->recent.dir[i]), car ? "car" : "track");
            ImGui::PushID(i);
            ImGui::BeginDisabled(!there);   // a folder that is gone is greyed
            if (ImGui::Selectable(label, false, 0, ImVec2(column, 0))) app_ask(a, APP_ACT_RECENT, a->recent.dir[i]);
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("%s%s", a->recent.dir[i], there ? "" : "\nnot found: the folder was moved or deleted");
            ImGui::PopID();
        }
        if (a->status[0] != '\0') {
            ImGui::Dummy(ImVec2(0, 10));
            muted_wrapped("%s", a->status);
        }
        ImGui::EndGroup();
    }
    ImGui::End();
}

void car_panel(App *a)
{
    if (ImGui::Begin(PANEL_CAR)) {
        const CarProject &c = a->car;
        const ModInfo &m = c.info;
        ImGui::SetWindowFontScale(1.25f);
        ImGui::TextUnformatted(m.name);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::Text("kit %s   stem %s", c.kit, m.stem);
        muted_wrapped("%s", c.root);
        muted_wrapped("Read only for now: Studio writes nothing to this car.");
        ImGui::Separator();
        if (ImGui::BeginTable("mod.ini", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("key", ImGuiTableColumnFlags_WidthFixed, 96);
            ImGui::TableSetupColumn("value");
            field_row("id", m.id);
            field_row("version", m.version);
            field_row("short name", m.short_name);
            field_row("make", m.make);
            field_row("model", m.model);
            field_row("country", m.country);
            field_row("year", m.year);
            field_row("power", m.power);
            field_row("weight", m.weight);
            field_row("category", m.category);
            field_row("ABS", m.assist_abs);
            field_row("traction", m.assist_traction);
            field_row("AI level", m.ai_level);
            field_row("sound from", m.sound_from);
            field_row("dash from", m.dash_from);
            field_row("author", m.author);
            field_row("description", m.description);
            if (m.card_name_off) field_row("card name", "off");
            ImGui::EndTable();
        }
        if (m.paint_count > 0) {
            ImGui::SeparatorText("Colors");
            for (int i = 0; i < m.paint_count; ++i) {
                const ModPaint &p = m.paints[i];
                ImGui::PushID(i);
                ImGui::ColorButton("##paint", ImVec4(p.rgb[0] / 255.0f, p.rgb[1] / 255.0f, p.rgb[2] / 255.0f, 1.0f),
                                   ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, ImVec2(14, 14));
                ImGui::SameLine();
                ImGui::Text("%s  #%02x%02x%02x", p.name, p.rgb[0], p.rgb[1], p.rgb[2]);
                ImGui::PopID();
            }
        }
    }
    ImGui::End();
}

void textures_panel(App *a)
{
    if (ImGui::Begin(PANEL_TEXTURES)) {
        const CarProject &c = a->car;
        if (!c.has_modelo) {
            muted_wrapped("No modelo/ folder beside this car's folder: there are no textures to list.");
        } else {
            ImGui::Text("Close up (modelo/c1): %d", c.texture_count);
            name_list(c.texture, c.texture_count);
            ImGui::Spacing();
            ImGui::Text("Far away (modelo/c3): %d", c.far_count);
        }
    }
    ImGui::End();
}

void masks_panel(App *a)
{
    if (ImGui::Begin(PANEL_MASKS)) {
        const CarProject &c = a->car;
        ImGui::Text("Car file: %s", leaf(c.cfm));
        ImGui::Spacing();
        if (!c.has_masks_dir) {
            ImGui::Text("Masks of the car (car_masks/%s): none yet", c.info.stem);
        } else {
            ImGui::Text("Masks of the car (car_masks/%s): %d", c.info.stem, c.mask_count);
            name_list(c.mask, c.mask_count);
        }
        ImGui::Spacing();
        if (c.kit_mask_count > 0) {
            ImGui::Text("Your color masks (mascaras/): %d", c.kit_mask_count);
            name_list(c.kit_mask, c.kit_mask_count);
        } else {
            muted_wrapped("No color masks of your own (mascaras/).");
        }
    }
    ImGui::End();
}
