// The editor's interface layer (ui.h): Dear ImGui with docking over the SDL renderer: the dock space, the shortcuts and the panels (panels.cpp);
// the top view is viewport.cpp's.
#include "ui.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"

extern "C" {
#include "formats/track_gen.h"
#include "app_points.h"
#include "app_select.h"
#include "interact.h"
}
#include "panels.h"
#include "shortcut_keys.h"
#include "command_keys.h"
#include "viewport.h"

#include <algorithm>
#include <stdio.h>
#include <string.h>

static char g_ini[APP_PATH];
static App *g_app;              // the app the frames draw, for the editor's settings in the ini
static int g_drive_read = -1;   // the Drive car the ini had ([StudioSettings][Editor] DriveCar=N), until ui_settings_to gives it to the app

// The editor's own settings, kept in the ImGui ini beside the layout: the car Test drive gives the player.
static void *settings_open(ImGuiContext *, ImGuiSettingsHandler *, const char *name)
{
    return strcmp(name, "Editor") == 0 ? reinterpret_cast<void *>(1) : nullptr;
}

static void settings_line(ImGuiContext *, ImGuiSettingsHandler *, void *, const char *line)
{
    int pick = -1;
    if (sscanf(line, "DriveCar=%d", &pick) == 1 && car_race_menu_car(pick) != nullptr) g_drive_read = pick;   // a number past the menu keeps the default
}

static void settings_write(ImGuiContext *, ImGuiSettingsHandler *handler, ImGuiTextBuffer *out)
{
    const int pick = g_app != nullptr ? g_app->drive_pick : g_drive_read;   // before ui_settings_to (an early exit): what the ini had
    if (pick < 0) return;
    out->appendf("[%s][Editor]\nDriveCar=%d\n\n", handler->TypeName, pick);
}
static bool g_had_layout;   // the ini file was there at the start: its layout is the user's, not ours to replace
static SDL_Window *g_win;
static int g_win_w = 960, g_win_h = 600;   // the window's default size (ui_window_default)
static int g_reset_in, g_save_in;   // frames until Reset layout rebuilds the dock (the window's new size in first), and until the layout file is rewritten
enum { RESET_AFTER_FRAMES = 3, SAVE_AFTER_FRAMES = 2 };

int ui_open(SDL_Window *win, SDL_Renderer *ren, const char *ini, char *why, size_t why_cap)
{
    IMGUI_CHECKVERSION();
    if (ImGui::CreateContext() == nullptr) {
        snprintf(why, why_cap, "ImGui: no context");
        return 0;
    }
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;
    g_win = win;
    ImGuiSettingsHandler settings;
    settings.TypeName = "StudioSettings";
    settings.TypeHash = ImHashStr("StudioSettings");
    settings.ReadOpenFn = settings_open;
    settings.ReadLineFn = settings_line;
    settings.WriteAllFn = settings_write;
    ImGui::AddSettingsHandler(&settings);
    if (ini != nullptr) {
        snprintf(g_ini, sizeof g_ini, "%s", ini);
        io.IniFilename = g_ini;
        FILE *f = fopen(g_ini, "rb");
        g_had_layout = f != nullptr;
        if (f != nullptr) fclose(f);
    } else {
        io.IniFilename = nullptr;
    }
    if (ini != nullptr && g_had_layout) ImGui::LoadIniSettingsFromDisk(g_ini);   // now, not at the first frame: the settings are the app's before it runs anything
    ImGui::StyleColorsDark();
    if (!ImGui_ImplSDL3_InitForSDLRenderer(win, ren)) {
        snprintf(why, why_cap, "ImGui: the SDL backend did not start");
        ImGui::DestroyContext();
        return 0;
    }
    if (!ImGui_ImplSDLRenderer3_Init(ren)) {
        snprintf(why, why_cap, "ImGui: the renderer backend did not start");
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        return 0;
    }
    return 1;
}

void ui_settings_to(App *a)
{
    g_app = a;
    if (g_drive_read >= 0) a->drive_pick = g_drive_read;
    g_drive_read = -1;
}

void ui_close(void)
{
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}

void ui_model_shine(int on)
{
    model_panel_shine(on != 0);
}

