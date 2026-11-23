#include "ui_probe.h"

#include "imgui.h"

#include <stdio.h>
#include <string.h>

namespace {

struct Entry { char name[48]; float x, y, w, h; int frame; };
Entry g_table[160];
int g_count = 0;
char g_scope[32] = "";

}   // namespace

void ui_probe_scope(const char *scope)
{
    snprintf(g_scope, sizeof g_scope, "%s", scope);
}

void ui_probe(const char *name)
{
    char full[48];
    if (g_scope[0] != '\0') snprintf(full, sizeof full, "%s_%s", g_scope, name);
    else snprintf(full, sizeof full, "%s", name);
    const ImVec2 lo = ImGui::GetItemRectMin(), hi = ImGui::GetItemRectMax();
    const int frame = ImGui::GetFrameCount();
    Entry *e = nullptr;
    for (int i = 0; i < g_count && e == nullptr; ++i)
        if (strcmp(g_table[i].name, full) == 0) e = &g_table[i];
    for (int i = 0; i < g_count && e == nullptr; ++i)
        if (g_table[i].frame < frame - 1) e = &g_table[i];   // a name that has gone
    if (e == nullptr && g_count < static_cast<int>(sizeof g_table / sizeof g_table[0])) e = &g_table[g_count++];
    if (e == nullptr) return;
    snprintf(e->name, sizeof e->name, "%s", full);
    e->x = lo.x, e->y = lo.y, e->w = hi.x - lo.x, e->h = hi.y - lo.y, e->frame = frame;
}

extern "C" int ui_probe_find(const char *name, float *x, float *y, float *w, float *h)
{
    const int frame = ImGui::GetFrameCount();
    for (int i = 0; i < g_count; ++i) {
        if (strcmp(g_table[i].name, name) != 0 || g_table[i].frame < frame - 1) continue;
        *x = g_table[i].x, *y = g_table[i].y, *w = g_table[i].w, *h = g_table[i].h;
        return 1;
    }
    return 0;
}
