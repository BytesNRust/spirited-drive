// The start screen and the car's panels (panels.h): what Studio shows with nothing open, and what it shows of an open car (read only for now).
#include "panels.h"
#include "help_marker.h"
#include "ui_probe.h"
#include "physics_popup.h"
#include "path_elide.h"
#include "sheet_ui.h"
#include "ui_probe.h"

#include "imgui.h"

extern "C" {
#include "car_export.h"
#include "command_keys.h"
#include "car_race.h"
#include "app_dash.h"
#include "app_sheet.h"
#include "car_photos.h"
#include "formats/car_piece.h"
#include "formats/car_paint.h"
}

#include <float.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

const char *const PANEL_CAR = "Car", *const PANEL_MASKS = "Masks", *const PANEL_RACE = "Drive###Race";   // shown "Drive"; the ### id keeps it apart from the track editor's Drive, which shares the ini (ImGui keys a window's settings by the part after ###: a layout saved with the old "Race" window keeps this one in its dock, checked by hand with an ini the old build wrote)

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

}   // namespace

// The workspace row of the main menu: the folder with a Change... button, or the one line offering to choose it (a (?), and Not now: put aside, never shown again)
static void workspace_row(App *a)
{
    if (a->workspace[0] == '\0') {
        if (!app_workspace_offer(a)) return;
        ImGui::Dummy(ImVec2(0, 8));
        if (ImGui::Button("Choose workspace...")) app_workspace_pick(a);
        ImGui::SameLine();
        help_marker("A workspace is one folder with your cars and tracks in it (cars/ and tracks/ inside).\nOpen and New start there and this screen lists what it holds.\nYou can also set it later in File > Workspace.");
        ImGui::SameLine();
        if (ImGui::SmallButton("Not now")) app_workspace_put_aside(a);
        return;
    }
    ImGui::Dummy(ImVec2(0, 8));
    ImGui::TextDisabled("Workspace");
    ImGui::SameLine();
    const float room = ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Change...").x - 3 * ImGui::GetStyle().ItemSpacing.x - 2 * ImGui::GetStyle().FramePadding.x;
    const float glyph = ImGui::CalcTextSize("M").x;
    char shown[APP_PATH];
    path_elide(a->workspace, glyph > 0 ? (int)(room / glyph) : 40, shown, sizeof shown);   // the whole path, cut in the middle when it does not fit; the tooltip has all of it
    ImGui::TextUnformatted(shown);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", a->workspace);
    ImGui::SameLine();
    if (ImGui::SmallButton("Change...")) app_workspace_pick(a);
}

// the projects of the workspace of one kind (0 cars, 1 tracks); a click opens one, by its own kind (the recent list knows only what was opened before)
static void workspace_list(App *a, int kind, float width)
{
    if (a->workspace[0] == '\0') return;
    const int count = kind == 0 ? a->workspace_list.cars : a->workspace_list.tracks;
    if (count == 0) muted_wrapped("none yet");
    for (int i = 0; i < count; ++i) {
        const char *name = kind == 0 ? a->workspace_list.car[i] : a->workspace_list.track[i];
        char path[APP_PATH];
        ImGui::PushID(kind * WORKSPACE_LIST + i);
        if (ImGui::Selectable(name, false, 0, ImVec2(width, 0)) && app_workspace_item(a, kind, i, path, sizeof path)) app_ask(a, kind == 0 ? APP_ACT_OPEN_CAR_AT : APP_ACT_OPEN_AT, path);
        ImGui::PopID();
    }
}