void ui_window_default(int w, int h)
{
    g_win_w = w, g_win_h = h;
}

void ui_reset_layout(void)
{
    if (g_win != nullptr) {
        SDL_RestoreWindow(g_win);   // out of full screen or maximized first: a size is not taken there
        SDL_SetWindowSize(g_win, g_win_w, g_win_h);
        SDL_SetWindowPosition(g_win, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }
    g_reset_in = RESET_AFTER_FRAMES;
}

void ui_model_orbit(double yaw, double pitch, int dist)
{
    model_panel_preset(yaw, pitch, dist);
}

int ui_popup_open(void)
{
    return ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel) ? 1 : 0;
}

void ui_event(const SDL_Event *ev)
{
    ImGui_ImplSDL3_ProcessEvent(ev);
}

namespace {

// the first-run track layout's parts of the window (right column) and of the bottom (Road profile): at least the first, at most the second
const float RIGHT_LEAST = 0.33f, RIGHT_MOST = 0.45f, PROFILE_LEAST = 0.5f, PROFILE_MOST = 0.7f;

// the layout of a first run (no ini): the top view in the middle, the limits and the problems (two tabs) under it, the project and the points to its right
void default_layout(ImGuiID dock)
{
    ImGui::DockBuilderRemoveNode(dock);
    ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dock, ImGui::GetMainViewport()->Size);
    // the right column and the Road profile as wide as their panels need (all the Points columns, the profile's combos whole), within limits that keep
    // the view the larger part; the window's width is what DockBuilderSplitNode's ratios are of
    const float wide = ImGui::GetMainViewport()->Size.x;
    const float right_part = std::clamp(points_panel_width() / wide, RIGHT_LEAST, RIGHT_MOST);
    const float profile_part = std::clamp(road_key_panel_width() / (wide * (1.0f - right_part)), PROFILE_LEAST, PROFILE_MOST);
    ImGuiID right = 0, centre = dock, bottom = 0, points = 0, node = 0;
    right = ImGui::DockBuilderSplitNode(centre, ImGuiDir_Right, right_part, nullptr, &centre);
    bottom = ImGui::DockBuilderSplitNode(centre, ImGuiDir_Down, 0.30f, nullptr, &centre);
    points = ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, 0.55f, nullptr, &right);
    ImGui::DockBuilderDockWindow("Viewport", centre);
    ImGui::DockBuilderDockWindow(PANEL_LIMITS, bottom);
    ImGui::DockBuilderDockWindow(PANEL_PROBLEMS, bottom);   // its own tab beside Limits
    const ImGuiID profile = ImGui::DockBuilderSplitNode(bottom, ImGuiDir_Right, profile_part, nullptr, &bottom);   // the Road profile beside them: Node stays in sight
    ImGui::DockBuilderDockWindow(PANEL_ROAD_KEY, profile);
    ImGui::DockBuilderDockWindow(PANEL_DRIVE, bottom);   // and Test drive's
    const ImGuiID track = ImGui::DockBuilderSplitNode(right, ImGuiDir_Up, 0.42f, nullptr, &right);   // the road's width and verge material above the project
    ImGui::DockBuilderDockWindow(PANEL_TRACK, track);
    ImGui::DockBuilderDockWindow(PANEL_PROJECT, right);
    node = ImGui::DockBuilderSplitNode(points, ImGuiDir_Down, 0.45f, nullptr, &points);
    ImGui::DockBuilderDockWindow(PANEL_POINTS, points);
    ImGui::DockBuilderDockWindow(PANEL_NODE, node);
    ImGui::DockBuilderFinish(dock);
}

