// The Model panel (dep59 car editor, step 2): the open car in 3D as the game draws it. The classic rasterizer of the core (raster/car_scene.h) draws the high
// model into an 8-bit surface, turned and placed by an orbit camera (raster/orbit.h), and the surface goes to the window as a texture. Drag with the left button
// to turn the car (up to 90 degrees above, 10 below the road), the wheel zooms, F / S / R / T are the front, side, rear and top, Home the first view; Auto-rotate
// turns it by itself. Unpainted: the bank's own colours (the painted car comes with the preview's paint, car_preview).
#include "panels.h"

#include "imgui.h"

#include <SDL3/SDL.h>

#include "model_scene.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *const PANEL_MODEL = "Model";

namespace {

enum { MAX_W = 2048, AUTO_DEG_PER_SECOND = 24 };

struct Model {
    char loaded[APP_PATH] = "";   // the CFM the scene holds (or tried to)
    ModelScene *scene = nullptr;
    char why[200] = "";
    Orbit orbit = {};
    bool auto_rotate = false, dirty = true;
    int preset = 0;               // an orbit asked for on the command line, waiting for the car
    double preset_yaw = 0, preset_pitch = 0;
    int32_t preset_dist = 0;
    SDL_Texture *tex = nullptr;
    int tw = 0, th = 0;
    uint8_t *rgba = nullptr;
    size_t capacity = 0;
};

Model g;

void close_scene()
{
    model_scene_close(g.scene);
    g.scene = nullptr;
}

// the car file read and the scene opened on it; the camera starts at home (or at the command line's orbit)
void load(const char *cfm)
{
    close_scene();
    snprintf(g.loaded, sizeof g.loaded, "%s", cfm);
    g.why[0] = 0;
    g.scene = model_scene_open(cfm, g.why, sizeof g.why);
    if (g.scene == nullptr) return;
    orbit_init(&g.orbit, model_scene_radius(g.scene));
    if (g.preset) {
        g.orbit.yaw = ((static_cast<int32_t>(g.preset_yaw * 16.0) % ORBIT_TURN) + ORBIT_TURN) % ORBIT_TURN;
        g.orbit.pitch = static_cast<int32_t>(g.preset_pitch * 16.0);
        if (g.orbit.pitch < ORBIT_PITCH_MIN) g.orbit.pitch = ORBIT_PITCH_MIN;
        if (g.orbit.pitch > ORBIT_PITCH_MAX) g.orbit.pitch = ORBIT_PITCH_MAX;
        if (g.preset_dist > 0) g.orbit.dist = g.preset_dist;
        g.preset = 0;
    }
    g.dirty = true;
}

// the surface and the texture, for a picture of w x h
bool make_target(SDL_Renderer *ren, int w, int h)
{
    if (g.tex != nullptr && g.tw == w && g.th == h) return true;
    if (g.tex != nullptr) SDL_DestroyTexture(g.tex), g.tex = nullptr;
    const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
    if (n > g.capacity) {
        free(g.rgba);
        g.rgba = static_cast<uint8_t *>(malloc(4 * n));
        g.capacity = g.rgba != nullptr ? n : 0;
        if (g.capacity == 0) return false;
    }
    g.tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, w, h);
    if (g.tex == nullptr) return false;
    SDL_SetTextureBlendMode(g.tex, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(g.tex, SDL_SCALEMODE_NEAREST);
    g.tw = w, g.th = h;
    g.dirty = true;
    return true;
}

void draw_picture(int w, int h)
{
    if (model_scene_render(g.scene, &g.orbit, w, h, g.rgba)) SDL_UpdateTexture(g.tex, nullptr, g.rgba, w * 4);
    g.dirty = false;
}

void keys()
{
    const ImGuiIO &io = ImGui::GetIO();
    if (io.WantTextInput || io.KeyCtrl || io.KeyAlt || io.KeySuper) return;
    if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows)) return;
    if (ImGui::IsKeyPressed(ImGuiKey_F, false)) orbit_view(&g.orbit, ORBIT_FRONT), g.dirty = true;
    if (ImGui::IsKeyPressed(ImGuiKey_S, false)) orbit_view(&g.orbit, ORBIT_SIDE), g.dirty = true;
    if (ImGui::IsKeyPressed(ImGuiKey_R, false)) orbit_view(&g.orbit, ORBIT_REAR), g.dirty = true;
    if (ImGui::IsKeyPressed(ImGuiKey_T, false)) orbit_view(&g.orbit, ORBIT_TOP), g.dirty = true;
    if (ImGui::IsKeyPressed(ImGuiKey_Home, false)) orbit_home(&g.orbit), g.dirty = true;
}