// the recent projects of one kind: those inside the workspace (all of them with none), each removable with a right click, and a button that clears the kind
static void recent_list(App *a, int kind, float width)
{
    int shown = 0;
    int remove = -1;
    for (int i = 0; i < a->recent.count; ++i) {
        if (a->recent.kind[i] != kind || !recent_inside(a->recent.dir[i], a->workspace)) continue;
        ++shown;
        const bool there = recent_present(a->recent.dir[i], a->recent.kind[i]);
        ImGui::PushID(1000 + i);
        ImGui::BeginDisabled(!there);   // a folder that is gone is greyed
        if (ImGui::Selectable(leaf(a->recent.dir[i]), false, 0, ImVec2(width, 0))) app_ask(a, APP_ACT_RECENT, a->recent.dir[i]);
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("%s%s\nRight click: remove from Recent", a->recent.dir[i], there ? "" : "\nnot found: the folder was moved or deleted");
        if (ImGui::BeginPopupContextItem("recent item")) {
            if (ImGui::MenuItem("Remove from Recent")) remove = i;
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
    if (remove >= 0) app_recent_remove(a, remove);
    if (shown == 0) muted_wrapped("Nothing opened yet.");
    else if (ImGui::SmallButton(kind == RECENT_CAR ? "Clear recent cars" : "Clear recent tracks")) app_recent_clear(a, kind);
}

void start_screen(App *a)
{
    const ImGuiViewport *vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus;
    if (ImGui::Begin("Start", nullptr, flags)) {
        const float width = ImGui::GetContentRegionAvail().x, total = width < 800.0f ? width : 800.0f;
        ImGui::SetCursorPosX((width - total) / 2 + ImGui::GetStyle().WindowPadding.x);
        ImGui::BeginGroup();
        ImGui::Dummy(ImVec2(0, 18));
        ImGui::SetWindowFontScale(1.6f);
        ImGui::TextUnformatted("Spirited Drive Studio");
        ImGui::SetWindowFontScale(1.0f);
        muted_wrapped("Make cars and tracks for Spirited Drive.");
        workspace_row(a);
        ImGui::Dummy(ImVec2(0, 10));
        // two rooms behind this one door: Cars and Tracks, each with its own actions, what the workspace holds and what was opened lately
        if (ImGui::BeginTable("rooms", 2, ImGuiTableFlags_SizingStretchSame, ImVec2(total, 0))) {
            const float half = total / 2 - 16;
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();   // the same baseline in both columns
            ImGui::SetWindowFontScale(1.25f);
            ImGui::TextUnformatted("Cars");
            ImGui::SetWindowFontScale(1.0f);
            ImGui::BeginDisabled(a->workspace[0] == '\0');
            if (ImGui::Button("New car...", ImVec2(half, 0))) app_ask(a, APP_ACT_NEW_CAR, "");
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Start a car as a copy of one of the eight you drive; it goes in the workspace's cars/ folder");
            if (ImGui::Button("Open car...", ImVec2(half, 0))) app_ask(a, APP_ACT_OPEN_CAR, "");
            if (ImGui::Button("Export original car...", ImVec2(half, 0))) a->export_dialog = 1;
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Make a project from one of the game's cars, to adjust its masks");
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();   // the same baseline in both columns
            ImGui::SetWindowFontScale(1.25f);
            ImGui::TextUnformatted("Tracks");
            ImGui::SetWindowFontScale(1.0f);
            if (ImGui::Button("New track...", ImVec2(half, 0))) app_ask(a, APP_ACT_NEW, "");
            if (ImGui::Button("Open track...", ImVec2(half, 0))) app_ask(a, APP_ACT_OPEN, "");
            ImGui::BeginDisabled(a->light_dir[0] == '\0');
            if (ImGui::Button("Open original track...", ImVec2(half, 0))) a->original_dialog = 1;
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Tune a CD track's light at dawn, day, dusk and night, with a photo from the game%s", a->light_dir[0] == '\0' ? "\n(Choose File > Game repo folder... first.)" : "");
            ImGui::BeginDisabled(a->workspace[0] == '\0' || !a->data_dir_ok);
            if (ImGui::Button("Add originals", ImVec2(half, 0))) app_original_add_all(a);
            ui_probe("add_originals");
            ImGui::EndDisabled();
            char missing[APP_PATH + 64];   // (the path that failed, shown in the tooltip)
            snprintf(missing, sizeof missing, "\n(Game data folder not found: %s)", a->data_dir);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("Put the game's 16 tracks in the workspace: their light file and their mask PSDs. Run it again to bring an older workspace up to date: the one light file replaces\nthe old light files, and whatever you already edited (light, PSDs, masks) is never touched.%s",
                                  a->workspace[0] == '\0' ? "\n(Choose a workspace first: File > Workspace.)" : a->data_dir_ok ? "" : a->data_dir[0] == '\0' ? "\n(Choose File > Game repo folder... first: Studio needs the game's data/ to copy the light files from.)" : missing);
            ImGui::BeginDisabled(a->workspace[0] == '\0');
            if (ImGui::Button("Export original track...", ImVec2(half, 0))) app_ask(a, APP_ACT_EXPORT_TRACK, "");
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Start a new track that follows the road of one of the CD's circuits; it goes in the workspace's tracks/ folder");
            ImGui::EndTable();
        }
        // what the workspace holds and what was opened lately: each heading once across both rooms, the lists in the columns of their room
        const float half = total / 2 - 16;
        if (a->workspace[0] != '\0') {
            ImGui::SeparatorText("In the workspace");
            if (ImGui::BeginTable("in the workspace", 2, ImGuiTableFlags_SizingStretchSame, ImVec2(total, 0))) {
                ImGui::TableNextColumn();
                workspace_list(a, 0, half);
                ImGui::TableNextColumn();
                workspace_list(a, 1, half);
                ImGui::EndTable();
            }
        }
        ImGui::SeparatorText("Recent");
        if (ImGui::BeginTable("recent", 2, ImGuiTableFlags_SizingStretchSame, ImVec2(total, 0))) {
            ImGui::TableNextColumn();
            recent_list(a, RECENT_CAR, half);
            ImGui::TableNextColumn();
            recent_list(a, RECENT_TRACK, half);
            ImGui::EndTable();
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
    ui_probe_scope("card"), ui_probe(car_edit_label(field)), ui_probe_scope("");   // the card row by its label ("card_Name")
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

// the Showcase photos: the pictures the game's Car Showcase shows for this car, in order (the card's photos= line, an undo step each)
void photos_section(App *a, const CarEditProblem *problems, int count)
{
    char names[MOD_INI_PHOTOS_MAX][MOD_INI_PHOTO_NAME_MAX + 1];
    const int n = car_photos_split(a->edit.now.value[CE_PHOTOS], names, MOD_INI_PHOTOS_MAX);
    char photos_dir[APP_PATH];
    app_photos_dir(a, photos_dir, sizeof photos_dir);
    ImGui::PushID("showcase_photos");   // the rows' ids must not meet the Colors rows' (both number their rows from 0)
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Showcase photos");
    ImGui::SameLine();
    help_marker("The pictures of this car in the game's Car Showcase, in this order. Add photo... copies a PNG (100x60 to 1280x960; 450x254 fits the page) into the car's folder "
                "under a safe name and lists it on the card (photos=). Save writes the line, and removes the copies you took off the list.");
    ImGui::Separator();
    if (n == 0) {
        muted_wrapped("No photos");
        ImGui::SameLine();
        help_marker("With no showcase photos the game's Car Showcase shows only the card photo.");
    }
    for (int i = 0; i < n; ++i) {
        uint32_t w = 0, h = 0;
        const bool there = car_photos_size(photos_dir, names[i], &w, &h) != 0;
        ImGui::PushID(i);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(names[i]);
        ImGui::SameLine();
        if (there) ImGui::TextDisabled("%ux%u", (unsigned)w, (unsigned)h);
        else {
            ImGui::TextColored(BAD_TEXT, "missing");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("The car's folder has no PNG of this name");
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(i == 0);
        if (ImGui::SmallButton("Up")) app_photo_move(a, i, -1);
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(i == n - 1);
        if (ImGui::SmallButton("Down")) app_photo_move(a, i, 1);
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::SmallButton("Remove")) app_photo_remove(a, i);
        ImGui::PopID();
    }
    const CarEditProblem *bad = problem_for(problems, count, CE_PHOTOS);
    if (bad != nullptr) problem_text(bad->text);
    const bool full = n >= MOD_INI_PHOTOS_MAX;
    ImGui::BeginDisabled(full);
    if (ImGui::Button("Add photo...")) a->photo_dialog = 1;
    ImGui::EndDisabled();
    if (full && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("A card keeps up to %d photos", MOD_INI_PHOTOS_MAX);
    ImGui::PopID();
}

void colors_section(App *a, const CarEditProblem *problems, int count)
{
    CarEditState &s = a->edit.now;
    ImGui::PushID("colors");
    ImGui::SeparatorText("Colors");
    if (s.paint_count == 0) {
        muted_wrapped("No colors of its own");
        ImGui::SameLine();
        help_marker("A car with no colors of its own is offered the game's standard ones.");
    }
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
        { char scope[16]; snprintf(scope, sizeof scope, "colour%d", i); ui_probe_scope(scope); ui_probe("name"); }
        if (bad != nullptr) ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::TextDisabled("#%02x%02x%02x", p.rgb[0], p.rgb[1], p.rgb[2]);
        ImGui::SameLine();
        ImGui::BeginDisabled(i == 0);
        if (ImGui::SmallButton("Up")) car_edit_paint_move(&a->edit, i, -1);
        ui_probe("up");
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(i == s.paint_count - 1);
        if (ImGui::SmallButton("Down")) car_edit_paint_move(&a->edit, i, 1);
        ui_probe("down");
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::SmallButton("Remove")) car_edit_paint_remove(&a->edit, i);
        ui_probe("remove");
        ui_probe_scope("");
        ImGui::PopID();
        if (bad != nullptr) problem_text(bad->text);
        if (i >= s.paint_count) break;   // the one just removed
    }
    const int most = app_car_is_original(a) ? CAR_PAINT_MAX : MOD_INI_PAINTS_MAX;   // the game offers up to 8 colors for one of its own cars
    const bool full = s.paint_count >= most;
    ImGui::BeginDisabled(full);
    const bool add_colour = ImGui::Button("Add color");
    ui_probe("add_color");
    if (add_colour) {
        char name[sizeof s.paints[0].name];
        snprintf(name, sizeof name, "Color %d", s.paint_count + 1);
        const uint8_t grey[3] = { 128, 128, 128 };
        car_edit_paint_add(&a->edit, name, grey);
    }
    ImGui::EndDisabled();
    if (full && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip(most < MOD_INI_PAINTS_MAX ? "The game offers up to %d colors for one of its own cars" : "A card keeps up to %d colors", most);
    ImGui::PopID();
}

}   // namespace

void car_shortcuts(App *a)
{
    const CommandKey key = command_key(ImGui::GetIO().WantTextInput);   // a text box has its own Cmd+Z while it has the keyboard
    if (key == COMMAND_SAVE) app_save(a);
    else if (key == COMMAND_UNDO) app_undo(a);   // the car's one history: the newest change, the card's or the mask's
    else if (key == COMMAND_REDO) app_redo(a);
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
                "Adjust them in your picture editor; Show PSD folder (in Masks) finds them.");
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
            if (strcmp(row[0], "masks") == 0 && c.has_masks_dir) {
                char rel[128];
                car_project_masks_rel(&c, rel, sizeof rel);
                ImGui::Text("%d in %s", c.mask_count, rel);
            }
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
        if (app_car_is_kit(a)) {   // a kit in the current layout: its car is made from its sources, then installed in the game
            const bool own = app_car_is_original(a);   // the game's own car: nothing to build or install
            static const char *const OWN = "An original car is the game's own: retouch its masks and copy them to the game repo; it is not installed";
            ImGui::BeginDisabled(own || app_car_building(a) || app_install_busy(a));
            if (ImGui::Button("Build")) app_build(a);
            ui_probe("build");
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("%s", own ? OWN : app_car_building(a) ? "The car is being built" : "Make the car from this kit's files: build/piece and build/<id>.zip (save what you changed first)");
            ImGui::SameLine();
            if (own) {   // the game's own car is not installed: what goes on is a copy of its masks and colors into the game repo
                const bool repo_set = app_repo_data(a)[0] != '\0';
                ImGui::BeginDisabled(repo_set && !app_car_copy_can(a));   // with no repo folder the button stays on: a click asks for it
                const bool copy_now = ImGui::Button("Copy to game repo");
                ui_probe("copy_repo");
                if (copy_now) repo_set ? (void)app_car_copy_ask(a) : (void)app_repo_data_pick(a);
                ImGui::EndDisabled();
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                    ImGui::SetTooltip("%s", !repo_set ? "Set the game repo folder first: File > Game repo folder..."
                                                     : "Copy this car's changed masks into the game repo's data/ and its Colors into core/formats/car_paint.c (nothing is committed); the window lists what goes");
            } else {
                ImGui::BeginDisabled(app_car_building(a) || app_install_busy(a));
                if (ImGui::Button("Install")) app_install(a);
                ImGui::EndDisabled();
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                    ImGui::SetTooltip("%s", app_install_busy(a) ? "The game is installing the package" : "Install the last build in the game (Build first)");
            }
            ImGui::SameLine();
            ImGui::BeginDisabled(app_car_building(a) || app_install_busy(a));
            if (ImGui::Button("Check")) app_check(a);
            ui_probe("check");
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("%s", app_car_building(a) ? "The car is being built or checked" : "What is wrong with this kit before a build: the model, its wheels, its masks (save what you changed first)");
            ImGui::SameLine();
        }
        help_marker("File > Save (Cmd+S) writes only mod.ini, and only the lines you changed.\nEvery other line and every other file stays as it is; the showcase photos you add are copied into the folder, and Save removes the copies you took off the list.");
        if (a->cdisk.card_changed) {   // another program wrote mod.ini while the card has unsaved changes here
            ImGui::TextColored(ImVec4(0.95f, 0.78f, 0.35f, 1.0f), "Changed on disk");
            ImGui::SameLine();
            if (ImGui::Button("Reload")) app_card_reload(a);
            ImGui::SameLine();
            if (ImGui::Button("Keep mine")) app_card_keep_mine(a);
            ImGui::SameLine();
            help_marker("Another program wrote mod.ini while the card has unsaved changes here. Reload takes the file's card and drops the changes; "
                        "Keep mine keeps them, and Save writes them over the file.");
        }
        if (count > 0) {
            ImGui::PushStyleColor(ImGuiCol_Text, BAD_TEXT);
            ImGui::Text("%d problem%s: Save is blocked", count, count == 1 ? "" : "s");
            ImGui::PopStyleColor();
        } else if (a->status[0] != '\0') {
            muted_wrapped("%s", a->status);
        }
        if (app_car_is_kit(a) && a->car_build.checked) {   // the kit's check: a line a finding, red for an error, amber for a warning
            for (size_t i = 0; i < a->car_build.list.count; i++) {
                const CarCheckItem &f = a->car_build.list.item[i];
                ImGui::PushStyleColor(ImGuiCol_Text, f.severity == CAR_CHECK_ERROR ? BAD_TEXT : ImVec4(0.95f, 0.78f, 0.35f, 1.0f));
                ImGui::TextWrapped("%s: %s", f.severity == CAR_CHECK_ERROR ? "error" : "warning", f.text);
                ImGui::PopStyleColor();
            }
        }
        ImGui::Separator();
        const CarEditProblem *card = problem_for(problems, count, CE_CARD);
        if (card != nullptr) problem_text(card->text);
        const bool own_card = app_car_is_original(a);   // the game's own car: its card is the game's, only the Colors go to the game repo
        if (own_card) {
            ImGui::TextUnformatted("Card: the game's own");
            ImGui::SameLine();
            help_marker("An original car's card is the game's own: it is shown, not edited, and it is not installed. Only its Colors travel: Copy to game repo writes them into this car's row of the colors table "
                        "(core/formats/car_paint.c) of the game repo; the game then offers them in its Color box.");
        }
        ImGui::BeginDisabled(own_card);
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
        physics_row(a);
        ImGui::EndDisabled();
        colors_section(a, problems, count);
        ImGui::BeginDisabled(own_card);
        photos_section(a, problems, count);
        ImGui::EndDisabled();
    }
    ImGui::End();
}

void masks_panel(App *a, ImTextureID tex)
{
    static char shown[APP_PATH] = "";   // the project whose Masks tab was brought forward: a masks project just opened shows its masks
    static int wait = 0;                // frames to let the dock layout settle first: a focus given in the frame that builds it is lost
    if (strcmp(shown, a->dir) != 0) {
        snprintf(shown, sizeof shown, "%s", a->dir);
        wait = 3;
    }
    if (wait > 0 && --wait == 0 && a->car.masks_only) ImGui::SetNextWindowFocus();   // (a car's own front tab is Preview)
    const bool open = ImGui::Begin(PANEL_MASKS);
    a->mask.focus = open && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);   // the keyboard's + - 0 zoom the view
    if (open) {
        const CarProject &c = a->car;
        const bool original = car_race_original_of(&c) != nullptr;   // an exported original car (or its masks): one of the game's own
        if (c.built || c.masks_only) ImGui::Text("Car file: %s", leaf(c.cfm));
        else ImGui::Text("Car file: %s (not built yet)", leaf(c.cfm));
        char rel[CAR_PROJECT_NAME + 16];   // art-generated/car_masks for a kit, car_masks/<folder> for the other shapes
        car_project_masks_rel(&c, rel, sizeof rel);
        masks_changes_section(a);   // what the project's masks have changed from the game's: first, it is what a hand-over needs
        ImGui::Spacing();
        {   // the owner's PSDs of this car, turned into the masks above (the C reader, and the dash's when the folder has a dash.psd)
            char psd[APP_PATH];
            const bool found = app_psd_psd_dir(a, psd, sizeof psd);
            const bool busy = app_psd_busy(a);
            char edited[200];
            const int n_edited = app_psd_edited(a, edited, sizeof edited);   // PNGs Studio keeps (edited by hand, or not made by it and older than their PSD)
            ImGui::BeginDisabled(!found || busy);
            if (ImGui::Button("Update masks from PSD")) {
                if (n_edited > 0) ImGui::OpenPopup("Replace edited masks?");   // asks before replacing what was edited by hand
                else app_psd_start(a);
            }
            ui_probe("psd_convert");
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                if (busy) ImGui::SetTooltip("Converting: the masks are replaced when every PSD has been read");
                else if (!found) ImGui::SetTooltip("No folder named %s in %s", c.folder, a->psd_root[0] != '\0' ? a->psd_root : "the PSD folder (--psd-root)");
                else ImGui::SetTooltip("Reads the PSDs in %s (the car's art-source/masks) and writes the mask PNGs of this car (art-generated/car_masks, and the dash's).\n"
                                       "Only the PSDs newer than their PNG are converted. The PSDs are never written.\n"
                                       "A PNG edited by hand is not replaced without asking first.", psd);
            }
            ImGui::SameLine();
            char why[800];
            const bool can_check = app_dash_possible(a, why, sizeof why);
            ImGui::BeginDisabled(!can_check);
            if (ImGui::Button("Update + dash photo")) app_dash_check_start(a);
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("%s", can_check ? "Update masks from PSD, then take the cockpit photo at night with the car's own gauge colours: the dash as the game lights it.\nThe photo shows in the Drive window."
                                                  : why);
            if (ImGui::BeginPopupModal("Replace edited masks?", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
                if (n_edited == 0 || busy) ImGui::CloseCurrentPopup();   // nothing left to ask about (a conversion ran meanwhile)
                ImGui::TextWrapped("%s edited by hand (or not made by Studio from the PSD): %s.", n_edited == 1 ? "This mask is" : "These masks are", edited);
                ImGui::TextWrapped("Replace %s with what the PSD makes (the changes made by hand are lost), or keep %s as %s.", n_edited == 1 ? "it" : "them", n_edited == 1 ? "it" : "them", n_edited == 1 ? "it is" : "they are");
                if (ImGui::Button("Replace")) app_psd_regenerate(a), ImGui::CloseCurrentPopup();
                ui_probe("psd_replace");
                ImGui::SameLine();
                if (ImGui::Button("Keep")) app_psd_start(a), ImGui::CloseCurrentPopup();   // the other PSDs are converted, the edited PNGs stay
                ui_probe("psd_keep");
                ImGui::SameLine();
                if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();   // nothing is converted
                ui_probe("psd_cancel");
                ImGui::EndPopup();
            }
            if (n_edited > 0) {
                ImGui::SameLine();
                char kept[300];
                snprintf(kept, sizeof kept, "Kept as they are (edited by hand, or not made by Studio from the PSD): %s.\nUpdate masks from PSD asks before replacing them.", edited);
                help_marker(kept);
            }
            if (a->dash_stale[0] != '\0') {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.69f, 0.13f, 1.0f));
                ImGui::TextWrapped("%s", a->dash_stale);
                ImGui::PopStyleColor();
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("The PSD was painted after the mask the game uses was made: Update masks from PSD makes the masks again.");
            }
            if (busy) {
                ImGui::SameLine();
                ImGui::TextUnformatted("Converting...");
                ImGui::SameLine();
                if (ImGui::Button("Stop")) app_psd_stop(a);
            }
            ImGui::Spacing();
        }
        const bool kit = app_car_is_kit(a);   // a kit's masks are made from its PSDs: art-source/masks is edited, art-generated/car_masks is Studio's
        if (!c.has_masks_dir) ImGui::Text("Masks of the car (%s): none yet", rel);
        else ImGui::Text("Masks of the car (%s): %d", rel, c.mask_count);
        if (kit) {
            ImGui::SameLine();
            help_marker("These PNGs are made from the PSDs: do not edit them, a conversion replaces them.\n"
                        "The PSDs are what you edit; Show PSD folder shows them (art-source/masks).");
        }
        if (c.has_masks_dir || kit) {
            ImGui::SameLine();
            if (ImGui::Button("Show PSD folder")) app_reveal_masks(a);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", kit ? "Show art-source/masks in the Finder: the PSDs you edit.\nart-generated/car_masks is made from them and is not edited."
                                            : "Show the folder in the Finder");
        }
        if (c.has_masks_dir && original) {   // only one of the game's own cars has its masks in the game's source tree; a mod car's stay in its mod
            char repo[256];
            car_export_repo_path(c.folder, repo, sizeof repo);
            ImGui::SameLine();
            char help[320];
            snprintf(help, sizeof help, "In the game's source tree these files are %s:\ncopy them there to change the game's masks.", repo);
            help_marker(help);
        }
        ImGui::Spacing();
        mask_view_section(a, tex);
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
    ImGui::BeginDisabled(!picked || a->go != APP_ACT_NONE || a->confirm != APP_ACT_NONE);
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
void race_panel(App *a, ImTextureID photo, ImTextureID sheet)
{
    static int seen = 0, wait = 0;   // a new photo brings the tab forward (after the dock layout has settled: a focus given in the frame that builds it is lost)
    if (a->race.photo_serial != seen) {
        seen = a->race.photo_serial;
        wait = 3;
    }
    if (wait > 0 && --wait == 0) ImGui::SetNextWindowFocus();
    if (ImGui::Begin(PANEL_RACE)) {
        const ExportCar *car = car_race_original_of(&a->car);
        const bool busy = app_race_busy(a) || app_sheet_busy(a);
        const bool traffic = car != nullptr && car->kind != EXPORT_DRIVABLE;
        if (car == nullptr) {
            ImGui::TextDisabled("Test drive: the game's own cars only");
            ImGui::SameLine();
            help_marker("Test drive runs the game with this project's masks in front of its own, so it needs a project of one of the game's cars. "
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
            RaceModTrack mods[RACE_MOD_TRACKS];   // the tracks installed in the player's folder, listed after the game's own
            const int mod_count = race_mods_scan(a->user, mods, RACE_MOD_TRACKS);
            const char *shown = now != nullptr ? now->label : "";
            for (int i = 0; a->race_mod[0] != '\0' && i < mod_count; ++i)
                if (strcmp(mods[i].id, a->race_mod) == 0) shown = mods[i].label;
            ImGui::SetNextItemWidth(190);
            if (ImGui::BeginCombo("##track", shown)) {
                for (int i = 0; i < car_race_track_count(); ++i) {
                    const RaceTrack *tr = car_race_track(i);
                    const bool off = traffic && !tr->stage;   // the circuits have no traffic
                    ImGui::BeginDisabled(off);
                    if (ImGui::Selectable(tr->label, a->race_mod[0] == '\0' && a->race_track == i)) a->race_track = i, a->race_mod[0] = '\0';
                    ImGui::EndDisabled();
                    if (off && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("A circuit has no traffic");
                }
                if (mod_count > 0) ImGui::SeparatorText("Installed tracks");
                for (int i = 0; i < mod_count; ++i) {
                    ImGui::PushID(mods[i].id);
                    ImGui::BeginDisabled(traffic);
                    if (ImGui::Selectable(mods[i].label, strcmp(a->race_mod, mods[i].id) == 0)) snprintf(a->race_mod, sizeof a->race_mod, "%s", mods[i].id);
                    ImGui::EndDisabled();
                    if (traffic && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("An installed track has no traffic here");
                    ImGui::PopID();
                }
                ImGui::EndCombo();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("The night is each track's own; Rusty Springs is the reference");
            if (a->race_night && a->race_mod[0] == '\0' && now != nullptr && strcmp(now->stem, "TR7") == 0) ImGui::TextDisabled("Lost Vegas has no time of day: it stays as it is");
            ImGui::EndDisabled();
            char note[500];   // Test in race: who drives what, and where a traffic car is
            car_race_note(car, now != nullptr && a->race_mod[0] == '\0' ? now->stem : "", note, sizeof note);
            char help[800];   // the detail, behind the (?) beside the buttons: no loose explanation in the panel
            const int n = snprintf(help, sizeof help, "The game runs on a scratch player folder, with this project's masks. The car wears the colour chosen in Preview when the game's catalogue has it; then, in a Test drive, the rivals wear the game's own liveries too (the photo has no rivals).%s%s%s",
                                   traffic ? " A traffic or police car: City, Coastal or Alpine stages only." : "", note[0] != '\0' ? "\n\nTest drive: " : "", note);
            if (n < 0 || static_cast<size_t>(n) >= sizeof help) snprintf(help + sizeof help - 4, 4, "...");
            ImGui::BeginDisabled(busy);
            const bool drive_now = ImGui::Button("Test drive");
            ui_probe("test_drive");
            if (drive_now) app_race(a, 0);
            ImGui::SameLine();
            ImGui::BeginDisabled(a->race_mod[0] != '\0');
            const bool photo_now = ImGui::Button("In-game photo");
            ui_probe("in_game_photo");
            if (photo_now) app_race(a, 1);
            ImGui::EndDisabled();
            if (a->race_mod[0] != '\0' && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("The photo is of the game's own tracks");
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(!busy);
            if (ImGui::Button("Stop")) app_sheet_busy(a) ? app_sheet_stop(a) : app_race_stop(a);
            ImGui::EndDisabled();
            ImGui::SameLine();
            help_marker(help);
            if (!traffic) {   // a car you drive has a cockpit: the photo may be taken from the driver's seat, the dash lit by its gauge light (dep98)
                static const char *const VIEWS[] = { "Follow the car", "Cockpit" };
                static const char *const GAUGES[] = { "Default (from the car)", "0  white (the exe's)", "1  green", "2  red", "3  orange", "4  blue", "5  from the car's dash colours" };
                RaceLook &look = a->race_look;
                ImGui::BeginDisabled(busy);
                int view = look.cockpit ? 1 : 0;
                ImGui::SetNextItemWidth(170.0f);
                if (ImGui::Combo("View##racephoto", &view, VIEWS, 2)) look.cockpit = view;
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Race photo from behind the car, or from the driver's seat (the dash, with this project's dash_masks)");
                if (look.cockpit) {
                    int gauge = look.gauge_set ? look.gauge_light + 1 : 0;
                    ImGui::SetNextItemWidth(220.0f);
                    if (ImGui::Combo("Gauge light##racephoto", &gauge, GAUGES, 7)) look.gauge_set = gauge > 0, look.gauge_light = gauge > 0 ? gauge - 1 : 0;
                    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                        ImGui::SetTooltip("The colour the lit gauges take at night. 5 takes it from the project's dash_masks/dash_color.png; Default is the game's own setting (Restomod: 5). Use Night to see it");
                }
                ImGui::EndDisabled();
            }
            sheet_row(a, busy);
            ImGui::BeginDisabled(busy);
            race_look_controls(&a->race_look);
            ImGui::EndDisabled();
        }
        if (busy) ImGui::TextUnformatted("Running...");
        else if (a->race.summary[0] != '\0') {
            if (a->race.ok) muted_wrapped("%s", a->race.summary);
            else problem_text(a->race.summary);
        }
        if (a->sheet.count > 0 && sheet != 0) {   // a sheet takes the place of the single photo
            const ImVec2 room = ImGui::GetContentRegionAvail();
            const ImVec2 size = sheet_size(a);
            const float sw = size.x, sh = size.y;
            const float w = room.x < sw ? room.x : sw;   // the pane's width, however low it is: a sheet is wide, the pane scrolls
            ImGui::Image(sheet, ImVec2(w, w * sh / sw));
        } else if (a->race.photo != nullptr && photo != 0) {   // the picture, as big as the window leaves room for beside the log's few lines
            const ImVec2 room = ImGui::GetContentRegionAvail();
            const float pw = static_cast<float>(a->race.photo_w), ph = static_cast<float>(a->race.photo_h);   // 640 x 480, or 1280 x 960
            float w = room.x < pw ? room.x : pw;
            const float high = room.y - 90.0f > 120.0f ? room.y - 90.0f : 120.0f;
            if (w * ph / pw > high) w = high * pw / ph;
            ImGui::Image(photo, ImVec2(w, w * ph / pw));
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click for a large view");
            if (ImGui::IsItemClicked()) a->photo_view_open = 1;
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