// the layout of a car's first run: its card on the left, the textures and the masks stacked on the right
void car_layout(ImGuiID dock)
{
    ImGui::DockBuilderRemoveNode(dock);
    ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dock, ImGui::GetMainViewport()->Size);
    ImGuiID left = 0, right = 0, masks = 0;
    left = ImGui::DockBuilderSplitNode(dock, ImGuiDir_Left, 0.4f, nullptr, &right);
    masks = ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, 0.3f, nullptr, &right);   // the race and its photo need the room
    const ImGuiID model = ImGui::DockBuilderSplitNode(right, ImGuiDir_Up, 0.6f, nullptr, &right);   // the car in 3D above its textures
    ImGui::DockBuilderDockWindow(PANEL_CAR, left);
    ImGui::DockBuilderDockWindow(PANEL_MODEL, model);
    ImGui::DockBuilderDockWindow(PANEL_TEXTURES, right);
    ImGui::DockBuilderDockWindow(PANEL_RACE, right);   // a tab beside Textures
    ImGui::DockBuilderDockWindow(PANEL_MASK_EDIT, right);   // and the mask editor, which needs the room
    ImGui::DockBuilderDockWindow(PANEL_PREVIEW, right);
    ImGui::DockBuilderDockWindow(PANEL_REHACER, masks);   // a tab beside Masks, which is the one in front
    ImGui::DockBuilderDockWindow(PANEL_MASKS, masks);
    ImGui::DockBuilderFinish(dock);
}

// the layout of a masks project: its facts on the left, the masks and under them the race on the right (it has no textures to list and no script to run)
void masks_layout(ImGuiID dock)
{
    ImGui::DockBuilderRemoveNode(dock);
    ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dock, ImGui::GetMainViewport()->Size);
    ImGuiID left = 0, right = 0, race = 0;
    left = ImGui::DockBuilderSplitNode(dock, ImGuiDir_Left, 0.3f, nullptr, &right);
    race = ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, 0.5f, nullptr, &right);   // the masks list and its changes need room above
    ImGui::DockBuilderDockWindow(PANEL_CAR, left);
    ImGui::DockBuilderDockWindow(PANEL_MODEL, right);   // a tab beside Masks
    ImGui::DockBuilderDockWindow(PANEL_MASKS, right);
    ImGui::DockBuilderDockWindow(PANEL_RACE, race);
    ImGui::DockBuilderDockWindow(PANEL_MASK_EDIT, race);
    ImGui::DockBuilderDockWindow(PANEL_PREVIEW, race);
    ImGui::DockBuilderFinish(dock);
}

// 1 when a dock space has no layout yet: a lone leaf with nothing docked in it (a layout kept in the ini file is built before the first frame)
bool dock_is_empty(ImGuiID dock)
{
    const ImGuiDockNode *node = ImGui::DockBuilderGetNode(dock);
    return node == nullptr || (node->ChildNodes[0] == nullptr && node->Windows.Size == 0);
}

// the shortcuts, when no text field has the keyboard
void shortcuts(App *a)
{
    const ImGuiIO &io = ImGui::GetIO();
    if (io.WantTextInput || io.KeyCtrl || io.KeyAlt || io.KeySuper) return;   // Cmd+S and the like are not ours
    const double cx = a->view.width / 2.0, cy = a->view.height / 2.0;
    if (ImGui::IsKeyPressed(ImGuiKey_N, false)) app_add_point(a);
    if (ImGui::IsKeyPressed(ImGuiKey_I, false)) app_insert_point(a);
    if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) app_remove_selection(a);
    else if (ImGui::IsKeyPressed(ImGuiKey_X, false)) app_remove_selection(a);
    const double rotate_step = io.KeyShift ? ROTATE_KEY_STEP_SHIFT : ROTATE_KEY_STEP;   // [ and ]: the selected points rotate about the first
    if (ImGui::IsKeyPressed(ImGuiKey_LeftBracket)) app_sel_rotate_by(a, -rotate_step);
    if (ImGui::IsKeyPressed(ImGuiKey_RightBracket)) app_sel_rotate_by(a, rotate_step);
    if (ImGui::IsKeyPressed(ImGuiKey_B, false)) app_build(a);
    if (ImGui::IsKeyPressed(ImGuiKey_U, false)) app_install(a);
    if (ImGui::IsKeyPressed(ImGuiKey_F, false)) interact_fit(a);
    if (ImGui::IsKeyPressed(ImGuiKey_Equal, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadAdd, false)) interact_zoom(a, cx, cy, VIEW_ZOOM_STEP);
    if (ImGui::IsKeyPressed(ImGuiKey_Minus, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract, false)) interact_zoom(a, cx, cy, 1 / VIEW_ZOOM_STEP);
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) interact_nudge(a, -INTERACT_NUDGE, 0);
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) interact_nudge(a, INTERACT_NUDGE, 0);
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) interact_nudge(a, 0, INTERACT_NUDGE);
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) interact_nudge(a, 0, -INTERACT_NUDGE);
}

