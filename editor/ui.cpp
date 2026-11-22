// The editor's interface layer (ui.h): Dear ImGui with docking over the SDL renderer: the dock space, the shortcuts and the panels (panels.cpp);
// the top view is viewport.cpp's.
#include "ui.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"

extern "C" {
#include "formats/track_gen.h"
#include "formats/mod_ini.h"
#include "app_points.h"
#include "app_select.h"
#include "interact.h"
}
#include "help_marker.h"
#include "new_car_popup.h"
#include "export_track_popup.h"
#include "physics_popup.h"
#include "panels.h"
#include "shortcut_keys.h"
#include "command_keys.h"
#include "edit_clipboard.h"
#include "viewport.h"

#include <algorithm>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static char g_ini[APP_PATH];
static App *g_app;              // the app the frames draw, for the editor's settings in the ini
static int g_drive_read = -1;   // the Drive car the ini had ([StudioSettings][Editor] DriveCar=N), until ui_settings_to gives it to the app

// the panels the Window menu has closed: not drawn at all, so their place in the dock is kept by the layout and reopening puts them back; kept in the ini
enum { CLOSED_MAX = 32, CLOSED_NAME = 48 };
static char g_closed[CLOSED_MAX][CLOSED_NAME];
static int g_closed_n;

static bool panel_open(const char *name)
{
    for (int i = 0; i < g_closed_n; ++i)
        if (strcmp(g_closed[i], name) == 0) return false;
    return true;
}

static void panel_set_open(const char *name, bool open)
{
    for (int i = 0; i < g_closed_n; ++i) {
        if (strcmp(g_closed[i], name) != 0) continue;
        if (open) memcpy(g_closed[i], g_closed[--g_closed_n], CLOSED_NAME);   // out of the set: the last one takes its place
        return;
    }
    if (!open && g_closed_n < CLOSED_MAX && strlen(name) < CLOSED_NAME) snprintf(g_closed[g_closed_n++], CLOSED_NAME, "%s", name);
}

// The editor's own settings, kept in the ImGui ini beside the layout: the car Test drive gives the player, and the panels the Window menu closed.
static void *settings_open(ImGuiContext *, ImGuiSettingsHandler *, const char *name)
{
    return strcmp(name, "Editor") == 0 ? reinterpret_cast<void *>(1) : nullptr;
}

static void settings_line(ImGuiContext *, ImGuiSettingsHandler *, void *, const char *line)
{
    int pick = -1, view = 0;
    if (sscanf(line, "DriveCar=%d", &pick) == 1 && car_race_menu_car(pick) != nullptr) g_drive_read = pick;   // a number past the menu keeps the default
    if (strncmp(line, "Closed=", 7) == 0) panel_set_open(line + 7, false);
    int light = NIGHT_LIGHT_DEFAULT;
    float tint[3] = { NIGHT_TINT_DEFAULT, NIGHT_TINT_DEFAULT, NIGHT_TINT_DEFAULT };
    night_pref_get(&light, tint);   // the light and tint so far: a line sets one of them, the other stays
    int read_light = 0;
    float read_tint[3];
    if (sscanf(line, "NightLight=%d", &read_light) == 1) night_pref_set(read_light, tint);   // the Preview's Night view: the light at the car (clamped) ...
    else if (sscanf(line, "NightTint=%f,%f,%f", &read_tint[0], &read_tint[1], &read_tint[2]) == 3) night_pref_set(light, read_tint);   // ... and its tint
    if (sscanf(line, "LightView=%d", &view) == 1) light_photo_view_pref_set(view);   // clamped: a damaged line gives the default
    if (sscanf(line, "LightHud=%d", &view) == 1) light_photo_hud_pref_set(view);
}

static void settings_write(ImGuiContext *, ImGuiSettingsHandler *handler, ImGuiTextBuffer *out)
{
    const int pick = g_app != nullptr ? g_app->drive_pick : g_drive_read;   // before ui_settings_to (an early exit): what the ini had
    int light = 0;
    float tint[3];
    const bool night = night_pref_get(&light, tint) != 0;
    const int view = light_photo_view_pref(), hud = light_photo_hud_pref();
    if (pick < 0 && g_closed_n == 0 && !night && view == LIGHT_VIEW_DEFAULT && !hud) return;
    out->appendf("[%s][Editor]\n", handler->TypeName);
    if (pick >= 0) out->appendf("DriveCar=%d\n", pick);
    for (int i = 0; i < g_closed_n; ++i) out->appendf("Closed=%s\n", g_closed[i]);
    if (night) out->appendf("NightLight=%d\nNightTint=%.3f,%.3f,%.3f\n", light, tint[0], tint[1], tint[2]);
    if (view != LIGHT_VIEW_DEFAULT) out->appendf("LightView=%d\n", view);
    if (hud) out->appendf("LightHud=1\n");
    out->append("\n");
}
static bool g_had_layout;   // the ini file was there at the start: its layout is the user's, not ours to replace
static SDL_Window *g_win;
static const float PANE_LEAST_PX = 90.0f;   // no window or pane is made smaller than this by a splitter: the Viewport and the Distance strip can neither vanish
static int g_win_w = 960, g_win_h = 600;   // the window's default size (ui_window_default)
static int g_reset_in, g_save_in;   // frames until Reset layout rebuilds the dock (the window's new size in first), and until the layout file is rewritten
enum { RESET_AFTER_FRAMES = 3, SAVE_AFTER_FRAMES = 2 };

