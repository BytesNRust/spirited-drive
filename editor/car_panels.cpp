// The start screen and the car's panels (panels.h): what Studio shows with nothing open, and what it shows of an open car (read only for now).
#include "panels.h"

#include "imgui.h"

extern "C" {
#include "formats/car_piece.h"
}

#include <float.h>
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

namespace {

const ImVec4 BAD_TEXT(1.0f, 0.35f, 0.35f, 1.0f);

void problem_text(const char *text)
{
    ImGui::PushStyleColor(ImGuiCol_Text, BAD_TEXT);
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
}

// the first problem of `field` (and of colour `paint` when the field is the colors), or nullptr
const CarEditProblem *problem_for(const CarEditProblem *list, int count, int field, int paint = -1)
{
    for (int i = 0; i < count; ++i)
        if (list[i].field == field && (field != CE_PAINTS || list[i].paint == paint)) return &list[i];
    return nullptr;
}

// one row of the card: its label, a text box (red when the field has a problem), and the problem under it
void text_row(App *a, int field, const CarEditProblem *problems, int count)
{
    const CarEditProblem *bad = problem_for(problems, count, field);
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(car_edit_label(field));
    ImGui::TableSetColumnIndex(1);
    char buf[CE_VALUE_MAX];
    snprintf(buf, sizeof buf, "%s", a->edit.now.value[field]);
    ImGui::PushID(field);
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (bad != nullptr) ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.45f, 0.10f, 0.10f, 1.0f));
    if (ImGui::InputText("##v", buf, sizeof buf)) car_edit_set(&a->edit, field, buf);
    if (bad != nullptr) ImGui::PopStyleColor();
    ImGui::PopID();
    if (bad != nullptr) problem_text(bad->text);
}

// a row with a few words to choose from; "" is `none` (the card gives no line)
void word_row(App *a, int field, const char *none, const char *const *words, int word_count, const CarEditProblem *problems, int count)
{
    const CarEditProblem *bad = problem_for(problems, count, field);
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(car_edit_label(field));
    ImGui::TableSetColumnIndex(1);
    const char *now = a->edit.now.value[field];
    ImGui::PushID(field);
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (bad != nullptr) ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.45f, 0.10f, 0.10f, 1.0f));
    if (ImGui::BeginCombo("##w", now[0] != '\0' ? now : none)) {
        if (none != nullptr && ImGui::Selectable(none, now[0] == '\0')) car_edit_set(&a->edit, field, "");
        for (int i = 0; i < word_count; ++i)
            if (ImGui::Selectable(words[i], strcmp(now, words[i]) == 0)) car_edit_set(&a->edit, field, words[i]);
        ImGui::EndCombo();
    }
    if (bad != nullptr) ImGui::PopStyleColor();
    ImGui::PopID();
    if (bad != nullptr) problem_text(bad->text);
}

void colors_section(App *a, const CarEditProblem *problems, int count)
{
    CarEditState &s = a->edit.now;
    ImGui::SeparatorText("Colors");
    if (s.paint_count == 0) muted_wrapped("No colors of its own: the game offers its standard ones.");
    for (int i = 0; i < s.paint_count; ++i) {
        ModPaint &p = s.paints[i];
        const CarEditProblem *bad = problem_for(problems, count, CE_PAINTS, i);
        ImGui::PushID(i);
        float rgb[3] = { p.rgb[0] / 255.0f, p.rgb[1] / 255.0f, p.rgb[2] / 255.0f };
        if (ImGui::ColorEdit3("##rgb", rgb, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel)) {
            const uint8_t next[3] = { (uint8_t)(rgb[0] * 255.0f + 0.5f), (uint8_t)(rgb[1] * 255.0f + 0.5f), (uint8_t)(rgb[2] * 255.0f + 0.5f) };
            car_edit_paint_rgb(&a->edit, i, next);
        }
        ImGui::SameLine();
        char name[sizeof p.name];
        snprintf(name, sizeof name, "%s", p.name);
        ImGui::SetNextItemWidth(150);
        if (bad != nullptr) ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.45f, 0.10f, 0.10f, 1.0f));
        if (ImGui::InputText("##name", name, sizeof name)) car_edit_paint_name(&a->edit, i, name);
        if (bad != nullptr) ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::TextDisabled("#%02x%02x%02x", p.rgb[0], p.rgb[1], p.rgb[2]);
        ImGui::SameLine();
        ImGui::BeginDisabled(i == 0);
        if (ImGui::SmallButton("Up")) car_edit_paint_move(&a->edit, i, -1);
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(i == s.paint_count - 1);
        if (ImGui::SmallButton("Down")) car_edit_paint_move(&a->edit, i, 1);
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::SmallButton("Remove")) car_edit_paint_remove(&a->edit, i);
        ImGui::PopID();
        if (bad != nullptr) problem_text(bad->text);
        if (i >= s.paint_count) break;   // the one just removed
    }
    const bool full = s.paint_count >= MOD_INI_PAINTS_MAX;
    ImGui::BeginDisabled(full);
    if (ImGui::Button("Add color")) {
        char name[sizeof s.paints[0].name];
        snprintf(name, sizeof name, "Color %d", s.paint_count + 1);
        const uint8_t grey[3] = { 128, 128, 128 };
        car_edit_paint_add(&a->edit, name, grey);
    }
    ImGui::EndDisabled();
    if (full && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("A card keeps up to %d colors", MOD_INI_PAINTS_MAX);
}

}   // namespace

