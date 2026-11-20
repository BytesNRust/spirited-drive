// The Preview window (dep59 car editor E1): the textures of the open car as the race paints them (editor/car_preview.h), in the paint chosen from the card's own, made again
// when a mask is edited or saved, the paint list or the chosen colour changes, or Rehacer ends.
#include "panels.h"

extern "C" {
#include "car_preview.h"
}

#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

const char *const PANEL_PREVIEW = "Preview";

namespace {

struct Shown {
    CarPreview preview;
    SDL_Texture *tex[MASK_TEX_LIST] = {};
    bool have = false;
    char why[256] = "";
    char key_dir[APP_PATH] = "";
    unsigned key_generation = 0, key_version = 0;
    int key_paint = -2;
    uint8_t key_rgb[3] = { 0, 0, 0 };
    int key_count = -1;
};

Shown shown;

void drop(SDL_Texture **tex)
{
    for (int i = 0; i < MASK_TEX_LIST; ++i)
        if (tex[i] != nullptr) SDL_DestroyTexture(tex[i]), tex[i] = nullptr;
}

// the car's paints, from the card as it is on screen (a masks project and the game's traffic have none)
int paint_count(const App *a)
{
    const ExportCar *oc = car_race_original_of(&a->car);
    if (a->car.masks_only || (oc != nullptr && oc->kind != EXPORT_DRIVABLE)) return 0;
    return a->edit.now.paint_count;
}

void make(App *a, SDL_Renderer *ren, int paint)
{
    car_preview_free(&shown.preview);
    drop(shown.tex);
    shown.have = false;
    shown.why[0] = '\0';
    const ExportCar *oc = car_race_original_of(&a->car);
    char masks[APP_PATH];
    snprintf(masks, sizeof masks, "%s/car_masks/%s", a->car.piece, a->car.folder);
    PreviewIn in = {};
    in.cfm = a->car.cfm;
    in.stem = oc != nullptr ? oc->folder : a->car.folder;
    in.masks_dir = a->car.has_masks_dir ? masks : nullptr;
    char fallback[APP_PATH];   // the game's own masks of an original car, for a texture the project has none for
    snprintf(fallback, sizeof fallback, "%s/car_masks/%s", a->export_src.data, oc != nullptr ? oc->folder : "");
    in.fallback_dir = oc != nullptr && a->export_src.data[0] != '\0' ? fallback : nullptr;
    in.edit = a->mask.sel >= 0 && app_mask_modified(a) ? &a->mask.me : nullptr;
    in.paint = paint >= 0 && paint < paint_count(a) ? a->edit.now.paints[paint].rgb : nullptr;
    in.added = oc == nullptr;
    if (!car_preview_make(&in, &shown.preview, shown.why, sizeof shown.why)) return;
    for (int i = 0; i < shown.preview.count; ++i) {
        const PreviewTex &t = shown.preview.tex[i];
        shown.tex[i] = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, t.info.width, t.info.height);
        if (shown.tex[i] == nullptr) continue;
        SDL_SetTextureScaleMode(shown.tex[i], SDL_SCALEMODE_NEAREST);
        SDL_UpdateTexture(shown.tex[i], nullptr, t.rgba, t.info.width * 4);
    }
    shown.have = true;
}

}   // namespace

