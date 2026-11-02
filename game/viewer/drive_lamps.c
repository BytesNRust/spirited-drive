#include "game/viewer/drive_lamps.h"

#include "game/sys/port_log.h"
#include "game/sys/port_settings.h"
#include "game/viewer/drive_livery.h"

#include <string.h>

typedef struct {
    LampTexels texels;
    uint8_t *file;
    size_t size;
    int armed;
    LampState state;
} Slot;

static Slot slots[DRIVE_LAMP_SLOTS];
static LampEntries entries;

static int valid(int slot) { return slot >= 0 && slot < DRIVE_LAMP_SLOTS; }

void drive_lamps_clear(int slot)
{
    if (!valid(slot)) return;
    lamp_texels_free(&slots[slot].texels);
    slots[slot] = (Slot){ 0 };
}

int drive_lamps_find(int slot, const char *stem, uint8_t *file, size_t size)
{
    if (!valid(slot) || file == NULL || !port_emissive_lamps()) return 0;
    Slot *s = &slots[slot];
    s->file = file, s->size = size;
    const int n = drive_livery_lamps(stem, file, size, &s->texels);
    if (n > 0) port_log("emissive_lamps: slot %d %s: %d lamp texels\n", slot, stem, n);
    return n;
}

int drive_lamps_has(int slot, int kind)
{
    if (!valid(slot)) return 0;
    for (int i = 0; i < slots[slot].texels.count; i++)
        if (slots[slot].texels.texel[i].kind == kind) return 1;
    return 0;
}

int drive_lamps_count(int slot) { return valid(slot) ? slots[slot].texels.count : 0; }

int drive_lamps_reached(int slot, const Object3D *obj)
{
    if (!valid(slot) || obj == NULL || slots[slot].file == NULL) return 0;
    const Slot *s = &slots[slot];
    int reached = 0;
    for (int i = 0; i < s->texels.count; i++) {
        const uint32_t at = s->texels.texel[i].offset;
        for (uint32_t k = 0; k < obj->texture_count; k++) {
            const FshEntry *t = obj->textures[k].texture;
            if (t == NULL || t->payload < s->file || t->payload >= s->file + s->size) continue;
            const size_t from = (size_t)(t->payload - s->file), count = (size_t)t->width * t->height;
            if (at >= from && at < from + count) {
                reached++;
                break;
            }
        }
    }
    return reached;
}

void drive_lamps_arm(void)
{
    for (int i = 0; i < DRIVE_LAMP_SLOTS; i++) {
        Slot *s = &slots[i];
        if (s->texels.count == 0 || s->file == NULL) continue;
        lamp_texels_read(&s->texels, s->file, s->size);
        s->armed = 1, s->state = LAMP_DAY;
    }
}

void drive_lamps_set_entries(uint8_t brake_lit, uint8_t position, uint8_t headlight, uint8_t reverse)
{
    entries = (LampEntries){ brake_lit, position, headlight, reverse };
}

void drive_lamps_frame(int slot, int low, int braking, int reversing)
{
    if (!valid(slot) || !slots[slot].armed) return;
    Slot *s = &slots[slot];
    const LampState state = (LampState)((!low ? LAMP_DAY : braking ? LAMP_BRAKING : LAMP_LOW) | (reversing ? LAMP_REVERSING : 0));
    if (state == s->state) return;
    lamp_texels_write(&s->texels, s->file, s->size, state, &entries);
    s->state = state;
}