void car_shortcuts(App *a)
{
    const ImGuiIO &io = ImGui::GetIO();
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S, ImGuiInputFlags_RouteGlobal)) app_save(a);
    if (io.WantTextInput) return;   // a text box has its own Ctrl+Z while it has the keyboard
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z, ImGuiInputFlags_RouteGlobal) || ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Y, ImGuiInputFlags_RouteGlobal)) car_edit_redo(&a->edit);
    else if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z, ImGuiInputFlags_RouteGlobal)) car_edit_undo(&a->edit);
}

void car_panel(App *a)
{
    if (ImGui::Begin(PANEL_CAR)) {
        const CarProject &c = a->car;
        const ModInfo &m = c.info;
        CarEditProblem problems[CE_PROBLEMS_MAX];
        const int count = car_edit_problems(&a->edit, problems, CE_PROBLEMS_MAX);
        const bool modified = car_edit_modified(&a->edit);
        ImGui::SetWindowFontScale(1.25f);
        ImGui::Text("%s%s", m.name, modified ? " *" : "");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::Text("kit %s   stem %s   id %s", c.kit, m.stem, m.id);
        muted_wrapped("%s", c.root);
        muted_wrapped("Save writes only coche/mod.ini, and only the lines you changed. Every other line and every other file stays as it is.");
        ImGui::Spacing();
        ImGui::BeginDisabled(count > 0 || !modified);
        if (ImGui::Button("Save")) app_save(a);
        ImGui::EndDisabled();
        if (count > 0 && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Fix the %d problem%s below first", count, count == 1 ? "" : "s");
        ImGui::SameLine();
        ImGui::BeginDisabled(!car_edit_can_undo(&a->edit));
        if (ImGui::Button("Undo")) car_edit_undo(&a->edit);
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!car_edit_can_redo(&a->edit));
        if (ImGui::Button("Redo")) car_edit_redo(&a->edit);
        ImGui::EndDisabled();
        if (count > 0) {
            ImGui::PushStyleColor(ImGuiCol_Text, BAD_TEXT);
            ImGui::Text("%d problem%s: Save is blocked", count, count == 1 ? "" : "s");
            ImGui::PopStyleColor();
        } else if (a->status[0] != '\0') {
            muted_wrapped("%s", a->status);
        }
        ImGui::Separator();
        const CarEditProblem *card = problem_for(problems, count, CE_CARD);
        if (card != nullptr) problem_text(card->text);
        if (ImGui::BeginTable("mod.ini", 2, ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("key", ImGuiTableColumnFlags_WidthFixed, 120);
            ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);
            static const char *const YES_NO[] = { "yes", "no" };
            const int fields[] = { CE_VERSION, CE_YEAR, CE_POWER, CE_WEIGHT, CE_MAKE, CE_MODEL, CE_COUNTRY, CE_CATEGORY, CE_AUTHOR, CE_DESCRIPTION };
            for (int f : fields) text_row(a, f, problems, count);
            word_row(a, CE_ABS, "Default (the car's own file)", YES_NO, 2, problems, count);
            word_row(a, CE_TRACTION, "Default (the car's own file)", YES_NO, 2, problems, count);
            const char *words[8];
            int word_count = 0;
            for (int w = 0; w < 8 && car_piece_word_of_model(w) != nullptr; ++w) words[word_count++] = car_piece_word_of_model(w);
            word_row(a, CE_SOUND_FROM, nullptr, words, word_count, problems, count);
            ImGui::EndTable();
        }
        colors_section(a, problems, count);
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
