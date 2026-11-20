// The editor's interface layer (ui.h): Dear ImGui with docking over the SDL renderer: the dock space, the shortcuts and the panels (panels.cpp);
// the top view is viewport.cpp's.
#include "ui.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"

extern "C" {
#include "formats/track_gen.h"
#include "interact.h"
}
#include "panels.h"
#include "shortcut_keys.h"
#include "viewport.h"

#include <stdio.h>

static char g_ini[APP_PATH];
static bool g_had_layout;   // the ini file was there at the start: its layout is the user's, not ours to replace

int ui_open(SDL_Window *win, SDL_Renderer *ren, const char *ini, char *why, size_t why_cap)
{
    IMGUI_CHECKVERSION();
    if (ImGui::CreateContext() == nullptr) {
        snprintf(why, why_cap, "ImGui: no context");
        return 0;
    }
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;
    if (ini != nullptr) {
        snprintf(g_ini, sizeof g_ini, "%s", ini);
        io.IniFilename = g_ini;
        FILE *f = fopen(g_ini, "rb");
        g_had_layout = f != nullptr;
        if (f != nullptr) fclose(f);
    } else {
        io.IniFilename = nullptr;
    }
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

void ui_close(void)
{
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
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

// the layout of a first run (no ini): the top view in the middle, the limits and the problems (two tabs) under it, the project and the points to its right
void default_layout(ImGuiID dock)
{
    ImGui::DockBuilderRemoveNode(dock);
    ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dock, ImGui::GetMainViewport()->Size);
    ImGuiID right = 0, centre = dock, bottom = 0, points = 0, node = 0;
    right = ImGui::DockBuilderSplitNode(centre, ImGuiDir_Right, 0.33f, nullptr, &centre);
    bottom = ImGui::DockBuilderSplitNode(centre, ImGuiDir_Down, 0.30f, nullptr, &centre);
    points = ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, 0.55f, nullptr, &right);
    ImGui::DockBuilderDockWindow("Viewport", centre);
    ImGui::DockBuilderDockWindow(PANEL_LIMITS, bottom);
    ImGui::DockBuilderDockWindow(PANEL_PROBLEMS, bottom);   // its own tab beside Limits
    const ImGuiID track = ImGui::DockBuilderSplitNode(right, ImGuiDir_Up, 0.34f, nullptr, &right);   // the road's width above the project
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
    if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) app_remove_point(a);
    else if (ImGui::IsKeyPressed(ImGuiKey_X, false)) app_remove_point(a);
    if (ImGui::IsKeyPressed(ImGuiKey_S, false)) app_save(a);
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
        shortcut_label(save_keys, sizeof save_keys, SHORTCUT_MAC, "S");   // the car's Save is Cmd+S on a Mac, Ctrl+S elsewhere; a track's is S alone
        if (ImGui::MenuItem("Save", a->kind == APP_KIND_CAR ? save_keys : "S", false, a->kind != APP_KIND_NONE && !app_rehacer_busy(a))) app_save(a);   // as the keys: not while Rehacer works on the kit
        if (ImGui::MenuItem("Quit")) app_ask(a, APP_ACT_QUIT, "");
        ImGui::EndMenu();
    }
    if (a->kind == APP_KIND_CAR && ImGui::BeginMenu("Edit")) {
        char undo_keys[16], redo_keys[24];
        shortcut_label(undo_keys, sizeof undo_keys, SHORTCUT_MAC, "Z");
        shortcut_label(redo_keys, sizeof redo_keys, SHORTCUT_MAC, "Shift+Z");
        if (ImGui::MenuItem("Undo", undo_keys, false, app_can_undo(a) && !app_rehacer_busy(a))) app_undo(a);
        if (ImGui::MenuItem("Redo", redo_keys, false, app_can_redo(a) && !app_rehacer_busy(a))) app_redo(a);
        ImGui::EndMenu();
    }
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
        tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, RACE_PHOTO_W, RACE_PHOTO_H);
        if (tex != nullptr) SDL_UpdateTexture(tex, nullptr, a->race.photo, RACE_PHOTO_W * 4);
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
    main_menu(a);
    if (a->kind == APP_KIND_TRACK) {   // the track editor, as it was; its dock space keeps the layout of the first Studio
        const ImGuiID dock = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
        if (!track_laid_out) {
            track_laid_out = true;
            if (!g_had_layout || dock_is_empty(dock)) default_layout(dock);
        }
        if (a->confirm == APP_ACT_NONE) shortcuts(a);   // the keys edit nothing while the question is up
        viewport_panel(a);
        project_panel(a);
        track_panel(a);
        points_panel(a);
        limits_panel(a);
        problems_panel(a);
        node_panel(a);
    } else if (a->kind == APP_KIND_CAR) {   // a car has a dock space of its own: each kind keeps its layout
        const bool masks = a->car.masks_only;   // a masks project keeps a layout of its own: it shows fewer windows
        const ImGuiID dock = ImGui::DockSpaceOverViewport(ImHashStr(masks ? "StudioMasksDock" : "StudioCarDock"), ImGui::GetMainViewport());
        bool &laid_out = masks ? masks_laid_out : car_laid_out;
        if (!laid_out) {
            laid_out = true;
            if (dock_is_empty(dock)) (masks ? masks_layout : car_layout)(dock);
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
    export_popup(a);
    confirm_popup(a);
    open_error_popup(a);
    ImGui::Render();
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), ren);
}
