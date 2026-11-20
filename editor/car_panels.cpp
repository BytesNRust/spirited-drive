// The start screen and the car's panels (panels.h): what Studio shows with nothing open, and what it shows of an open car (read only for now).
#include "panels.h"
#include "help_marker.h"

#include "imgui.h"

extern "C" {
#include "car_export.h"
#include "car_race.h"
#include "formats/car_piece.h"
}

#include <float.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

const char *const PANEL_CAR = "Car", *const PANEL_TEXTURES = "Textures", *const PANEL_MASKS = "Masks", *const PANEL_REHACER = "Rehacer", *const PANEL_RACE = "Race";

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
        if (ImGui::Button("Export original car...", button)) a->export_dialog = 1;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Make a project from one of the game's cars, to adjust its masks");
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
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S, ImGuiInputFlags_RouteGlobal) && !app_rehacer_busy(a)) app_save(a);
    if (app_rehacer_busy(a)) return;   // the script is working on the kit: the card is not edited meanwhile
    if (io.WantTextInput) return;   // a text box has its own Ctrl+Z while it has the keyboard
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z, ImGuiInputFlags_RouteGlobal) || ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Y, ImGuiInputFlags_RouteGlobal)) app_redo(a);
    else if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z, ImGuiInputFlags_RouteGlobal)) app_undo(a);
}

// the facts of a masks project, read only: there is no card to edit
static void masks_project_facts(const App *a)
{
    const CarProject &c = a->car;
    ImGui::SetWindowFontScale(1.25f);
    ImGui::TextUnformatted(c.info.name);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::Text("project %s   masks car_masks/%s", c.kit, c.folder);
    ImGui::SameLine();
    help_marker("There is no card to edit and nothing to Save here: the masks are the PNG files in car_masks/.\n"
                "Adjust them in your picture editor; Reveal masks folder (in Masks) finds them.");
    muted_wrapped("%s", c.root);
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.78f, 0.35f, 1.0f));
    ImGui::TextWrapped("Made from the game's files: for your own use, do not share.");
    ImGui::PopStyleColor();
    ImGui::Spacing();
    if (ImGui::BeginTable("facts", 2, ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("key", ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);
        const char *rows[][2] = { { "kind", "masks (a traffic or police car: it has no specs of its own, so it cannot be driven)" },
                                  { "car file", leaf(c.cfm) },
                                  { "masks", c.has_masks_dir ? "" : "none" } };
        for (const auto &row : rows) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(row[0]);
            ImGui::TableSetColumnIndex(1);
            if (strcmp(row[0], "masks") == 0 && c.has_masks_dir) ImGui::Text("%d in car_masks/%s", c.mask_count, c.folder);
            else ImGui::TextWrapped("%s", row[1]);
        }
        ImGui::EndTable();
    }
}