int ui_card_photo(App *a)
{
    return model_panel_card_photo(a);
}

int ui_card_backdrop(App *a, int kind, int r, int g, int b, int bottom)
{
    return model_panel_backdrop(a, kind, r, g, b, bottom);
}

void ui_card_backdrop_file(const char *path)
{
    model_panel_backdrop_file(path);
}

const char *ui_card_backdrop_path(void)
{
    return model_panel_backdrop_path();
}

int ui_card_view(App *a, int card, int as_card)
{
    return model_panel_card_view(a, card != 0, as_card != 0);
}

static int g_errors;
static char g_first_error[256];

static void imgui_error(ImGuiContext *, void *, const char *msg)
{
    if (g_errors++ == 0) snprintf(g_first_error, sizeof g_first_error, "%s", msg);
    fprintf(stderr, "imgui error: %s\n", msg);
}

int ui_error_count(void)
{
    return g_errors;
}

const char *ui_error_first(void)
{
    return g_first_error;
}

void ui_fault(void)
{
    ImGui::EndDisabled();   // none was begun: ImGui's "Calling EndDisabled() too many times!"
}

void ui_double_click_by_frames(void)
{
    ImGui::GetIO().MouseDoubleClickTime = 30.0f;
    ImGui::GetIO().MouseSingleClickDelay = 31.0f;   // ImGui wants it over the double-click time
}

int ui_open(SDL_Window *win, SDL_Renderer *ren, const char *ini, char *why, size_t why_cap)
{
    IMGUI_CHECKVERSION();
    if (ImGui::CreateContext() == nullptr) {
        snprintf(why, why_cap, "ImGui: no context");
        return 0;
    }
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;
    ImGui::GetCurrentContext()->ErrorCallback = imgui_error;   // the context's own hook for the recoverable errors (imgui_internal.h)
    io.ConfigErrorRecoveryEnableAssert = false;                // the callback reports the error; an assert after it would abort the sanitizer builds before the run can say it failed
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
    ImGui::GetStyle().WindowMinSize = ImVec2(PANE_LEAST_PX, PANE_LEAST_PX);   // a splitter stops here: the Viewport and the Distance strip (and every dock) keep this much
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
    int light = 0;
    float tint[3];
    if (night_pref_get(&light, tint)) {   // the Preview's Night light and tint the ini had (the command line, which comes after, wins)
        a->night.light = light;
        for (int i = 0; i < 3; ++i) a->night.tint[i] = tint[i];
    }
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
    g_closed_n = 0;   // Reset layout shows every panel again
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
const float STRIP_PART = 0.46f;   // the Distance strip's share of the height under the Viewport (what is left of the centre after the bottom row) at most
const float STRIP_DEFAULT_PX = 120.0f;   // the Distance strip's height on a first run (the owner drags its top border for more); the layout is kept in the ini

// the layout of a first run (no ini): the top view in the middle, the Distance strip as a row under it, then the limits and the problems (tabs), the project and the points to its right
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
    const float centre_h = ImGui::GetMainViewport()->Size.y * 0.70f;   // what is left under the bottom row
    const float strip_part = std::clamp(STRIP_DEFAULT_PX / std::max(centre_h, 1.0f), 0.1f, STRIP_PART);
    const ImGuiID strip = ImGui::DockBuilderSplitNode(centre, ImGuiDir_Down, strip_part, nullptr, &centre);   // the Distance strip: a row of its own under the view, as wide as it
    points = ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, 0.55f, nullptr, &right);
    ImGui::DockBuilderDockWindow("Viewport", centre);
    ImGui::DockBuilderDockWindow(PANEL_LIMITS, bottom);
    ImGui::DockBuilderDockWindow(PANEL_PROBLEMS, bottom);   // its own tab beside Limits
    const ImGuiID profile = ImGui::DockBuilderSplitNode(bottom, ImGuiDir_Right, profile_part, nullptr, &bottom);   // the Road profile beside them: Node stays in sight
    ImGui::DockBuilderDockWindow(PANEL_ROAD_KEY, profile);
    ImGui::DockBuilderDockWindow(PANEL_DRIVE, bottom);   // and Test drive's
    ImGui::DockBuilderDockWindow(PANEL_STRIP, strip);
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
    ImGuiID left = 0, right = 0;
    left = ImGui::DockBuilderSplitNode(dock, ImGuiDir_Left, 0.4f, nullptr, &right);
    const ImGuiID model = ImGui::DockBuilderSplitNode(right, ImGuiDir_Up, 0.4f, nullptr, &right);   // the car in 3D above its masks, the race and its photo
    ImGui::DockBuilderDockWindow(PANEL_CAR, left);
    ImGui::DockBuilderDockWindow(PANEL_MODEL, model);
    ImGui::DockBuilderDockWindow(PANEL_MASKS, right);   // the masks, which need the room
    ImGui::DockBuilderDockWindow(PANEL_RACE, right);   // a tab beside Masks
    ImGui::DockBuilderDockWindow(PANEL_PREVIEW, right);
    ImGui::DockBuilderFinish(dock);
}