const char *const TRACK_PANELS[] = { "Viewport", PANEL_TRACK, PANEL_PROJECT, PANEL_POINTS, PANEL_NODE, PANEL_ROAD_KEY, PANEL_LIMITS, PANEL_PROBLEMS };

// the panels of the mode on screen, as the Window menu lists them (a car's Textures and Rehacer are not a masks project's)
int mode_panels(const App *a, const char *out[], int cap)
{
    if (a->kind == APP_KIND_TRACK) {
        const int n = (int)(sizeof TRACK_PANELS / sizeof TRACK_PANELS[0]);
        for (int i = 0; i < n && i < cap; ++i) out[i] = TRACK_PANELS[i];
        return n < cap ? n : cap;
    }
    if (a->kind != APP_KIND_CAR) return 0;
    const bool masks = a->car.masks_only;
    const char *const all[] = { PANEL_CAR, PANEL_MODEL, masks ? nullptr : PANEL_TEXTURES, PANEL_RACE, PANEL_MASK_EDIT, PANEL_PREVIEW, PANEL_MASKS,
                                masks ? nullptr : PANEL_REHACER };
    int n = 0;
    for (const char *name : all)
        if (name != nullptr && n < cap) out[n++] = name;
    return n;
}

// does a floating panel lie (at least in part) inside the window? one dragged off it cannot be got back by hand
bool on_screen(const ImGuiWindow *w)
{
    const ImGuiViewport *v = ImGui::GetMainViewport();
    return w->Pos.x < v->Pos.x + v->Size.x && w->Pos.y < v->Pos.y + v->Size.y && w->Pos.x + w->Size.x > v->Pos.x && w->Pos.y + w->Size.y > v->Pos.y;
}

// a panel the user can see now: drawn last frame, not collapsed, its tab the one in front when docked, inside the window when floating
bool panel_shown(const char *name)
{
    const ImGuiWindow *w = ImGui::FindWindowByName(name);
    if (w == nullptr || !w->WasActive || w->Collapsed) return false;
    return w->DockIsActive ? w->DockTabIsVisible : on_screen(w);
}

// brings a panel to the front: uncollapsed, its tab chosen, a floating one off the window put back in its middle
void panel_show(const char *name)
{
    const ImGuiWindow *w = ImGui::FindWindowByName(name);
    if (w == nullptr) return;
    ImGui::SetWindowCollapsed(name, false);
    if (!w->DockIsActive && !on_screen(w)) {
        const ImGuiViewport *v = ImGui::GetMainViewport();
        ImGui::SetWindowPos(name, ImVec2(v->Pos.x + (v->Size.x - w->Size.x) / 2, v->Pos.y + (v->Size.y - w->Size.y) / 2));
    }
    ImGui::SetWindowFocus(name);
}

// the Window menu: Reset layout, then the mode's panels, a tick on the ones on screen; choosing one brings it to the front (no panel closes, so none reopens)
void window_menu(const App *a)
{
    if (a->kind == APP_KIND_NONE || !ImGui::BeginMenu("Window")) return;
    char keys[24];
    shortcut_label(keys, sizeof keys, SHORTCUT_MAC, "Shift+L");
    if (ImGui::MenuItem("Reset layout", keys)) ui_reset_layout();
    ImGui::Separator();
    const char *names[16];
    const int n = mode_panels(a, names, 16);
    for (int i = 0; i < n; ++i)
        if (ImGui::MenuItem(names[i], nullptr, panel_shown(names[i]))) panel_show(names[i]);
    ImGui::EndMenu();
}

// Reset layout's last step, in the frame its countdown ends: the mode's panels uncollapsed (DockBuilderDockWindow has docked the floating ones)
void uncollapse_panels(const App *a)
{
    const char *names[16];
    const int n = mode_panels(a, names, 16);
    for (int i = 0; i < n; ++i) ImGui::SetWindowCollapsed(names[i], false);
}

