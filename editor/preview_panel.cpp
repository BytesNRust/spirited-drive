// The Preview window (dep59 car editor E1): the textures of the open car as the race paints them (editor/car_preview.h), in the paint chosen from the card's own, made again
// when a mask is edited or saved, the paint list or the chosen colour changes, or Rehacer ends.
#include "panels.h"

extern "C" {
#include "car_preview.h"
#include "formats/car_night.h"
#include "formats/lamp_emissive.h"
#include "formats/police_flash.h"
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
    NightSettings key_night = {};   // the night view the textures were made for
    int key_phase = -1;             // and the police flash's phase
    bool uploaded = false;
    bool has_police = false;        // a texel names a police light entry (0xfa-0xfd)
    int lamp_texels[4] = {};        // the mask's lamp texels of each kind (LampKind), all textures
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

void make(App *a, int paint)
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
    shown.have = true;
    shown.uploaded = false;
}

// The textures on the screen: the Preview's own picture, or the car in the night view's state (formats/car_night.h) when the switches change it
void upload(SDL_Renderer *ren, const NightSettings &night, int phase)
{
    drop(shown.tex);
    shown.has_police = false;
    memset(shown.lamp_texels, 0, sizeof shown.lamp_texels);
    const bool plain = night_settings_plain(&night);
    const CarNightState state = night_settings_state(&night, phase * POLICE_FLASH_TICKS);
    for (int i = 0; i < shown.preview.count; ++i) {
        const PreviewTex &t = shown.preview.tex[i];
        const size_t count = (size_t)t.info.width * (size_t)t.info.height;
        if (t.index != nullptr)
            for (size_t k = 0; k < count; ++k)
                if (t.index[k] >= POLICE_LIGHT_FIRST && t.index[k] < POLICE_LIGHT_FIRST + POLICE_LIGHT_COUNT) { shown.has_police = true; break; }
        if (t.mask != nullptr) {
            LampTexels l = { nullptr, 0, 0 };
            if (lamp_texels_collect(&l, 0, t.mask, count) >= 0)
                for (int k = 0; k < l.count; ++k) shown.lamp_texels[l.texel[k].kind]++;
            lamp_texels_free(&l);
        }
        const uint8_t *rgba = t.rgba;
        uint8_t *made = nullptr;
        if (!plain && t.index != nullptr) {
            made = static_cast<uint8_t *>(malloc(count * 4));
            if (made != nullptr && car_night_texture(t.index, t.mask, t.info.width, t.info.height, shown.preview.palette[t.info.level == 0 ? 0 : 1], &state, made)) rgba = made;
        }
        shown.tex[i] = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, t.info.width, t.info.height);
        if (shown.tex[i] != nullptr) {
            SDL_SetTextureScaleMode(shown.tex[i], SDL_SCALEMODE_NEAREST);
            SDL_UpdateTexture(shown.tex[i], nullptr, rgba, t.info.width * 4);
        }
        free(made);
    }
    shown.key_night = night;
    shown.key_phase = phase;
    shown.uploaded = true;
}

// Day / Night, the track the night is of, and the lamps (core/formats/car_night.h): only the combinations the race can show are offered
void night_controls(App *a)
{
    NightSettings &n = a->night;
    ImGui::SeparatorText("View");
    if (ImGui::RadioButton("Day", !n.night)) n.night = 0;
    ImGui::SameLine();
    if (ImGui::RadioButton("Night", n.night != 0)) n.night = 1;
    ImGui::SameLine();
    ImGui::BeginDisabled(!n.night);
    ImGui::SetNextItemWidth(170);
    if (ImGui::BeginCombo("Night of", night_refs()[n.ref].label)) {
        for (int i = 0; i < night_ref_count(); ++i)
            if (ImGui::Selectable(night_refs()[i].label, n.ref == i)) n.ref = i;
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();
    ImGui::BeginDisabled(!n.night);
    ImGui::Checkbox("Headlights + side lights", reinterpret_cast<bool *>(&n.lights));
    ImGui::SameLine();
    ImGui::Checkbox("Brake", reinterpret_cast<bool *>(&n.brake));
    ImGui::EndDisabled();
    if (!n.night && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Lights only glow in low light: choose Night.");
    ImGui::SameLine();
    ImGui::Checkbox("Reverse", reinterpret_cast<bool *>(&n.reverse));
    if (shown.has_police) {
        ImGui::SameLine();
        ImGui::Checkbox("Police lights", reinterpret_cast<bool *>(&n.police));
    }
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    if (n.night)
        ImGui::TextWrapped("Night tone: the game's own night (the flat cool tone of its light effects) with the City and Lost Vegas lifts at their default, 0. The race also lights the body from the place (the road, the headlight pool): in a race it is lighter than here, see Race photo (night).");
    ImGui::TextWrapped("In the race the headlights and the side lights are on together at night. Glow on the road and halos show only in a race: use Race photo (night).");
    if (shown.have)
        ImGui::TextWrapped("Lamp texels in the masks: headlight %d, position %d, brake %d, reversing %d", shown.lamp_texels[LAMP_HEADLIGHT], shown.lamp_texels[LAMP_POSITION],
                            shown.lamp_texels[LAMP_BRAKE], shown.lamp_texels[LAMP_REVERSE]);
    ImGui::PopStyleColor();
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
        night_controls(a);
        // made again when what it is made from changed; not while a pencil stroke is going (every texel of it would make one) or a script is at work on the kit
        const uint8_t *rgb = paint >= 0 ? a->edit.now.paints[paint].rgb : nullptr;
        const bool busy = a->mask.me.stroke != 0 || app_rehacer_busy(a) || a->car.cfm[0] == '\0';
        const bool changed = shown.key_count != 1 || strcmp(shown.key_dir, a->dir) != 0 || shown.key_generation != a->mask.generation || shown.key_version != (a->mask.sel >= 0 ? a->mask.me.version : 0u)
                             || shown.key_paint != paint || (rgb != nullptr && memcmp(shown.key_rgb, rgb, 3) != 0);
        if (changed && !busy) {
            make(a, paint);
            snprintf(shown.key_dir, sizeof shown.key_dir, "%s", a->dir);
            shown.key_generation = a->mask.generation;
            shown.key_version = a->mask.sel >= 0 ? a->mask.me.version : 0u;
            shown.key_paint = paint;
            if (rgb != nullptr) memcpy(shown.key_rgb, rgb, 3);
            shown.key_count = 1;
        }
        const int phase = a->night.police ? static_cast<int>((SDL_GetTicks() / 500u) & 1u) : 0;   // the flash: a phase every 15 ticks of 30 a second
        if (shown.have && (!shown.uploaded || memcmp(&shown.key_night, &a->night, sizeof a->night) != 0 || shown.key_phase != phase)) upload(ren, a->night, phase);
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
                if (a->preview_focus[0] != '\0' && t.info.level == 0 && memcmp(t.info.name, a->preview_focus, 4) == 0) ImGui::SetScrollHereY(0.0f);
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