void car_panel(App *a)
{
    if (ImGui::Begin(PANEL_CAR)) {
        if (a->car.masks_only) {
            masks_project_facts(a);
            ImGui::End();
            return;
        }
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
        ImGui::Spacing();
        const bool running = app_rehacer_busy(a);
        ImGui::BeginDisabled(count > 0 || !modified || running);
        if (ImGui::Button("Save")) app_save(a);
        ImGui::EndDisabled();
        if (count > 0 && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Fix the %d problem%s below first", count, count == 1 ? "" : "s");
        ImGui::SameLine();
        ImGui::BeginDisabled(!car_edit_can_undo(&a->edit) || running);
        if (ImGui::Button("Undo")) car_edit_undo(&a->edit);
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!car_edit_can_redo(&a->edit) || running);
        if (ImGui::Button("Redo")) car_edit_redo(&a->edit);
        ImGui::EndDisabled();
        ImGui::SameLine();
        const bool has_script = app_rehacer_available(a);
        ImGui::BeginDisabled(running || !has_script);
        if (ImGui::Button("Rehacer")) {
            if (modified) ImGui::OpenPopup("Save before Rehacer");   // it reads mod.ini: the changes are saved first
            else app_rehacer(a);
        }
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("%s", running ? "Rehacer is running" : has_script ? "Run this kit's rehacer.command (the kit is backed up first)" : "This kit has no rehacer.command");
        ImGui::SameLine();
        help_marker("Save writes only coche/mod.ini, and only the lines you changed.\nEvery other line and every other file stays as it is.");
        if (ImGui::BeginPopupModal("Save before Rehacer", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("mod.ini has changes that are not saved. Rehacer reads the card,");
            ImGui::TextUnformatted("so they are saved first.");
            if (count > 0) problem_text("A field has a problem: fix it first, Save is blocked.");
            ImGui::Spacing();
            ImGui::BeginDisabled(count > 0);
            if (ImGui::Button("Save and run") || (count == 0 && ImGui::IsKeyPressed(ImGuiKey_Enter, false))) {
                if (app_save(a)) app_rehacer(a);
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
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
        ImGui::BeginDisabled(running);   // the card is not edited while the script works on the kit
        if (ImGui::BeginTable("mod.ini", 2, ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("key", ImGuiTableColumnFlags_WidthFixed, 120);
            ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);
            static const char *const YES_NO[] = { "yes", "no" };
            const int fields[] = { CE_VERSION, CE_YEAR, CE_POWER, CE_WEIGHT, CE_MAKE, CE_MODEL, CE_COUNTRY, CE_CATEGORY, CE_AUTHOR, CE_DESCRIPTION };
            for (int f : fields) text_row(a, f, problems, count);
            word_row(a, CE_ABS, "Default (the car's own file)", YES_NO, 2, problems, count);
            word_row(a, CE_TRACTION, "Default (the car's own file)", YES_NO, 2, problems, count);
            const char *words[16];
            const int word_count = car_edit_sound_words(words, 16);
            word_row(a, CE_SOUND_FROM, nullptr, words, word_count, problems, count);
            ImGui::EndTable();
        }
        colors_section(a, problems, count);
        ImGui::EndDisabled();
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

static ImGuiID masks_dock = 0;   // the dock node Masks is in, where a layout kept from before Rehacer puts it as a tab

void masks_panel(App *a)
{
    static char shown[APP_PATH] = "";   // the project whose Masks tab was brought forward: a car just opened shows its masks, not the script's empty log
    static int wait = 0;                // frames to let the dock layout settle first: a focus given in the frame that builds it is lost
    if (strcmp(shown, a->dir) != 0) {
        snprintf(shown, sizeof shown, "%s", a->dir);
        wait = 3;
    }
    if (wait > 0 && --wait == 0 && !app_rehacer_busy(a) && !a->rehacer.run.finished) ImGui::SetNextWindowFocus();   // a run's log is what to look at, then
    const bool open = ImGui::Begin(PANEL_MASKS);
    masks_dock = ImGui::GetWindowDockID();
    if (open) {
        const CarProject &c = a->car;
        const bool original = car_race_original_of(&c) != nullptr;   // an exported original car (or its masks): one of the game's own
        ImGui::Text("Car file: %s", leaf(c.cfm));
        masks_changes_section(a);   // what the project's masks have changed from the game's: first, it is what a hand-over needs
        ImGui::Spacing();
        if (!c.has_masks_dir) {
            ImGui::Text("Masks of the car (car_masks/%s): none yet", c.folder);
        } else {
            ImGui::Text("Masks of the car (car_masks/%s): %d", c.folder, c.mask_count);
            name_list(c.mask, c.mask_count);
            ImGui::Spacing();
            if (ImGui::Button("Reveal masks folder")) snprintf(a->reveal, sizeof a->reveal, "%s/car_masks/%s", c.piece, c.folder);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Show the folder in the Finder");
            if (original) {   // only one of the game's own cars has its masks in the game's source tree; a mod car's stay in its mod
                char repo[256];
                car_export_repo_path(c.folder, repo, sizeof repo);
                ImGui::SameLine();
                char help[320];
                snprintf(help, sizeof help, "In the game's source tree these files are %s:\ncopy them there to change the game's masks.", repo);
                help_marker(help);
            }
        }
        ImGui::Spacing();
        if (c.kit_mask_count > 0) {
            ImGui::Text("Your colour masks for an external image editor (mascaras/): %d", c.kit_mask_count);
            name_list(c.kit_mask, c.kit_mask_count);
        } else if (!c.masks_only && !original) {   // the kit's folder of colour PNGs, not the grey masks above: an exported original has no kit
            muted_wrapped("No colour masks for an external image editor (mascaras/)");
            ImGui::SameLine();
            char help[320];
            snprintf(help, sizeof help, "That folder of the kit holds colour PNGs painted in an external image editor.\nEdit mask writes the grey masks above, in car_masks/%s/.", c.folder);
            help_marker(help);
        }
    }
    ImGui::End();
}

// Export original car: the cars of the game to choose from (the unavailable ones greyed, with why), then a folder to make the project in
void export_popup(App *a)
{
    static const char *const NAME = "Export original car";
    static bool known = false;
    static int usable[EXPORT_CARS];
    static char why[EXPORT_CARS][256];
    if (a->export_dialog && !ImGui::IsPopupOpen(NAME)) {
        ImGui::OpenPopup(NAME);
        known = false;
    }
    if (!known && a->export_dialog) {   // asked once when the window comes up: it reads folders
        for (int i = 0; i < EXPORT_CARS; ++i) usable[i] = car_export_available(&a->export_src, i, why[i], sizeof why[i]);
        known = true;
    }
    ImGui::SetNextWindowSize(ImVec2(540, 520), ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (!ImGui::BeginPopupModal(NAME, nullptr, ImGuiWindowFlags_NoSavedSettings)) return;
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.78f, 0.35f, 1.0f));
    ImGui::TextWrapped("Made from the game's files: for your own use, do not share.");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    help_marker("Make a project from one of the game's cars, to adjust its masks.\nThe project holds the masks the game has now, with the same names and sizes.");
    muted_wrapped("Reading the game's files from %s and its masks from %s", a->export_src.cd, a->export_src.data);
    ImGui::Spacing();
    const float footer = ImGui::GetFrameHeightWithSpacing() * 2 + 6;
    if (ImGui::BeginChild("cars", ImVec2(0, -footer), ImGuiChildFlags_Borders)) {
        static const char *const HEADS[] = { "Cars you drive: a normal car project", "Traffic cars: a masks project (they have no specs, only masks)", "Police car: a masks project" };
        int last = -1;
        for (int i = 0; i < EXPORT_CARS; ++i) {
            const ExportCar *car = car_export_car(i);
            if ((int)car->kind != last) {
                last = (int)car->kind;
                if (i > 0) ImGui::Spacing();
                ImGui::SeparatorText(HEADS[last]);
            }
            char label[96];
            snprintf(label, sizeof label, "%s  (%s)", car->label, car->folder);
            ImGui::PushID(i);
            ImGui::BeginDisabled(!usable[i]);
            if (ImGui::Selectable(label, a->export_pick == i)) a->export_pick = i;
            ImGui::EndDisabled();
            if (!usable[i] && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("%s", why[i]);
            if (!usable[i]) {
                ImGui::SameLine();
                ImGui::TextDisabled("not available");
            }
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
    const bool picked = a->export_pick >= 0 && a->export_pick < EXPORT_CARS && usable[a->export_pick];
    if (picked) {
        char name[96];
        car_export_folder_name(a->export_pick, name, sizeof name);
        muted_wrapped("You choose a folder next; the project is made in it, in a folder named %s.", name);
    } else {
        muted_wrapped("Choose a car.");
    }
    ImGui::BeginDisabled(!picked || a->go != APP_ACT_NONE || a->confirm != APP_ACT_NONE || app_rehacer_busy(a));
    if (ImGui::Button("Choose folder and export...")) a->go = APP_ACT_EXPORT_PICK;
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        a->export_dialog = 0;
        ImGui::CloseCurrentPopup();
    }
    if (!a->export_dialog) ImGui::CloseCurrentPopup();   // the project was made and opened
    ImGui::EndPopup();
}

// R2: the game's install said the files changed but the version did not. The button raises the card's version one step, saves and runs again; nothing is
// raised without it.
static void raise_version_offer(App *a)
{
    if (!rehacer_version_unraised(a->rehacer.run.log) || a->rehacer.run.stopped) return;
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, BAD_TEXT);
    ImGui::TextWrapped("The game installed it, but its files changed and its version did not. Raise the version so the new car can be told from the old one.");
    ImGui::PopStyleColor();
    char next[MOD_INI_VERSION_MAX + 1], label[64];
    if (app_rehacer_raise_offer(a, next, sizeof next)) {
        snprintf(label, sizeof label, "Raise version to %s and run again", next);
        if (ImGui::Button(label)) app_rehacer_raise_and_run(a);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("Sets Version in the Car tab to %s, saves mod.ini and runs Rehacer again (undo takes the version back)", next);
    } else if (car_edit_modified(&a->edit)) {
        muted_wrapped("Save the card's changes first: then the version can be raised here.");
    } else {
        muted_wrapped("Version \"%s\" is not numbers like 1.2.3: raise it by hand in the Car tab, save, and run Rehacer again.", a->edit.now.value[CE_VERSION]);
    }
}

void rehacer_panel(App *a)
{
    static int shown = 0;   // the run whose tab was brought forward
    if (masks_dock != 0) ImGui::SetNextWindowDockID(masks_dock, ImGuiCond_FirstUseEver);
    if (app_rehacer_busy(a) && a->rehacer.run_id != shown) {   // a run was asked for: its tab comes forward at once, while the kit is still copied
        shown = a->rehacer.run_id;
        ImGui::SetNextWindowFocus();
    }
    if (ImGui::Begin(PANEL_REHACER)) {
        const Rehacer &r = a->rehacer;
        ImGui::TextUnformatted("Kit script: rehacer.command");
        ImGui::SameLine();
        char help[240];
        snprintf(help, sizeof help, "Runs this kit's rehacer.command with the kit as its folder (Rehacer, in the Car panel).\n"
                 "Before it runs, Studio copies the kit into .studio-backup/<time>/ (the last %d are kept).", REHACER_KEEP);
        help_marker(help);
        if (!app_rehacer_available(a)) {
            ImGui::Spacing();
            ImGui::TextUnformatted("This kit has no rehacer.command.");
        }
        if (r.backing_up) {
            ImGui::Spacing();
            ImGui::TextUnformatted("Backing up the kit, then the script runs...");
            ImGui::SameLine();
            if (ImGui::Button("Stop")) app_rehacer_stop(a);
        } else if (r.run.running) {
            ImGui::Spacing();
            ImGui::TextUnformatted("Running...");
            ImGui::SameLine();
            if (ImGui::Button("Stop")) app_rehacer_stop(a);
        } else if (r.failed) {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, BAD_TEXT);
            ImGui::TextWrapped("Did not run: %s", r.why);
            ImGui::PopStyleColor();
        } else if (r.run.finished) {
            ImGui::Spacing();
            const ImVec4 colour = r.run.exit_code == 0 ? ImVec4(0.47f, 0.86f, 0.55f, 1.0f) : BAD_TEXT;
            ImGui::PushStyleColor(ImGuiCol_Text, colour);
            ImGui::Text("%s: exit code %d", r.run.stopped ? "Stopped" : r.run.exit_code == 0 ? "Finished" : "Failed", r.run.exit_code);
            ImGui::PopStyleColor();
            raise_version_offer(a);
        }
        if (r.backup[0] != '\0') {
            const char *slash = strrchr(r.backup, '/');
            muted_wrapped("Backup: %s/%s", REHACER_BACKUPS, slash != nullptr ? slash + 1 : r.backup);
        }
        ImGui::Separator();
        if (ImGui::BeginChild("log", ImVec2(0, 0), ImGuiChildFlags_None)) {
            const bool at_bottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.0f;
            if (r.run.log_cut) ImGui::TextDisabled("(the start of the output was dropped)");
            if (r.run.log != nullptr && r.run.log_len > 0) ImGui::TextUnformatted(r.run.log, r.run.log + r.run.log_len);
            else if (!app_rehacer_busy(a) && !r.run.finished && !r.failed) ImGui::TextDisabled("Nothing has run yet.");
            if (r.run.running && at_bottom) ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();
    }
    ImGui::End();
}

namespace {

// the last lines of the game's output, errors red and the masks it took green
void race_log_lines(const char *text, size_t len, int *budget, bool show_notes)
{
    size_t start = 0;
    for (size_t at = len; at > 0 && *budget > 0; --at) {   // the last `budget` lines
        if (text[at - 1] == '\n' && at != len && --*budget == 0) {
            start = at;
            break;
        }
    }
    const char *p = text + start, *end = text + len;
    while (p < end) {
        const char *nl = static_cast<const char *>(memchr(p, '\n', static_cast<size_t>(end - p)));
        const char *line_end = nl != nullptr ? nl : end;
        const RaceLineKind kind = car_race_line_kind(p, static_cast<size_t>(line_end - p));
        if (kind == RACE_LINE_NOTE && !show_notes) {   // collapsed into the summary line
            p = nl != nullptr ? nl + 1 : end;
            continue;
        }
        if (kind == RACE_LINE_ERROR) ImGui::PushStyleColor(ImGuiCol_Text, BAD_TEXT);
        else if (kind == RACE_LINE_MASK) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.47f, 0.86f, 0.55f, 1.0f));
        else if (kind == RACE_LINE_NOTE) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextUnformatted(p, line_end);
        if (kind != RACE_LINE_PLAIN) ImGui::PopStyleColor();
        p = nl != nullptr ? nl + 1 : end;
    }
}

}   // namespace

// Test in race and Race photo of an original car's project: the preset, the track, the buttons, the look, the picture, and the game's log
void race_panel(App *a, ImTextureID photo)
{
    static int seen = 0, wait = 0;   // a new photo brings the tab forward (after the dock layout has settled: a focus given in the frame that builds it is lost)
    if (a->race.photo_serial != seen) {
        seen = a->race.photo_serial;
        wait = 3;
    }
    if (wait > 0 && --wait == 0) ImGui::SetNextWindowFocus();
    if (ImGui::Begin(PANEL_RACE)) {
        const ExportCar *car = car_race_original_of(&a->car);
        const bool busy = app_race_busy(a);
        const bool traffic = car != nullptr && car->kind != EXPORT_DRIVABLE;
        if (car == nullptr) {
            ImGui::TextDisabled("Test in race: the game's own cars only");
            ImGui::SameLine();
            help_marker("Test in race runs the game with this project's masks in front of its own, so it needs a project of one of the game's cars. "
                        "Make one with Export original car.");
        } else {
            ImGui::BeginDisabled(busy);
            ImGui::TextUnformatted("Time");
            ImGui::SameLine();
            if (ImGui::RadioButton("Day", a->race_night == 0)) a->race_night = 0;
            ImGui::SameLine();
            if (ImGui::RadioButton("Night", a->race_night == 1)) a->race_night = 1;
            ImGui::SameLine();
            ImGui::TextUnformatted("  Track");
            ImGui::SameLine();
            const RaceTrack *now = car_race_track(a->race_track);
            ImGui::SetNextItemWidth(190);
            if (ImGui::BeginCombo("##track", now != nullptr ? now->label : "")) {
                for (int i = 0; i < car_race_track_count(); ++i) {
                    const RaceTrack *tr = car_race_track(i);
                    const bool off = traffic && !tr->stage;   // the circuits have no traffic
                    ImGui::BeginDisabled(off);
                    if (ImGui::Selectable(tr->label, a->race_track == i)) a->race_track = i;
                    ImGui::EndDisabled();
                    if (off && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("A circuit has no traffic");
                }
                ImGui::EndCombo();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("The night is each track's own; Rusty Springs is the reference");
            if (a->race_night && now != nullptr && strcmp(now->stem, "TR7") == 0) ImGui::TextDisabled("Lost Vegas has no time of day: it stays as it is");
            ImGui::EndDisabled();
            char note[500];   // Test in race: who drives what, and where a traffic car is
            car_race_note(car, now != nullptr ? now->stem : "", note, sizeof note);
            char help[800];   // the detail, behind the (?) beside the buttons: no loose explanation in the panel
            const int n = snprintf(help, sizeof help, "The game runs on a scratch player folder, with this project's masks.%s%s%s",
                                   traffic ? " A traffic or police car: City, Coastal or Alpine stages only." : "", note[0] != '\0' ? "\n\nTest in race: " : "", note);
            if (n < 0 || static_cast<size_t>(n) >= sizeof help) snprintf(help + sizeof help - 4, 4, "...");
            ImGui::BeginDisabled(busy);
            if (ImGui::Button("Test in race")) app_race(a, 0);
            ImGui::SameLine();
            if (ImGui::Button("Race photo")) app_race(a, 1);
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(!busy);
            if (ImGui::Button("Stop")) app_race_stop(a);
            ImGui::EndDisabled();
            ImGui::SameLine();
            help_marker(help);
            ImGui::BeginDisabled(busy);
            race_look_controls(&a->race_look);
            ImGui::EndDisabled();
        }
        if (busy) ImGui::TextUnformatted("Running...");
        else if (a->race.summary[0] != '\0') {
            if (a->race.ok) muted_wrapped("%s", a->race.summary);
            else problem_text(a->race.summary);
        }
        if (a->race.photo != nullptr && photo != 0) {   // the picture, as big as the window leaves room for beside the log's few lines
            const ImVec2 room = ImGui::GetContentRegionAvail();
            const float pw = static_cast<float>(a->race.photo_w), ph = static_cast<float>(a->race.photo_h);   // 640 x 480, or 1280 x 960
            float w = room.x < pw ? room.x : pw;
            const float high = room.y - 90.0f > 120.0f ? room.y - 90.0f : 120.0f;
            if (w * ph / pw > high) w = high * pw / ph;
            ImGui::Image(photo, ImVec2(w, w * ph / pw));
        }
        ImGui::SeparatorText("The game's log");
        if (ImGui::BeginChild("racelog", ImVec2(0, 0), ImGuiChildFlags_Borders)) {
            const RaceJob &j = a->race;
            static bool show_notes = false;
            const int notes = (j.log != nullptr ? car_race_count_notes(j.log, j.log_len) : 0) + (j.run.log != nullptr ? car_race_count_notes(j.run.log, j.run.log_len) : 0);
            if (notes > 0) {   // the game's "cannot read ..._mask.png" for each car the project has no mask for: one dim line, expandable
                ImGui::TextDisabled("%d mask%s not in this project: the game uses its own", notes, notes == 1 ? "" : "s");
                ImGui::SameLine();
                if (ImGui::SmallButton(show_notes ? "Hide" : "Show")) show_notes = !show_notes;
            }
            int before = 250, now_lines = 150;   // the runs that ended, then the one that runs
            if (j.log != nullptr && j.log_len > 0) race_log_lines(j.log, j.log_len, &before, show_notes);
            else if (j.run.log == nullptr || j.run.log_len == 0) ImGui::TextDisabled("Nothing has run yet.");
            if (j.run.log != nullptr && j.run.log_len > 0) race_log_lines(j.run.log, j.run.log_len, &now_lines, show_notes);
        }
        ImGui::EndChild();
    }
    ImGui::End();
}