// the frame after Reset layout: in each dock node with tabs, the one a first run has in front (a panel that exists does not take the front by appearing,
// as on a first run, so it is chosen here)
void front_tabs(const App *a)
{
    const char *const track[] = { PANEL_LIMITS, PANEL_NODE }, *const car[] = { PANEL_PREVIEW, PANEL_REHACER }, *const masks[] = { PANEL_MODEL, PANEL_PREVIEW };
    const char *const *names = a->kind == APP_KIND_TRACK ? track : a->car.masks_only ? masks : car;
    if (a->kind == APP_KIND_NONE) return;
    for (int i = 0; i < 2; ++i) ImGui::SetWindowFocus(names[i]);
}

// Save, Undo and Redo on the platform's command key (command_keys.h)
void track_command_keys(App *a)
{
    const CommandKey key = command_key(ImGui::GetIO().WantTextInput);
    if (key == COMMAND_SAVE) app_save(a);
    else if (key == COMMAND_UNDO) app_undo(a);
    else if (key == COMMAND_REDO) app_redo(a);
    else if (key == COMMAND_COPY) app_copy_points(a);
    else if (key == COMMAND_PASTE) app_paste_points(a);
}

// a track's edit is one step to undo once it is done: no field typed in, no point dragged, no arrow held
bool track_edit_done()
{
    for (int b = 0; b < ImGuiMouseButton_COUNT; ++b)
        if (ImGui::IsMouseDown(b)) return false;
    return !ImGui::IsAnyItemActive() && !ImGui::IsKeyDown(ImGuiKey_LeftArrow) && !ImGui::IsKeyDown(ImGuiKey_RightArrow)
           && !ImGui::IsKeyDown(ImGuiKey_UpArrow) && !ImGui::IsKeyDown(ImGuiKey_DownArrow);
}