void preview_panel(App *a, SDL_Renderer *ren)
{
    int &paint = a->preview_paint;   // the card's paint shown; -1 the car as its file has it
    static char paint_dir[APP_PATH] = "";
    static int wait = 0;
    if (strcmp(paint_dir, a->dir) != 0) {
        snprintf(paint_dir, sizeof paint_dir, "%s", a->dir);
        shown.key_count = -1;
        wait = 3;
    }
    if (wait > 0 && --wait == 0 && a->mask.sel < 0) ImGui::SetNextWindowFocus();   // (a mask being edited has the front)   // a car just opened shows how the race paints it (after the dock layout has settled: a focus given in that frame is lost)
    if (ImGui::Begin(PANEL_PREVIEW)) {
        const int count = paint_count(a);
        if (paint >= count) paint = count - 1;
        char current[96] = "As the car file has it";
        if (paint >= 0) snprintf(current, sizeof current, "%s", a->edit.now.paints[paint].name);
        if (count > 0) {
            ImGui::SetNextItemWidth(220);
            if (ImGui::BeginCombo("Paint", current)) {
                if (ImGui::Selectable("As the car file has it", paint < 0)) paint = -1;
                for (int i = 0; i < count; ++i) {
                    const uint8_t *c = a->edit.now.paints[i].rgb;
                    ImGui::ColorButton("##c", ImVec4(c[0] / 255.0f, c[1] / 255.0f, c[2] / 255.0f, 1.0f), ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker, ImVec2(14, 14));
                    ImGui::SameLine();
                    if (ImGui::Selectable(a->edit.now.paints[i].name, paint == i)) paint = i;
                }
                ImGui::EndCombo();
            }
        } else {
            const ExportCar *oc = car_race_original_of(&a->car);
            ImGui::TextDisabled(a->car.masks_only || (oc != nullptr && oc->kind != EXPORT_DRIVABLE) ? "A traffic or police car: the race does not paint it, it shows its own colours with its masks' shine and black."
                                                                                                  : "The card has no paints: the car is shown as its file has it.");
        }
        // made again when what it is made from changed; not while a pencil stroke is going (every texel of it would make one) or a script is at work on the kit
        const uint8_t *rgb = paint >= 0 ? a->edit.now.paints[paint].rgb : nullptr;
        const bool busy = a->mask.me.stroke != 0 || app_rehacer_busy(a) || a->car.cfm[0] == '\0';
        const bool changed = shown.key_count != 1 || strcmp(shown.key_dir, a->dir) != 0 || shown.key_generation != a->mask.generation || shown.key_version != (a->mask.sel >= 0 ? a->mask.me.version : 0u)
                             || shown.key_paint != paint || (rgb != nullptr && memcmp(shown.key_rgb, rgb, 3) != 0);
        if (changed && !busy) {
            make(a, ren, paint);
            snprintf(shown.key_dir, sizeof shown.key_dir, "%s", a->dir);
            shown.key_generation = a->mask.generation;
            shown.key_version = a->mask.sel >= 0 ? a->mask.me.version : 0u;
            shown.key_paint = paint;
            if (rgb != nullptr) memcpy(shown.key_rgb, rgb, 3);
            shown.key_count = 1;
        }
        if (!shown.have) {
            if (shown.why[0] != '\0') ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "%s", shown.why);
        } else {
            const CarPreview &p = shown.preview;
            const bool over = p.body_colours > PREVIEW_OWN_ENTRIES;
            ImGui::PushStyleColor(ImGuiCol_Text, over ? ImVec4(0.95f, 0.78f, 0.35f, 1.0f) : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            ImGui::TextWrapped("%s", p.note);
            ImGui::PopStyleColor();
            if (p.special > 0)
                ImGui::TextDisabled("Hatched magenta: the police lights' screen entries (0xfa-0xfd). Hatched green: the car's own slot colour (0xfe). Chequer: 0xff, transparent in a polygon of mode 1.");
            ImGui::Separator();
            ImGui::BeginChild("textures", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
            const float width = ImGui::GetContentRegionAvail().x;
            float x = 0;
            for (int i = 0; i < p.count; ++i) {
                const PreviewTex &t = p.tex[i];
                const float scale = fminf(6.0f, fmaxf(1.0f, floorf(260.0f / (float)t.info.width)));
                const float w = fmaxf(t.info.width * scale, 120.0f);
                if (i > 0 && x + w + 12 < width) ImGui::SameLine();
                else x = 0;
                ImGui::BeginGroup();
                ImGui::Text("%s%s  %dx%d%s", t.info.name, t.info.level == 0 ? "" : " (low)", t.info.width, t.info.height, t.body ? "  body" : "");
                if (shown.tex[i] != nullptr) ImGui::Image(reinterpret_cast<ImTextureID>(shown.tex[i]), ImVec2(t.info.width * scale, t.info.height * scale));
                ImGui::EndGroup();
                x += w + 12;
            }
            ImGui::EndChild();
        }
    }
    ImGui::End();
}

void preview_free(void)
{
    car_preview_free(&shown.preview);
    drop(shown.tex);
    shown.have = false;
}