// the layout of the Track light window: the file's controls on the left, the photo on the right
void light_layout(ImGuiID dock)
{
    ImGui::DockBuilderRemoveNode(dock);
    ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dock, ImGui::GetMainViewport()->Size);
    ImGuiID left = 0, right = 0;
    left = ImGui::DockBuilderSplitNode(dock, ImGuiDir_Left, 0.34f, nullptr, &right);
    ImGuiID photo = 0, compare = 0;
    photo = ImGui::DockBuilderSplitNode(right, ImGuiDir_Left, 0.5f, nullptr, &compare);   // the photo and, beside it, the comparison
    ImGui::DockBuilderDockWindow(PANEL_LIGHT, left);
    ImGui::DockBuilderDockWindow(PANEL_LIGHT_PHOTO, photo);
    ImGui::DockBuilderDockWindow(PANEL_LIGHT_COMPARE, compare);
    ImGui::DockBuilderDockWindow(PANEL_TRACK_MASKS, compare);   // a tab beside the comparison (only an original track draws it)
    ImGui::DockBuilderFinish(dock);
}

// the layout of a masks project: its facts on the left, the masks (list and view) and the race on the right
void masks_layout(ImGuiID dock)
{
    ImGui::DockBuilderRemoveNode(dock);
    ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dock, ImGui::GetMainViewport()->Size);
    ImGuiID left = 0, right = 0, race = 0;
    left = ImGui::DockBuilderSplitNode(dock, ImGuiDir_Left, 0.3f, nullptr, &right);
    race = ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, 0.4f, nullptr, &right);   // the masks list, its changes and the view need room above
    ImGui::DockBuilderDockWindow(PANEL_CAR, left);
    ImGui::DockBuilderDockWindow(PANEL_MASKS, right);   // alone above: the list and the view need the room
    ImGui::DockBuilderDockWindow(PANEL_MODEL, race);
    ImGui::DockBuilderDockWindow(PANEL_RACE, race);
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

const char *const TRACK_PANELS[] = { "Viewport", PANEL_TRACK, PANEL_PROJECT, PANEL_POINTS, PANEL_NODE, PANEL_ROAD_KEY, PANEL_LIMITS, PANEL_PROBLEMS, PANEL_STRIP, PANEL_DRIVE };

// the panels of the mode on screen, as the Window menu lists them
int mode_panels(const App *a, const char *out[], int cap)
{
    if (a->kind == APP_KIND_TRACK) {
        const int n = (int)(sizeof TRACK_PANELS / sizeof TRACK_PANELS[0]);
        for (int i = 0; i < n && i < cap; ++i) out[i] = TRACK_PANELS[i];
        return n < cap ? n : cap;
    }
    if (a->kind == APP_KIND_LIGHT) {
        const char *const light[] = { PANEL_LIGHT, PANEL_LIGHT_PHOTO, PANEL_LIGHT_COMPARE, PANEL_TRACK_MASKS };
        const int n = a->original_dir[0] != '\0' ? 4 : 3;   // the masks are an original track's
        for (int i = 0; i < n && i < cap; ++i) out[i] = light[i];
        return cap < n ? cap : n;
    }
    if (a->kind != APP_KIND_CAR) return 0;
    const char *const all[] = { PANEL_CAR, PANEL_MODEL, PANEL_RACE, PANEL_PREVIEW, PANEL_MASKS };
    int n = 0;
    for (const char *name : all)
        if (n < cap) out[n++] = name;
    return n;
}

// the Window menu: Reset layout, then the mode's panels, each a toggle: ticked when the panel is open, a click closes it or opens it again (docked where it was)
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
        if (ImGui::MenuItem(names[i], nullptr, panel_open(names[i]))) panel_set_open(names[i], !panel_open(names[i]));
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
    const char *const track[] = { PANEL_LIMITS, PANEL_NODE }, *const car[] = { PANEL_PREVIEW, PANEL_CAR }, *const masks[] = { PANEL_MASKS, PANEL_PREVIEW };
    const char *const *names = a->kind == APP_KIND_TRACK ? track : a->car.masks_only ? masks : car;
    if (a->kind == APP_KIND_NONE || a->kind == APP_KIND_LIGHT) return;   // the light window has no tabs
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

// The File menu holds the actions of the room on screen only (Cars, Tracks, the main menu), New before Open: a car's room has no New track, a track's no Open car.
enum FileItem { FILE_NEW_CAR, FILE_NEW_TRACK, FILE_OPEN_TRACK, FILE_LIGHT, FILE_EXPORT_TRACK, FILE_COPY, FILE_OPEN_CAR, FILE_EXPORT, FILE_RECENT, FILE_WORKSPACE, FILE_SEPARATOR, FILE_SAVE, FILE_MAIN, FILE_QUIT };

static const char *file_label(FileItem item)
{
    switch (item) {
    case FILE_NEW_CAR: return "New car...";
    case FILE_NEW_TRACK: return "New track...";
    case FILE_OPEN_TRACK: return "Open track...";
    case FILE_LIGHT: return "Open original track...";
    case FILE_EXPORT_TRACK: return "Export original track...";
    case FILE_COPY: return "Copy to game repo";
    case FILE_OPEN_CAR: return "Open car...";
    case FILE_EXPORT: return "Export original car...";
    case FILE_RECENT: return "Open recent";
    case FILE_WORKSPACE: return "Workspace";
    case FILE_SAVE: return "Save";
    case FILE_MAIN: return "Main menu";
    case FILE_QUIT: return "Quit";
    case FILE_SEPARATOR: break;
    }
    return "";
}