// the File menu: a new track, an open track or car, the recent ones (tracks and cars together), Save, Quit; each asks the app (app_ask), which asks about unsaved
// changes first
void main_menu(App *a)
{
    if (!ImGui::BeginMainMenuBar()) return;
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Open car...")) app_ask(a, APP_ACT_OPEN_CAR, "");
        if (ImGui::MenuItem("Open track...")) app_ask(a, APP_ACT_OPEN, "");
        if (ImGui::MenuItem("New track...")) app_ask(a, APP_ACT_NEW, "");
        if (ImGui::MenuItem("Export original car...")) a->export_dialog = 1;
        if (ImGui::BeginMenu("Open recent", a->recent.count > 0)) {
            for (int i = 0; i < a->recent.count; ++i) {
                char label[SESSION_PATH + 16];
                snprintf(label, sizeof label, "%s  (%s)", a->recent.dir[i], a->recent.kind[i] == RECENT_CAR ? "car" : "track");
                ImGui::PushID(i);
                if (ImGui::MenuItem(label, nullptr, false, recent_present(a->recent.dir[i], a->recent.kind[i]))) app_ask(a, APP_ACT_RECENT, a->recent.dir[i]);
                ImGui::PopID();
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();
        char save_keys[16];
        shortcut_label(save_keys, sizeof save_keys, SHORTCUT_MAC, "S");   // Save is Cmd+S on a Mac, Ctrl+S elsewhere, a car's and a track's
        if (ImGui::MenuItem("Save", save_keys, false, a->kind != APP_KIND_NONE && !app_rehacer_busy(a))) app_save(a);   // as the keys: not while Rehacer works on the kit
        if (ImGui::MenuItem("Quit")) app_ask(a, APP_ACT_QUIT, "");
        ImGui::EndMenu();
    }
    if (a->kind != APP_KIND_NONE && ImGui::BeginMenu("Edit")) {
        char undo_keys[16], redo_keys[24];
        shortcut_label(undo_keys, sizeof undo_keys, SHORTCUT_MAC, "Z");
        shortcut_label(redo_keys, sizeof redo_keys, SHORTCUT_MAC, "Shift+Z");
        if (ImGui::MenuItem("Undo", undo_keys, false, app_can_undo(a) && !app_rehacer_busy(a))) app_undo(a);
        if (ImGui::MenuItem("Redo", redo_keys, false, app_can_redo(a) && !app_rehacer_busy(a))) app_redo(a);
        ImGui::EndMenu();
    }
    window_menu(a);
    ImGui::EndMainMenuBar();
}

// "Could not open <folder>": the reason an Open, New or recent gave (a->open_error), with OK; a car that is not one says what was looked at and what is missing
void open_error_popup(App *a)
{
    static const char *const NAME = "Could not open";
    if (a->open_error[0] != '\0' && !ImGui::IsPopupOpen(NAME)) ImGui::OpenPopup(NAME);
    ImGui::SetNextWindowSizeConstraints(ImVec2(360, 0), ImVec2(560, 400));
    if (!ImGui::BeginPopupModal(NAME, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30.0f);
    ImGui::TextUnformatted(a->open_error);
    ImGui::PopTextWrapPos();
    ImGui::Spacing();
    if (ImGui::Button("OK") || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        a->open_error[0] = '\0';
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

// "Save changes to <project> before ...?": Save, Discard, Cancel (Escape, in main.c, is Cancel; Enter is Save, once the question has been on screen a
// frame, so the Enter that chose a menu item cannot answer it)
void confirm_popup(App *a)
{
    static const char *const NAME = "Unsaved changes";
    static int age = 0;               // frames the question has been up
    static bool save_failed = false;  // the last Save did not write: the question stays, with the reason
    if (a->confirm == APP_ACT_NONE) {
        age = 0;
        save_failed = false;
    } else if (!ImGui::IsPopupOpen(NAME)) {
        ImGui::OpenPopup(NAME);
    }
    if (!ImGui::BeginPopupModal(NAME, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    if (a->confirm == APP_ACT_NONE) {   // answered elsewhere (Escape)
        ImGui::CloseCurrentPopup();
    } else {
        ++age;
        const char *then = a->confirm == APP_ACT_QUIT ? "quitting" : a->confirm == APP_ACT_NEW || a->confirm == APP_ACT_NEW_AT ? "starting a new track"
                                                                                                                              : "opening another project";
        ImGui::Text("Save the changes to \"%s\" before %s?", a->kind == APP_KIND_CAR ? a->car.info.id : a->project.id, then);
        ImGui::Spacing();
        int answer = -1;
        if (ImGui::Button("Save")) answer = APP_ANSWER_SAVE;
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button("Discard")) answer = APP_ANSWER_DISCARD;
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) answer = APP_ANSWER_CANCEL;
        if (age > 1 && ImGui::IsKeyPressed(ImGuiKey_Enter, false)) answer = APP_ANSWER_SAVE;
        if (answer >= 0) {
            app_answer(a, answer);
            save_failed = answer == APP_ANSWER_SAVE && a->confirm != APP_ACT_NONE;
            if (a->confirm == APP_ACT_NONE) ImGui::CloseCurrentPopup();
        }
        if (save_failed) ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "%s", a->status);
    }
    ImGui::EndPopup();
}

}   // namespace

// the race photo as a texture: made again when the job took another picture (a->race.photo_serial)
static ImTextureID photo_texture(SDL_Renderer *ren, const App *a)
{
    static SDL_Texture *tex = nullptr;
    static int serial = -1;
    if (a->race.photo == nullptr) {
        if (tex != nullptr) SDL_DestroyTexture(tex), tex = nullptr;
        serial = -1;
        return 0;
    }
    if (tex == nullptr || serial != a->race.photo_serial) {
        if (tex != nullptr) SDL_DestroyTexture(tex);
        tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, a->race.photo_w, a->race.photo_h);   // 640 x 480, or twice at render scale 2
        if (tex != nullptr) SDL_UpdateTexture(tex, nullptr, a->race.photo, a->race.photo_w * 4);
        serial = a->race.photo_serial;
    }
    return reinterpret_cast<ImTextureID>(tex);
}

// the mask being edited over its texture as a texture: made again when the mask or what is shown of it changed
static ImTextureID mask_texture(SDL_Renderer *ren, const App *a)
{
    static SDL_Texture *tex = nullptr;
    static unsigned key = 0;
    static int width = 0, height = 0;
    const MaskUi &m = a->mask;
    if (m.sel < 0) {
        if (tex != nullptr) SDL_DestroyTexture(tex), tex = nullptr;
        width = height = 0;
        return 0;
    }
    unsigned now = m.me.version * 2654435761u + (unsigned)m.show * 7u + (unsigned)(m.opacity * 1000.0f) * 31u + (unsigned)m.sel * 131u;
    for (int i = 0; i < ME_CLASSES; ++i) now = now * 3u + (unsigned)m.class_on[i];
    if (tex == nullptr || width != m.me.width || height != m.me.height || key != now) {
        if (tex == nullptr || width != m.me.width || height != m.me.height) {
            if (tex != nullptr) SDL_DestroyTexture(tex);
            tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, m.me.width, m.me.height);
            if (tex != nullptr) SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
            width = m.me.width;
            height = m.me.height;
        }
        if (tex != nullptr) {
            uint8_t on[ME_CLASSES];
            for (int i = 0; i < ME_CLASSES; ++i) on[i] = (uint8_t)m.class_on[i];
            uint8_t *rgba = static_cast<uint8_t *>(malloc((size_t)width * height * 4));
            if (rgba != nullptr) {
                mask_edit_compose(&m.me, on, m.show, m.opacity, rgba);
                SDL_UpdateTexture(tex, nullptr, rgba, width * 4);
                free(rgba);
            }
        }
        key = now;
    }
    return reinterpret_cast<ImTextureID>(tex);
}