void toolbar()
{
    if (ImGui::Button("Front")) orbit_view(&g.orbit, ORBIT_FRONT), g.dirty = true;
    ImGui::SameLine();
    if (ImGui::Button("Side")) orbit_view(&g.orbit, ORBIT_SIDE), g.dirty = true;
    ImGui::SameLine();
    if (ImGui::Button("Rear")) orbit_view(&g.orbit, ORBIT_REAR), g.dirty = true;
    ImGui::SameLine();
    if (ImGui::Button("Top")) orbit_view(&g.orbit, ORBIT_TOP), g.dirty = true;
    ImGui::SameLine();
    if (ImGui::Button("Home")) orbit_home(&g.orbit), g.dirty = true;
    ImGui::SameLine();
    if (ImGui::Checkbox("Auto-rotate", &g.auto_rotate)) g.dirty = true;
    ImGui::SameLine();
    if (ImGui::Button("Reload")) g.loaded[0] = 0;   // the file again: after Rehacer or a new bank
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Read the car file again");
}

void turn_and_zoom(const ImVec2 size)
{
    ImGuiIO &io = ImGui::GetIO();
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f) && (io.MouseDelta.x != 0 || io.MouseDelta.y != 0)) {
        orbit_drag(&g.orbit, -static_cast<int>(roundf(io.MouseDelta.x)), static_cast<int>(roundf(io.MouseDelta.y)));   // the car follows the pointer
        g.dirty = true;
    }
    if (ImGui::IsItemHovered() && io.MouseWheel != 0) orbit_zoom(&g.orbit, io.MouseWheel > 0 ? 1 : -1), g.dirty = true;
    (void)size;
}

}   // namespace

void model_panel_preset(double yaw, double pitch, int dist)
{
    g.preset = 1, g.preset_yaw = yaw, g.preset_pitch = pitch, g.preset_dist = dist;
}

void model_panel(App *a, SDL_Renderer *ren)
{
    if (ImGui::Begin(PANEL_MODEL)) {
        if (strcmp(g.loaded, a->car.cfm) != 0) load(a->car.cfm);
        if (g.scene == nullptr) {
            ImGui::TextWrapped("%s", g.why[0] != 0 ? g.why : "No car is open.");
        } else {
            toolbar();
            keys();
            ImVec2 size = ImGui::GetContentRegionAvail();
            int w = static_cast<int>(size.x), h = static_cast<int>(size.y) - static_cast<int>(ImGui::GetTextLineHeightWithSpacing());
            if (w > MAX_W) w = MAX_W;
            const int shown_w = w;   // the area; the picture is a square in its middle, so the car fits whatever the shape of the panel
            if (h < w) w = h;
            if (h > model_scene_max_rows()) h = model_scene_max_rows();
            if (w >= 64 && h >= 64 && make_target(ren, w, h)) {
                if (g.auto_rotate) {
                    const int step = static_cast<int>(roundf(ImGui::GetIO().DeltaTime * AUTO_DEG_PER_SECOND * 16.0f));
                    orbit_drag(&g.orbit, -step / ORBIT_DRAG - (step % ORBIT_DRAG != 0), 0);   // at least a step a frame: the yaw moves by 1/16 degrees
                    g.dirty = true;
                }
                if (g.dirty) draw_picture(w, h);
                ImVec2 at = ImGui::GetCursorScreenPos();
                ImGui::InvisibleButton("##orbit", ImVec2(static_cast<float>(shown_w), static_cast<float>(h)));
                at.x += static_cast<float>((shown_w - w) / 2);
                turn_and_zoom(ImVec2(static_cast<float>(w), static_cast<float>(h)));
                ImGui::GetWindowDrawList()->AddImage(reinterpret_cast<ImTextureID>(g.tex), at, ImVec2(at.x + static_cast<float>(w), at.y + static_cast<float>(h)));
                ImGui::TextDisabled("Drag to turn, wheel to zoom, F S R T Home. Unpainted: the colors of the car's bank.");
            }
        }
    }
    ImGui::End();
}