static int file_items(const App *a, FileItem out[12])
{
    int n = 0;
    const AppRoom room = app_room(a);
    if (room == APP_ROOM_TRACKS) out[n++] = FILE_NEW_TRACK, out[n++] = FILE_OPEN_TRACK, out[n++] = FILE_LIGHT, out[n++] = FILE_EXPORT_TRACK, out[n++] = FILE_RECENT;
    if (room == APP_ROOM_TRACKS && app_original_can_copy(a)) out[n++] = FILE_COPY;
    if (room == APP_ROOM_CARS) out[n++] = FILE_NEW_CAR, out[n++] = FILE_OPEN_CAR, out[n++] = FILE_EXPORT, out[n++] = FILE_RECENT;
    if (room == APP_ROOM_CARS && app_car_copy_can(a)) out[n++] = FILE_COPY;
    if (room == APP_ROOM_HUB) out[n++] = FILE_WORKSPACE;   // the workspace is for both rooms: its menu is the main menu's, not a room's
    if (room == APP_ROOM_HUB) out[n++] = FILE_SEPARATOR;
    if (room != APP_ROOM_HUB) out[n++] = FILE_SAVE, out[n++] = FILE_MAIN;
    out[n++] = FILE_QUIT;
    return n;
}

// Open recent: the room's own projects (cars in Cars, tracks in Tracks)
static void recent_menu(App *a)
{
    const int kind = app_room(a) == APP_ROOM_CARS ? RECENT_CAR : RECENT_TRACK;
    int count = 0;
    for (int i = 0; i < a->recent.count; ++i) count += a->recent.kind[i] == kind;
    if (!ImGui::BeginMenu("Open recent", count > 0)) return;
    for (int i = 0; i < a->recent.count; ++i) {
        if (a->recent.kind[i] != kind) continue;
        ImGui::PushID(i);
        if (ImGui::MenuItem(a->recent.dir[i], nullptr, false, recent_present(a->recent.dir[i], a->recent.kind[i]))) app_ask(a, APP_ACT_RECENT, a->recent.dir[i]);
        ImGui::PopID();
    }
    ImGui::EndMenu();
}

static void workspace_menu(App *a)
{
    if (!ImGui::BeginMenu("Workspace")) return;
    if (ImGui::MenuItem("Choose folder...")) app_workspace_pick(a);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("One folder with cars/ and tracks/ in it: Open and New start there, and the start screen lists what it holds");
    if (ImGui::MenuItem("No workspace", nullptr, false, a->workspace[0] != '\0')) app_workspace_clear(a);
    if (a->workspace[0] != '\0') {
        ImGui::Separator();
        ImGui::TextDisabled("%s", a->workspace);
    }
    ImGui::EndMenu();
}

void main_menu(App *a)
{
    edit_clipboard_frame();   // before the menu bar can return: the text field the user was in, and an Edit item under way
    if (!ImGui::BeginMainMenuBar()) return;
    if (ImGui::BeginMenu("File")) {
        FileItem items[12];
        const int n = file_items(a, items);
        for (int i = 0; i < n; ++i) {
            const FileItem item = items[i];
            if (item == FILE_SEPARATOR) ImGui::Separator();
            else if (item == FILE_RECENT) recent_menu(a);
            else if (item == FILE_WORKSPACE) workspace_menu(a);
            else if (item == FILE_SAVE) {
                char save_keys[16];
                shortcut_label(save_keys, sizeof save_keys, SHORTCUT_MAC, "S");   // Save is Cmd+S on a Mac, Ctrl+S elsewhere, a car's and a track's
                if (ImGui::MenuItem("Save", save_keys)) app_save(a);
            } else if (item == FILE_MAIN) {
                char keys[16];
                shortcut_label(keys, sizeof keys, SHORTCUT_MAC, "W");
                if (ImGui::MenuItem("Main menu", keys)) app_ask(a, APP_ACT_MAIN, "");
            } else if (ImGui::MenuItem(file_label(item))) {
                if (item == FILE_NEW_CAR) app_ask(a, APP_ACT_NEW_CAR, "");
                else if (item == FILE_NEW_TRACK) app_ask(a, APP_ACT_NEW, "");
                else if (item == FILE_OPEN_TRACK) app_ask(a, APP_ACT_OPEN, "");
                else if (item == FILE_LIGHT) a->original_dialog = 1;
                else if (item == FILE_EXPORT_TRACK) app_ask(a, APP_ACT_EXPORT_TRACK, "");
                else if (item == FILE_COPY) app_original_copy_ask(a);
                else if (item == FILE_OPEN_CAR) app_ask(a, APP_ACT_OPEN_CAR, "");
                else if (item == FILE_EXPORT) a->export_dialog = 1;
                else if (item == FILE_QUIT) app_ask(a, APP_ACT_QUIT, "");
            }
        }
        ImGui::EndMenu();
    }
    if (a->kind != APP_KIND_NONE && ImGui::BeginMenu("Edit")) {
        char undo_keys[16], redo_keys[24];
        shortcut_label(undo_keys, sizeof undo_keys, SHORTCUT_MAC, "Z");
        shortcut_label(redo_keys, sizeof redo_keys, SHORTCUT_MAC, "Shift+Z");
        if (ImGui::MenuItem("Undo", undo_keys, false, app_can_undo(a))) app_undo(a);
        if (ImGui::MenuItem("Redo", redo_keys, false, app_can_redo(a))) app_redo(a);
        ImGui::Separator();
        static const struct { const char *label, *key; EditAction action; } CLIPBOARD[] = {
            { "Cut", "X", EDIT_CUT }, { "Copy", "C", EDIT_COPY }, { "Paste", "V", EDIT_PASTE }, { "Select all", "A", EDIT_SELECT_ALL } };
        for (const auto &item : CLIPBOARD) {   // on the text field the user was in: it is given back, the chord typed for it
            char keys[16];
            shortcut_label(keys, sizeof keys, SHORTCUT_MAC, item.key);
            if (ImGui::MenuItem(item.label, keys, false, edit_clipboard_ready() && !edit_clipboard_busy())) edit_clipboard_do(item.action);
        }
        ImGui::EndMenu();
    }
    window_menu(a);
    ImGui::EndMainMenuBar();
}