void ui_frame(SDL_Renderer *ren, App *a)
{
    static bool track_laid_out = false, car_laid_out = false, masks_laid_out = false;
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    if (a->kind != APP_KIND_NONE && ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_L, ImGuiInputFlags_RouteGlobal)) ui_reset_layout();
    const bool reset = g_reset_in > 0 && --g_reset_in == 0 && a->kind != APP_KIND_NONE;   // Reset layout's countdown ends: this frame rebuilds the dock
    g_app = a;
    main_menu(a);
    if (a->kind == APP_KIND_TRACK) {   // the track editor, as it was; its dock space keeps the layout of the first Studio
        const ImGuiID dock = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
        if (!track_laid_out || reset) {
            track_laid_out = true;
            if (reset || !g_had_layout || dock_is_empty(dock)) default_layout(dock);
        }
        app_sel_sync(a);   // the selected set agrees with the points and with `selected` before anything reads it
        if (a->confirm == APP_ACT_NONE) shortcuts(a), track_command_keys(a);   // the keys edit nothing while the question is up
        viewport_panel(a);
        project_panel(a);
        track_panel(a);
        points_panel(a);
        limits_panel(a);
        problems_panel(a);
        drive_panel(a);
        node_panel(a);
        road_key_window(a);
        if (track_edit_done()) app_track_commit(a);
    } else if (a->kind == APP_KIND_CAR) {   // a car has a dock space of its own: each kind keeps its layout
        const bool masks = a->car.masks_only;   // a masks project keeps a layout of its own: it shows fewer windows
        const ImGuiID dock = ImGui::DockSpaceOverViewport(ImHashStr(masks ? "StudioMasksDock" : "StudioCarDock"), ImGui::GetMainViewport());
        bool &laid_out = masks ? masks_laid_out : car_laid_out;
        if (!laid_out || reset) {
            laid_out = true;
            if (reset || dock_is_empty(dock)) (masks ? masks_layout : car_layout)(dock);
        }
        if (a->confirm == APP_ACT_NONE) car_shortcuts(a);
        car_panel(a);
        if (!a->car.masks_only) textures_panel(a);   // nor textures: only its masks
        model_panel(a, ren);
        masks_panel(a);
        race_panel(a, photo_texture(ren, a));
        mask_edit_panel(a, mask_texture(ren, a));
        preview_panel(a, ren);
        if (!a->car.masks_only) rehacer_panel(a);   // a masks project has no script
    } else {
        start_screen(a);
    }
    if (reset) {
        uncollapse_panels(a);
        g_save_in = SAVE_AFTER_FRAMES;   // the panels dock in this frame's Begin: the file is written once they are in
    } else if (g_save_in > 0) {
        if (g_save_in == SAVE_AFTER_FRAMES) front_tabs(a);
        if (--g_save_in == 0 && g_ini[0] != '\0') ImGui::SaveIniSettingsToDisk(g_ini);
    }
    export_popup(a);
    confirm_popup(a);
    open_error_popup(a);
    ImGui::Render();
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), ren);
}