// New track with a workspace: only a name is asked for; the id (the folder in <workspace>/tracks) is made from it, shown, and may be typed over; Create makes the track
void new_track_popup(App *a)
{
    static const char *const NAME = "New track";
    if (a->new_track_dialog && !ImGui::IsPopupOpen(NAME)) ImGui::OpenPopup(NAME);
    if (!ImGui::BeginPopupModal(NAME, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    if (!a->new_track_dialog) {
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }
    ImGui::TextUnformatted("Name");
    ImGui::SameLine();
    if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
    if (ImGui::InputText("##name", a->new_track_name, sizeof a->new_track_name) && !a->new_track_id_typed)
        workspace_id_from_name(a->new_track_name, a->new_track_id, sizeof a->new_track_id, MOD_INI_ID_MAX);
    ImGui::TextUnformatted("Id  ");
    ImGui::SameLine();
    if (ImGui::InputText("##id", a->new_track_id, sizeof a->new_track_id)) a->new_track_id_typed = 1;
    ImGui::SameLine();
    help_marker("The id is the track's folder in the workspace's tracks/. It is made from the name; type over it to choose another.");
    if (a->new_track_error[0] != '\0') ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "%s", a->new_track_error);
    ImGui::Spacing();
    const bool create = ImGui::Button("Create") || ImGui::IsKeyPressed(ImGuiKey_Enter, false);
    ImGui::SameLine();
    const bool cancel = ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape, false);
    if (create && app_new_track_named(a, a->new_track_name, a->new_track_id)) a->new_track_dialog = 0;
    if (cancel) a->new_track_dialog = 0, a->new_track_error[0] = '\0';
    if (!a->new_track_dialog) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

// Open original track: the originals in the workspace, one click each; Add makes all sixteen from the game's files when some are missing
void original_popup(App *a)
{
    static const char *const NAME = "Open original track";
    if (a->original_dialog && !ImGui::IsPopupOpen(NAME)) ImGui::OpenPopup(NAME);
    if (!ImGui::BeginPopupModal(NAME, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    if (!a->original_dialog) {
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }
    const int have = app_original_count(a);
    if (a->workspace[0] == '\0') {
        ImGui::TextUnformatted("Choose a workspace first (File > Workspace on the main menu):\nthe original tracks are kept in its tracks/ folder.");
    } else {
        for (int i = 0; i < OT_STEMS; ++i) {
            char dir[APP_PATH];
            if (!original_track_dir(a->workspace, original_track_stem(i), dir, sizeof dir) || !original_track_present(dir)) continue;
            if (ImGui::Selectable(original_track_stem(i), false, ImGuiSelectableFlags_DontClosePopups, ImVec2(160, 0))) {
                a->original_dialog = 0;
                app_ask(a, APP_ACT_OPEN_AT, dir);
            }
        }
        if (have == 0) ImGui::TextDisabled("No original track is in this workspace.");   // they are put there by the migration (--steps add-originals), not from here
    }
    ImGui::Spacing();
    if (ImGui::Button("Close") || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) a->original_dialog = 0;
    if (!a->original_dialog) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

// Copy to game repo: what would change in the repo's data/, with Copy and Cancel
void copy_popup(App *a)
{
    static const char *const NAME = "Copy to game repo";
    if (a->publish_dialog && !ImGui::IsPopupOpen(NAME)) ImGui::OpenPopup(NAME);
    if (!ImGui::BeginPopupModal(NAME, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    if (!a->publish_dialog) {
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }
    ImGui::TextUnformatted(a->publish_car ? "Copies the changed and new masks into the game repo's data/ (nothing is committed):" : "Copies the changed light files and mask PNGs into the game repo's data/ (nothing is committed):");
    ImGui::TextDisabled("%s", app_repo_data(a));
    ImGui::Spacing();
    if (ImGui::BeginChild("##publishlist", ImVec2(560.0f, 220.0f), ImGuiChildFlags_Borders)) ImGui::TextWrapped("%s", a->publish_text);   // many new masks: it scrolls
    ImGui::EndChild();
    ImGui::Spacing();
    if (ImGui::Button("Copy")) app_original_copy(a);
    ImGui::SameLine();
    if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) a->publish_dialog = 0;
    if (!a->publish_dialog) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
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
    static int age = 0;               // frames the question has been up
    static bool save_failed = false;  // the last Save did not write: the question stays, with the reason
    static const char *shown = nullptr;   // the title of the question that is up
    // leaving Studio is asked every time: with nothing unsaved it is only "Quit Studio?" (Quit / Cancel)
    const bool plain_quit = a->confirm == APP_ACT_QUIT && !app_modified(a);
    const char *const NAME = plain_quit ? "Quit Studio" : "Unsaved changes";
    if (a->confirm == APP_ACT_NONE) {
        age = 0;
        save_failed = false;
    } else if (!ImGui::IsPopupOpen(NAME)) {
        ImGui::OpenPopup(NAME);
        shown = NAME;
    }
    if (shown == nullptr || !ImGui::BeginPopupModal(shown, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;   // the name it was opened with, also while it is closed by an answer
    if (a->confirm == APP_ACT_NONE) {   // answered elsewhere (Escape)
        ImGui::CloseCurrentPopup();
    } else if (plain_quit) {
        ++age;
        ImGui::TextUnformatted("Quit Studio?");
        ImGui::Spacing();
        int answer = -1;
        if (ImGui::Button("Quit")) answer = APP_ANSWER_DISCARD;   // nothing to save: it goes on
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) answer = APP_ANSWER_CANCEL;
        if (age > 1 && ImGui::IsKeyPressed(ImGuiKey_Enter, false)) answer = APP_ANSWER_DISCARD;
        if (answer >= 0) {
            app_answer(a, answer);
            ImGui::CloseCurrentPopup();
        }
    } else {
        ++age;
        const char *then = a->confirm == APP_ACT_QUIT ? "quitting" : a->confirm == APP_ACT_NEW || a->confirm == APP_ACT_NEW_AT ? "starting a new track"
                                                                                                                              : a->confirm == APP_ACT_NEW_CAR ? "starting a new car"
                                                                                                                              : a->confirm == APP_ACT_EXPORT_TRACK ? "exporting an original track"
                                                                                                                              : a->confirm == APP_ACT_LIGHT_AT ? "choosing another light folder"
                                                                                                                                                               : a->confirm == APP_ACT_MAIN ? "going to the main menu"
                                                                                                                                                               : "opening another project";
        ImGui::Text("Save the changes to \"%s\" before %s?", a->kind == APP_KIND_CAR ? a->car.info.id : a->kind == APP_KIND_LIGHT ? "the light file" : a->project.id, then);
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

int ui_panel_toggle(const App *a, int index)
{
    const char *names[16];
    const int n = mode_panels(a, names, 16);
    if (index < 0 || index >= n) return 0;
    panel_set_open(names[index], !panel_open(names[index]));
    return 1;
}

// a window the user docks, as the Window menu lists them: not a popup, a child, a tooltip or a no-docking window (the menu bar, the dock space, Start, the photo's window)
static bool is_panel_window(const ImGuiWindow *w)
{
    const ImGuiWindowFlags not_panel = ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_Popup | ImGuiWindowFlags_Tooltip | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_Modal;
    return (w->Flags & not_panel) == 0 && !w->IsFallbackWindow && strncmp(w->Name, "Debug", 5) != 0 && strncmp(w->Name, "##", 2) != 0;
}

void ui_print_menu(const App *a)
{
    FileItem items[12];
    const int n = file_items(a, items);
    const AppRoom room = app_room(a);
    printf("room: %s\nfile:", room == APP_ROOM_CARS ? "cars" : room == APP_ROOM_TRACKS ? "tracks" : "hub");
    for (int i = 0; i < n; ++i)
        if (items[i] != FILE_SEPARATOR) printf("%s%s", i == 0 ? " " : " | ", file_label(items[i]));
    printf("\n");
}

void ui_print_panels(const App *a)
{
    const char *names[16];
    const int n = mode_panels(a, names, 16);
    for (int i = 0; i < n; ++i) {
        const ImGuiWindow *w = ImGui::FindWindowByName(names[i]);
        printf("panel %d: %s: %s, %s, %s\n", i, names[i], panel_open(names[i]) ? "open" : "closed", w != nullptr && w->Active ? "drawn" : "not drawn", w != nullptr && w->DockId != 0 ? "docked" : "floating");
    }
    printf("unlisted:");   // panels drawn this frame that the Window menu does not list
    const ImGuiContext *g = ImGui::GetCurrentContext();
    for (const ImGuiWindow *w : g->Windows) {
        if (!w->Active || !is_panel_window(w)) continue;
        bool listed = false;
        for (int i = 0; i < n; ++i) listed |= strcmp(names[i], w->Name) == 0;
        if (!listed) printf(" [%s]", w->Name);
    }
    printf("\n");
    printf("widths:");   // each drawn panel's width in pixels, so a layout that squeezes one (the laptop's 854 px window) is a test failure
    for (int i = 0; i < n; ++i) {
        const ImGuiWindow *w = ImGui::FindWindowByName(names[i]);
        if (w != nullptr && w->Active) printf(" [%s %.0f]", names[i], w->Size.x);
    }
    printf("\n");
}


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

// a Track light job's picture as a texture, in `tex`: made again when the job took another picture (its serial); none when the window is not on or the job has no picture
static ImTextureID job_texture(SDL_Renderer *ren, bool on, const LightPhotoJob &j, SDL_Texture *&tex, int &serial)
{
    if (!on || j.rgba == nullptr) {
        if (tex != nullptr) SDL_DestroyTexture(tex), tex = nullptr;
        serial = -1;
        return 0;
    }
    if (tex == nullptr || serial != j.serial) {
        if (tex != nullptr) SDL_DestroyTexture(tex);
        // the software renderer (a headless run) draws only half of a 1280 x 960 texture's quad: it gets the picture halved (2 x 2 box average); a window's renderer gets it whole
        const char *renderer = SDL_GetRendererName(ren);
        const bool halve = renderer != nullptr && strcmp(renderer, "software") == 0 && j.w >= 1280 && j.w % 2 == 0 && j.h % 2 == 0;
        const int tw = halve ? j.w / 2 : j.w, th = halve ? j.h / 2 : j.h;
        uint8_t *half = halve ? static_cast<uint8_t *>(malloc(static_cast<size_t>(tw) * th * 4)) : nullptr;
        if (half != nullptr) {
            for (int y = 0; y < th; ++y)
                for (int x = 0; x < tw; ++x)
                    for (int c = 0; c < 4; ++c) {
                        const uint8_t *p = j.rgba + (static_cast<size_t>(y) * 2 * j.w + static_cast<size_t>(x) * 2) * 4 + c;
                        half[(static_cast<size_t>(y) * tw + x) * 4 + c] = static_cast<uint8_t>((p[0] + p[4] + p[static_cast<size_t>(j.w) * 4] + p[static_cast<size_t>(j.w) * 4 + 4] + 2) / 4);
                    }
        }
        const bool use_half = half != nullptr;
        tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, use_half ? tw : j.w, use_half ? th : j.h);
        if (tex != nullptr) SDL_UpdateTexture(tex, nullptr, use_half ? half : j.rgba, (use_half ? tw : j.w) * 4);
        free(half);
        serial = j.serial;
    }
    return reinterpret_cast<ImTextureID>(tex);
}

// the picked track mask over its texture as a texture: made again when a new picture was made (a->tmask_serial)
static ImTextureID track_mask_texture(SDL_Renderer *ren, const App *a)
{
    static SDL_Texture *tex = nullptr;
    static unsigned serial = 0;
    if (a->tmask_rgba == nullptr) {
        if (tex != nullptr) SDL_DestroyTexture(tex), tex = nullptr;
        serial = 0;
        return 0;
    }
    if (tex == nullptr || serial != a->tmask_serial) {
        if (tex != nullptr) SDL_DestroyTexture(tex);
        tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, a->tmask_w, a->tmask_h);
        if (tex != nullptr) SDL_UpdateTexture(tex, nullptr, a->tmask_rgba, a->tmask_w * 4);
        serial = a->tmask_serial;
    }
    return reinterpret_cast<ImTextureID>(tex);
}

// the sheet as a texture: made again when a cell changed (a->sheet_serial)
static ImTextureID sheet_texture(SDL_Renderer *ren, const App *a)
{
    static SDL_Texture *tex = nullptr;
    static int serial = -1;
    if (a->sheet.count == 0) {
        if (tex != nullptr) SDL_DestroyTexture(tex), tex = nullptr;
        serial = -1;
        return 0;
    }
    if (tex == nullptr || serial != a->sheet_serial) {
        if (tex != nullptr) SDL_DestroyTexture(tex), tex = nullptr;
        uint8_t *rgba = nullptr;
        int w = 0, h = 0;
        char why[160];
        if (sheet_compose(&a->sheet, &rgba, &w, &h, why, sizeof why)) {
            tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, w, h);
            if (tex != nullptr) SDL_UpdateTexture(tex, nullptr, rgba, w * 4);
            free(rgba);
        }
        serial = a->sheet_serial;
    }
    return reinterpret_cast<ImTextureID>(tex);
}

// the window's own photo, and the comparison's pictures (Before is the first of them)
static ImTextureID light_texture(SDL_Renderer *ren, const App *a)
{
    static SDL_Texture *tex = nullptr;
    static int serial = -1;
    return job_texture(ren, a->kind == APP_KIND_LIGHT, a->light.photo, tex, serial);
}

static void light_compare_textures(SDL_Renderer *ren, const App *a, ImTextureID out[LIGHT_COMPARE_SLOTS])
{
    static SDL_Texture *tex[LIGHT_COMPARE_SLOTS] = {};
    static int serial[LIGHT_COMPARE_SLOTS] = { -1, -1, -1, -1 };
    for (int i = 0; i < LIGHT_COMPARE_SLOTS; ++i) out[i] = job_texture(ren, a->kind == APP_KIND_LIGHT && i < light_compare_count(a->light.compare.mode), a->light.compare.job[i], tex[i], serial[i]);
}

// the open mask over its texture as a texture: made again when the mask or what is shown of it changed
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
    static bool track_laid_out = false, car_laid_out = false, masks_laid_out = false, light_laid_out = false;
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    {   // the Track light window's view is kept in the ini: a change marks it for saving
        static int seen_view = LIGHT_VIEW_DEFAULT, seen_hud = 0;
        if (light_photo_view_pref() != seen_view || light_photo_hud_pref() != seen_hud) {
            seen_view = light_photo_view_pref(), seen_hud = light_photo_hud_pref();
            ImGui::MarkIniSettingsDirty();
        }
    }
    if (a->kind != APP_KIND_NONE && ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_L, ImGuiInputFlags_RouteGlobal)) ui_reset_layout();
    if (a->kind != APP_KIND_NONE && ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_W, ImGuiInputFlags_RouteGlobal)) app_ask(a, APP_ACT_MAIN, "");   // Cmd+W: the main menu
    const bool reset = g_reset_in > 0 && --g_reset_in == 0 && a->kind != APP_KIND_NONE;   // Reset layout's countdown ends: this frame rebuilds the dock
    g_app = a;
    if (a->kind == APP_KIND_CAR) {   // the Preview's Night light and tint are the editor's: kept for the next car and the next run (the ini is written when marked)
        int light = 0;
        float tint[3] = { 0, 0, 0 };
        const NightSettings &n = a->night;
        if (!night_pref_get(&light, tint) || light != n.light || tint[0] != n.tint[0] || tint[1] != n.tint[1] || tint[2] != n.tint[2]) {
            night_pref_set(n.light, n.tint);
            ImGui::MarkIniSettingsDirty();
        }
    }
    main_menu(a);
    if (a->kind == APP_KIND_TRACK) {   // the track editor, as it was; its dock space keeps the layout of the first Studio
        const ImGuiID dock = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
        if (!track_laid_out || reset) {
            track_laid_out = true;
            if (reset || !g_had_layout || dock_is_empty(dock)) default_layout(dock);
        }
        app_sel_sync(a);   // the selected set agrees with the points and with `selected` before anything reads it
        if (a->confirm == APP_ACT_NONE) shortcuts(a), track_command_keys(a);   // the keys edit nothing while the question is up
        if (panel_open("Viewport")) viewport_panel(a);
        if (panel_open(PANEL_PROJECT)) project_panel(a);
        if (panel_open(PANEL_TRACK)) track_panel(a);
        if (panel_open(PANEL_POINTS)) points_panel(a);
        if (panel_open(PANEL_LIMITS)) limits_panel(a);
        if (panel_open(PANEL_PROBLEMS)) problems_panel(a);
        if (panel_open(PANEL_DRIVE)) drive_panel(a);
        if (panel_open(PANEL_STRIP)) strip_panel(a);
        if (panel_open(PANEL_NODE)) node_panel(a);
        if (panel_open(PANEL_ROAD_KEY)) road_key_window(a);
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
        if (panel_open(PANEL_CAR)) car_panel(a);
        if (panel_open(PANEL_MODEL)) model_panel(a, ren);
        if (panel_open(PANEL_MASKS)) masks_panel(a, mask_texture(ren, a));
        const ImTextureID photo = photo_texture(ren, a);
        if (panel_open(PANEL_RACE)) race_panel(a, photo, sheet_texture(ren, a));
        photo_view_panel(a, photo);
        if (panel_open(PANEL_PREVIEW)) preview_panel(a, ren);
    } else if (a->kind == APP_KIND_LIGHT) {   // the Track light window has a dock space of its own
        const ImGuiID dock = ImGui::DockSpaceOverViewport(ImHashStr("StudioLightDock"), ImGui::GetMainViewport());
        if (!light_laid_out || reset) {
            light_laid_out = true;
            if (reset || dock_is_empty(dock)) light_layout(dock);
        }
        if (a->original_dir[0] != '\0') {   // before any photo of this frame starts: they show the PNGs these two leave
            app_track_masks_load(a);   // the masks of an original track: scanned and made from the PSDs once per open
            app_track_masks_poll(a);   // and again when a PSD is saved
        }
        if (a->confirm == APP_ACT_NONE) light_shortcuts(a);
        if (panel_open(PANEL_LIGHT)) light_panel(a);
        if (panel_open(PANEL_LIGHT_PHOTO)) light_photo_panel(a, light_texture(ren, a));
        ImTextureID compare[LIGHT_COMPARE_SLOTS];
        light_compare_textures(ren, a, compare);
        if (panel_open(PANEL_LIGHT_COMPARE)) light_compare_panel(a, compare);
        if (a->original_dir[0] != '\0') {
            if (panel_open(PANEL_TRACK_MASKS)) track_masks_panel(a, track_mask_texture(ren, a));
        }
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
    new_track_popup(a);
    new_car_popup(a);
    export_track_popup(a);
    physics_popup(a);
    original_popup(a);
    copy_popup(a);
    open_error_popup(a);
    if (ImGui::GetCurrentContext()->DebugDrawIdConflictsId != 0) imgui_error(nullptr, nullptr, "two items share an ID (ImGui's red box): a row of a list needs its own PushID");   // only for the item under the mouse: the scan step moves it over the window
    ImGui::Render();
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), ren);
}
